let socket;
let sensorChart;
const maxDataPoints = 20;
let alertHistory = [];

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
let wsReconnectTimer = null;

function connectWS() {
    if (wsReconnectTimer) clearTimeout(wsReconnectTimer);

    socket = new WebSocket(wsUrl);

    socket.onopen = () => {
        document.getElementById('statusDot').classList.add('online');
        document.getElementById('statusText').innerText = 'Connected';
        console.log('[WS] Connected');
    };

    socket.onclose = () => {
        document.getElementById('statusDot').classList.remove('online');
        document.getElementById('statusText').innerText = 'Reconnecting...';
        console.log('[WS] Disconnected, retrying in 3s...');
        wsReconnectTimer = setTimeout(connectWS, 3000);
    };

    socket.onerror = () => {
        console.log('[WS] Error occurred');
        socket.close();
    };

    socket.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        if (msg.type === 'ping') return; // Ignore server pings
        if (msg.type === 'state_update') {
            updateUIState(msg.state);
        } else if (msg.type === 'sensor_update') {
            updateSensorData(msg.data);
            updateAnomalyUI(msg.data.anomaly);
        } else if (msg.type === 'sys_alert') {
            showAlert(msg.message);
            addAlertToHistory(msg.message);
        }
    };
}

function updateAnomalyUI(status) {
    const display = document.getElementById('anomalyDisplay');
    const indicator = document.getElementById('anomalyIndicator');
    const lastCheck = document.getElementById('anomalyLastCheck');

    display.innerText = status === 'YES' ? 'ANOMALY' : 'NORMAL';
    display.classList.remove('normal', 'anomaly');
    display.classList.add(status === 'YES' ? 'anomaly' : 'normal');

    indicator.classList.remove('online');
    if (status === 'YES') {
        indicator.style.background = 'var(--danger)';
        indicator.style.boxShadow = '0 0 10px var(--danger)';
    } else {
        indicator.classList.add('online');
    }

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
    const alertBanner = document.getElementById('alertBanner');
    const alertMsg = document.getElementById('alertBannerMsg');
    alertMsg.innerText = msg;
    alertBanner.classList.add('show');
    
    // Auto hide after 8 seconds
    setTimeout(() => {
        alertBanner.classList.remove('show');
    }, 8000);
}

// Close alert banner
document.getElementById('closeAlertBanner').onclick = () => {
    document.getElementById('alertBanner').classList.remove('show');
};

// Alert History
function addAlertToHistory(msg) {
    const now = new Date();
    const time = now.toLocaleTimeString();
    
    const alertItem = {
        message: msg,
        time: time,
        id: Date.now()
    };
    
    alertHistory.unshift(alertItem);
    
    // Keep only last 20 alerts
    if (alertHistory.length > 20) {
        alertHistory.pop();
    }
    
    renderAlertHistory();
}

function renderAlertHistory() {
    const container = document.getElementById('alertHistory');
    
    if (alertHistory.length === 0) {
        container.innerHTML = '<p class="no-alerts">No alerts yet</p>';
        return;
    }
    
    container.innerHTML = '';
    alertHistory.forEach(alert => {
        const div = document.createElement('div');
        div.className = 'alert-item';
        div.innerHTML = `
            <i data-lucide="alert-triangle" class="i-small" style="color: var(--danger); flex-shrink: 0;"></i>
            <div>
                <div>${alert.message}</div>
                <div class="alert-time">${alert.time}</div>
            </div>
        `;
        container.appendChild(div);
    });
    
    lucide.createIcons();
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

async function loadHistoricalData() {
    const btn = document.getElementById('refreshHistBtn');
    const refreshIcon = document.getElementById('refreshIcon');
    const tbody = document.getElementById('histTableBody');
    
    btn.disabled = true;
    refreshIcon.style.animation = 'spin 1s linear infinite';
    tbody.style.opacity = '0.5';
    tbody.style.transition = 'opacity 0.2s ease';
    
    try {
        const res = await fetch(`${apiUrl}/predict/historical`);
        const data = await res.json();
        tbody.innerHTML = '';

        if (data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" class="no-data">No historical data yet</td></tr>';
        } else {
            data.forEach((item, index) => {
                const row = document.createElement('tr');
                row.style.opacity = '0';
                row.style.transform = 'translateY(10px)';
                row.style.transition = 'all 0.3s ease';
                row.style.transitionDelay = `${index * 0.02}s`;
                
                const time = new Date(item.timestamp).toLocaleString();
                row.innerHTML = `
                    <td>${time}</td>
                    <td>${item.temp.toFixed(1)}°C</td>
                    <td>${item.hum.toFixed(1)}%</td>
                    <td style="color: ${item.prediction === 'COOLING' ? 'var(--primary)' : 'var(--text)'}">${item.prediction}</td>
                    <td style="color: ${item.anomaly === 'YES' ? 'var(--danger)' : 'var(--success)'}">${item.anomaly}</td>
                `;
                tbody.appendChild(row);
                
                // Trigger animation
                requestAnimationFrame(() => {
                    row.style.opacity = '1';
                    row.style.transform = 'translateY(0)';
                });
            });
        }
    } catch (e) { 
        console.error('Analysis failed', e);
    }
    
    btn.disabled = false;
    refreshIcon.style.animation = 'none';
    tbody.style.opacity = '1';
}

document.getElementById('refreshHistBtn').onclick = loadHistoricalData;

// Add spin animation
const style = document.createElement('style');
style.textContent = `
    @keyframes spin {
        from { transform: rotate(0deg); }
        to { transform: rotate(360deg); }
    }
`;
document.head.appendChild(style);

// Start
initChart();
connectWS();
fetchAlarmStatus();
loadHistoricalData(); // Auto load on startup
setInterval(fetchAlarmStatus, 30000);
setInterval(loadHistoricalData, 10000); // Auto refresh every 10 seconds
