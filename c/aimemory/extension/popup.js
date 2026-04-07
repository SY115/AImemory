const API = 'http://127.0.0.1:7890';

async function checkHealth() {
    try {
        const r = await fetch(API + '/health');
        const d = await r.json();
        document.getElementById('statusDot').className = 'dot on';
        document.getElementById('statusText').textContent = 'Connected to engine';
        loadStats();
    } catch (e) {
        document.getElementById('statusDot').className = 'dot off';
        document.getElementById('statusText').textContent = 'Engine not running';
        document.getElementById('statsBox').innerHTML = '<p style="font-size:11px;color:#888;padding:4px;">Start engine: aimemory-server.exe</p>';
    }
}

async function loadStats() {
    try {
        const r = await fetch(API + '/stats');
        const d = await r.json();
        let html = '<table>';
        for (const [layer, count] of Object.entries(d.layers)) {
            html += '<tr><td>' + layer + '</td><td>' + count + '</td></tr>';
        }
        html += '<tr><td><strong>Total</strong></td><td><strong>' + d.total + '</strong></td></tr>';
        html += '</table>';
        document.getElementById('statsBox').innerHTML = html;
    } catch (e) {}
}

async function search(q) {
    if (!q || q.length < 2) {
        document.getElementById('resultsBox').innerHTML = '';
        return;
    }
    try {
        const r = await fetch(API + '/search?q=' + encodeURIComponent(q));
        const d = await r.json();
        let html = '';
        if (d.count === 0) {
            html = '<p style="font-size:11px;color:#888;">No results</p>';
        } else {
            d.results.forEach(function(r) {
                const val = r.value.length > 100 ? r.value.substring(0, 97) + '...' : r.value;
                html += '<div class="result">' +
                    '<span class="key">' + r.key + '</span> ' +
                    '<span class="layer">[' + r.layer + '] score:' + r.score + '</span>' +
                    '<div class="val">' + val + '</div>' +
                    '</div>';
            });
        }
        document.getElementById('resultsBox').innerHTML = html;
    } catch (e) {
        document.getElementById('resultsBox').innerHTML = '<p style="font-size:11px;color:#f44;">Search failed</p>';
    }
}

document.getElementById('searchInput').addEventListener('input', function(e) {
    search(e.target.value);
});

checkHealth();
