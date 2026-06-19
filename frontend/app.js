let socket;
let sensorChart;
const maxDataPoints = 20;

const serverIP = window.location.hostname || "localhost";
const wsUrl = `ws://${serverIP}:8000/ws/ui`;
const apiUrl = `http://${serverIP}:8000`;

// Theme Management
function initTheme() {
    const savedTheme = localStorage.getItem('theme') || 'dark';
    document.documentElement.setAttribute('data-theme', savedTheme);
    updateThemeIcon(savedTheme);
}

function toggleTheme() {
    const currentTheme = document.documentElement.getAttribute('data-theme');
    const newTheme = currentTheme === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', newTheme);
    localStorage.setItem('theme', newTheme);
    updateThemeIcon(newTheme);
}

function updateThemeIcon(theme) {
    const icon = document.getElementById('themeIcon');
    if (theme === 'dark') {
        icon.setAttribute('data-lucide', 'sun');
    } else {
        icon.setAttribute('data-lucide', 'moon');
    }
    lucide.createIcons();
}

document.getElementById('themeToggle').onclick = toggleTheme;

// Initialize Lucide
lucide.createIcons();
initTheme();
const chartData = {
    labels: [],
    datasets: [
        {
            label: 'Temperature (°C)',
            data: [],
            borderColor: '#6366f1',
            backgroundColor: 'rgba(99, 102, 241, 0.2)',
            fill: true,
            tension: 0.4
        },
        {
            label: 'Humidity (%)',
            data: [],
            borderColor: '#a855f7',
            backgroundColor: 'rgba(168, 85, 247, 0.2)',
            fill: true,
            tension: 0.4
        }
    ]
};

// Initialize Chart
function initChart() {
    const ctx = document.getElementById('sensorChart').getContext('2d');
    sensorChart = new Chart(ctx, {
        type: 'line',
        data: chartData,
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: { beginAtZero: true, grid: { color: 'rgba(255, 255, 255, 0.1)' } },
                x: { grid: { display: false } }
            },
            plugins: { legend: { labels: { color: '#94a3b8' } } }
        }
    });
}

// WebSocket Connection
function connectWS() {
    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
        document.getElementById('statusDot').classList.add('online');
        document.getElementById('statusText').innerText = 'Connected';
    };

    socket.onclose = () => {
        document.getElementById('statusDot').classList.remove('online');
        document.getElementById('statusText').innerText = 'Disconnected. Retrying...';
        setTimeout(connectWS, 3000);
    };

    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        if (msg.type === 'state_update') {
            updateUIState(msg.state);
        } else if (msg.type === 'sensor_update') {
            updateSensorData(msg.data);
            updateAnomalyUI(msg.data.anomaly);
        } else if (msg.type === 'sys_alert') {
            showAlert(msg.message);
        }
    };
}

function updateAnomalyUI(status) {
    const display = document.getElementById('anomalyDisplay');
    const indicator = document.getElementById('anomalyIndicator');
    const lastCheck = document.getElementById('anomalyLastCheck');

    display.innerText = status === 'YES' ? 'ANOMALY' : 'NORMAL';
    display.style.color = status === 'YES' ? 'var(--danger)' : 'var(--success)';

    indicator.className = 'status-dot ' + (status === 'YES' ? '' : 'online');
    if (status === 'YES') indicator.style.background = 'var(--danger)';
    else indicator.style.background = 'var(--success)';

    lastCheck.innerText = `Last checked: ${new Date().toLocaleTimeString()}`;
}

function updateUIState(state) {
    document.getElementById('modeDisplay').innerText = state.mode;
    document.getElementById('toggleMode').innerText = state.mode === 'AUTO' ? 'Switch to Manual' : 'Switch to Auto';

    document.getElementById('kipasSwitch').checked = state.kipas === 1;
    document.getElementById('mistSwitch').checked = state.mist === 1;
    document.getElementById('heaterSwitch').checked = state.heater === 1;

    document.getElementById('thresholdVal').innerText = state.temp_threshold;
    document.getElementById('thresholdSlider').value = state.temp_threshold;
}

function updateSensorData(data) {
    document.getElementById('tempValue').innerText = data.temp.toFixed(1);
    document.getElementById('humValue').innerText = data.hum.toFixed(1);

    // Update Chart
    const time = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    chartData.labels.push(time);
    chartData.datasets[0].data.push(data.temp);
    chartData.datasets[1].data.push(data.hum);

    if (chartData.labels.length > maxDataPoints) {
        chartData.labels.shift();
        chartData.datasets[0].data.shift();
        chartData.datasets[1].data.shift();
    }
    sensorChart.update();

    // Check CloudWatch Alarms periodically
    fetchAlarmStatus();
}

async function fetchAlarmStatus() {
    try {
        const res = await fetch(`${apiUrl}/alarms`);
        const data = await res.json();
        const alarmSpan = document.getElementById('alarmStatus');
        alarmSpan.innerText = data.state;
        alarmSpan.style.color = data.state === 'ALARM' ? 'var(--danger)' : 'var(--success)';
    } catch (e) { }
}

function showAlert(msg) {
    const alertBox = document.getElementById('alertBox');
    const alertMsg = document.getElementById('alertMsg');
    alertMsg.innerText = msg;
    alertBox.style.display = 'block';
    setTimeout(() => { alertBox.style.display = 'none'; }, 5000);
}

// Control Events
async function sendControl(component, status) {
    try {
        await fetch(`${apiUrl}/control`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ component, status })
        });
    } catch (e) { console.error('Control error:', e); }
}

document.getElementById('toggleMode').onclick = () => {
    const isAuto = document.getElementById('modeDisplay').innerText === 'AUTO';
    sendControl('mode', isAuto ? 1 : 0); // Toggle to manual(1) or auto(0)
};

document.getElementById('kipasSwitch').onchange = (e) => sendControl('kipas', e.target.checked ? 1 : 0);
document.getElementById('mistSwitch').onchange = (e) => sendControl('mist', e.target.checked ? 1 : 0);
document.getElementById('heaterSwitch').onchange = (e) => sendControl('heater', e.target.checked ? 1 : 0);

document.getElementById('thresholdSlider').oninput = (e) => {
    document.getElementById('thresholdVal').innerText = e.target.value;
};
document.getElementById('thresholdSlider').onchange = (e) => sendControl('threshold', e.target.value);

document.getElementById('trainBtn').onclick = async () => {
    const btn = document.getElementById('trainBtn');
    btn.innerText = 'Training...';
    btn.disabled = true;
    try {
        const res = await fetch(`${apiUrl}/train`, { method: 'POST' });
        const data = await res.json();
        alert(data.message);
    } catch (e) { alert('Training failed'); }
    btn.innerText = 'Train AI Models';
    btn.disabled = false;
};

document.getElementById('refreshHistBtn').onclick = async () => {
    const btn = document.getElementById('refreshHistBtn');
    btn.innerText = 'Analyzing...';
    btn.disabled = true;
    try {
        const res = await fetch(`${apiUrl}/predict/historical`);
        const data = await res.json();
        const tbody = document.getElementById('histTableBody');
        tbody.innerHTML = '';

        data.forEach(item => {
            const row = document.createElement('tr');
            row.style.borderBottom = '1px solid rgba(255, 255, 255, 0.05)';
            const time = new Date(item.timestamp).toLocaleString();
            row.innerHTML = `
                <td style="padding: 0.5rem;">${time}</td>
                <td style="padding: 0.5rem;">${item.temp.toFixed(1)}°C</td>
                <td style="padding: 0.5rem;">${item.hum.toFixed(1)}%</td>
                <td style="padding: 0.5rem; color: ${item.prediction === 'COOLING' ? 'var(--primary)' : 'var(--text)'}">${item.prediction}</td>
                <td style="padding: 0.5rem; color: ${item.anomaly === 'YES' ? 'var(--danger)' : 'var(--success)'}">${item.anomaly}</td>
            `;
            tbody.appendChild(row);
        });
    } catch (e) { alert('Analysis failed'); }
    btn.innerText = 'Run Batch Analysis';
    btn.disabled = false;
};

// Start
initChart();
connectWS();
fetchAlarmStatus();
setInterval(fetchAlarmStatus, 30000);
