"""Prints a password hash for DASHBOARD_PASSWORD_HASH in .env."""
from getpass import getpass

from werkzeug.security import generate_password_hash

password = getpass("Dashboard password: ")
if len(password) < 12:
    raise SystemExit("Please use at least 12 characters.")
if password != getpass("Repeat: "):
    raise SystemExit("Passwords do not match.")
print(generate_password_hash(password))
