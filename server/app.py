"""AutoCargo server: relays messages between ESP32 nodes and offers a web dashboard.

Message protocol (sent by the ESP32 nodes as text/plain to POST /receive):

    recipient&sender&password&message

All configuration and secrets come from environment variables (see .env.example).
"""

import datetime
import hmac
import ipaddress
import os
import re
import threading
import time
from functools import wraps

import mysql.connector
import mysql.connector.pooling
import requests
from dotenv import load_dotenv
from flask import (Flask, abort, flash, g, redirect, render_template, request,
                   session, url_for)
from flask_wtf.csrf import CSRFProtect
from werkzeug.middleware.proxy_fix import ProxyFix
from werkzeug.security import check_password_hash

load_dotenv()


def require_env(name):
    value = os.environ.get(name)
    if not value:
        raise RuntimeError(f"Environment variable {name} is not set (see .env.example)")
    return value


# --- Configuration -----------------------------------------------------------

DASHBOARD_PASSWORD_HASH = require_env("DASHBOARD_PASSWORD_HASH")
DEVICE_SHARED_SECRET = require_env("DEVICE_SHARED_SECRET")
CLEAR_MESSAGES_ON_START = os.environ.get("CLEAR_MESSAGES_ON_START", "true").lower() == "true"
FORWARD_TIMEOUT = 3  # seconds

NAME_PATTERN = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
MAX_MESSAGE_LENGTH = 500

app = Flask(__name__)
app.config.update(
    SECRET_KEY=require_env("FLASK_SECRET_KEY"),
    SESSION_COOKIE_HTTPONLY=True,
    SESSION_COOKIE_SAMESITE="Lax",
    # The dashboard is meant to be served over HTTPS (see deploy/nginx.conf).
    SESSION_COOKIE_SECURE=os.environ.get("SESSION_COOKIE_SECURE", "true").lower() == "true",
    PERMANENT_SESSION_LIFETIME=datetime.timedelta(hours=8),
    MAX_CONTENT_LENGTH=4 * 1024,
)
# Trust X-Forwarded-For / X-Forwarded-Proto from exactly one reverse proxy (nginx).
app.wsgi_app = ProxyFix(app.wsgi_app, x_for=1, x_proto=1)
csrf = CSRFProtect(app)

pool = mysql.connector.pooling.MySQLConnectionPool(
    pool_name="autocargo",
    pool_size=5,
    host=os.environ.get("DB_HOST", "localhost"),
    user=require_env("DB_USER"),
    password=require_env("DB_PASSWORD"),
    database=os.environ.get("DB_NAME", "autocargo"),
)


# --- Helpers -------------------------------------------------------------------

def db():
    """One pooled connection per request."""
    if "db" not in g:
        g.db = pool.get_connection()
    return g.db


@app.teardown_appcontext
def close_db(_exc):
    conn = g.pop("db", None)
    if conn is not None:
        conn.close()


def query(sql, params=(), fetch=False):
    conn = db()
    cursor = conn.cursor()
    try:
        cursor.execute(sql, params)
        if fetch:
            return cursor.fetchall()
        conn.commit()
        return None
    finally:
        cursor.close()


def login_required(view):
    @wraps(view)
    def wrapped(*args, **kwargs):
        if not session.get("logged_in"):
            return redirect(url_for("home"))
        return view(*args, **kwargs)
    return wrapped


class RateLimiter:
    """Small in-memory sliding-window limiter (the server runs as one process)."""

    def __init__(self, limit, window_seconds):
        self.limit = limit
        self.window = window_seconds
        self.hits = {}
        self.lock = threading.Lock()

    def allow(self, key):
        now = time.monotonic()
        with self.lock:
            recent = [t for t in self.hits.get(key, []) if now - t < self.window]
            allowed = len(recent) < self.limit
            if allowed:
                recent.append(now)
            self.hits[key] = recent
            return allowed


login_limiter = RateLimiter(limit=5, window_seconds=15 * 60)
receive_limiter = RateLimiter(limit=60, window_seconds=60)


def valid_name(name):
    return bool(name) and NAME_PATTERN.fullmatch(name) is not None


def valid_device_ip(ip):
    """Only private LAN addresses: the server must not forward to the internet or itself."""
    try:
        address = ipaddress.ip_address(ip)
    except ValueError:
        return False
    return address.is_private and not (address.is_loopback or address.is_link_local
                                       or address.is_unspecified or address.is_multicast)


def valid_message(message):
    return bool(message) and len(message) <= MAX_MESSAGE_LENGTH and "\x00" not in message


def store_message(sender, recipient, message):
    query(
        "INSERT INTO messages (sender, message, timestamp, recipient) VALUES (%s, %s, %s, %s)",
        (sender, message, datetime.datetime.now(), recipient),
    )


def forward_to_device(recipient, sender, message):
    """Send a message to the recipient ESP32. Returns False if it is unknown or unreachable.

    The ESP32 firmware expects four '&'-separated fields and only uses the last one.
    The shared secret is not forwarded: the password field is sent as '-'.
    """
    rows = query("SELECT ip FROM devices WHERE name = %s", (recipient,), fetch=True)
    if not rows:
        return False
    ip = rows[0][0]
    if not valid_device_ip(ip):
        return False
    try:
        requests.post(
            f"http://{ip}/receive",
            data=f"{recipient}&{sender}&-&{message}".encode("utf-8"),
            headers={"Content-Type": "text/plain"},
            timeout=FORWARD_TIMEOUT,
            allow_redirects=False,
        )
        return True
    except requests.RequestException as exc:
        app.logger.warning("Forwarding to %s failed: %s", recipient, exc)
        return False


@app.after_request
def security_headers(response):
    response.headers["Content-Security-Policy"] = (
        "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self'; "
        "frame-ancestors 'none'; form-action 'self'; base-uri 'none'"
    )
    response.headers["X-Content-Type-Options"] = "nosniff"
    response.headers["X-Frame-Options"] = "DENY"
    response.headers["Referrer-Policy"] = "no-referrer"
    return response


# --- Dashboard -----------------------------------------------------------------

@app.route("/", methods=["GET", "POST"])
def home():
    if session.get("logged_in"):
        return redirect(url_for("dashboard"))
    error = None
    if request.method == "POST":
        if not login_limiter.allow(request.remote_addr):
            error = "Zu viele Versuche. Bitte später erneut versuchen."
        elif check_password_hash(DASHBOARD_PASSWORD_HASH, request.form.get("password", "")):
            session.clear()  # new session after login
            session.permanent = True
            session["logged_in"] = True
            return redirect(url_for("dashboard"))
        else:
            error = "Zugriff verweigert"
    return render_template("login.html", error=error)


@app.route("/dashboard")
@login_required
def dashboard():
    devices = query("SELECT id, name, ip FROM devices ORDER BY name", fetch=True)
    messages = query(
        "SELECT sender, message, timestamp, recipient FROM messages ORDER BY timestamp DESC LIMIT 200",
        fetch=True,
    )
    return render_template("index.html", devices=devices, messages=messages)


@app.route("/messages")
@login_required
def messages_partial():
    messages = query(
        "SELECT sender, message, timestamp, recipient FROM messages ORDER BY timestamp DESC LIMIT 200",
        fetch=True,
    )
    return render_template("messages_table.html", messages=messages)


@app.route("/add_device", methods=["POST"])
@login_required
def add_device():
    name = request.form.get("name", "").strip()
    ip = request.form.get("ip", "").strip()
    if not valid_name(name):
        flash("Ungültiger Gerätename (1–32 Zeichen: Buchstaben, Ziffern, _ und -).", "error")
    elif not valid_device_ip(ip):
        flash("Ungültige IP-Adresse (nur private Adressen im lokalen Netz).", "error")
    elif query("SELECT 1 FROM devices WHERE LOWER(name) = LOWER(%s)", (name,), fetch=True):
        flash("Gerätename existiert bereits (Groß-/Kleinschreibung wird ignoriert)!", "error")
    else:
        query("INSERT INTO devices (name, ip) VALUES (%s, %s)", (name, ip))
        flash("Gerät erfolgreich hinzugefügt.", "success")
    return redirect(url_for("dashboard"))


@app.route("/delete_device/<int:device_id>", methods=["POST"])
@login_required
def delete_device(device_id):
    query("DELETE FROM devices WHERE id = %s", (device_id,))
    return redirect(url_for("dashboard"))


@app.route("/send_message", methods=["POST"])
@login_required
def send_message():
    recipient = request.form.get("recipient", "").strip()
    message = request.form.get("message", "").strip()
    if not valid_name(recipient) or not valid_message(message):
        flash(f"Ungültiger Empfänger oder Nachricht (max. {MAX_MESSAGE_LENGTH} Zeichen).", "error")
    elif not query("SELECT 1 FROM devices WHERE name = %s", (recipient,), fetch=True):
        flash(f"Gerät '{recipient}' nicht gefunden.", "error")
    elif forward_to_device(recipient, "WebDashboard", message):
        store_message("WebDashboard", recipient, message)
        flash(f"Nachricht erfolgreich an {recipient} gesendet.", "success")
    else:
        flash(f"Gerät '{recipient}' ist nicht erreichbar!", "error")
    return redirect(url_for("dashboard"))


@app.route("/logout", methods=["POST"])
def logout():
    session.clear()
    return redirect(url_for("home"))


# --- Device endpoint -----------------------------------------------------------

@app.route("/receive", methods=["POST"])
@csrf.exempt  # machine-to-machine endpoint, authenticated by the shared secret
def receive():
    if not receive_limiter.allow(request.remote_addr):
        abort(429)
    data = request.get_data(as_text=True).strip()
    parts = data.split("&", 3)  # the message itself may contain '&'
    if len(parts) != 4:
        return "Invalid format", 400
    recipient, sender, password, message = parts

    if not hmac.compare_digest(password.encode("utf-8"), DEVICE_SHARED_SECRET.encode("utf-8")):
        return "Unauthorized", 403
    if not (valid_name(recipient) and valid_name(sender) and valid_message(message)):
        return "Invalid format", 400

    store_message(sender, recipient, message)
    forward_to_device(recipient, sender, message)
    return "OK", 200


# --- Start-up ------------------------------------------------------------------

if CLEAR_MESSAGES_ON_START:
    # Messages are only kept until the next restart; devices stay.
    with app.app_context():
        query("DELETE FROM messages")


if __name__ == "__main__":
    # Development only. In production run it behind nginx (see README).
    app.run(host="127.0.0.1", port=8000)
