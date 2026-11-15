let autoUpdate = true;
let interval = setInterval(fetchMessages, 1000);

function fetchMessages() {
    if (!autoUpdate) return;
    fetch('/messages', { credentials: 'same-origin' })
        .then(res => {
            if (res.redirected || !res.ok) { window.location = '/'; return null; }
            return res.text();
        })
        .then(html => {
            if (html === null) return;
            document.getElementById('messages-table').innerHTML = html;
        });
}

document.getElementById('toggle-autoupdate').onclick = function() {
    autoUpdate = !autoUpdate;
    this.textContent = autoUpdate ? "Auto-Update pausieren" : "Auto-Update fortsetzen";
};
