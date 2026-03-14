// ============================================
// 1. КОНФИГУРАЦИЯ
// ============================================
const CONFIG = {
    MAX_POINTS: 7200,
    WS_URL: 'ws://' + window.location.hostname + ':8080',
    MEASURE_INTERVAL: 1000,
    CHART_UPDATE_INTERVAL: 1000,
    RANGES: {
        '30м': 3600,
        '15м': 1800,
        '5м': 600,
        '1м': 120
    },
    RECONNECT: {
        MAX_ATTEMPTS: 5,
        DELAYS: [1, 2, 4, 8, 15, 30],
        WATCHDOG_INTERVAL: 5000
    },
    WATCHDOG: {
        DATA_TIMEOUT: 15000
    }
};

// ============================================
// 2. СОСТОЯНИЕ
// ============================================
const state = {
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    lastDataTime: Date.now(),
    pageVisible: true,
    
    dataBuffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        guild: new Array(CONFIG.MAX_POINTS).fill(null),
        wall50: new Array(CONFIG.MAX_POINTS).fill(null),
        wall75: new Array(CONFIG.MAX_POINTS).fill(null),
        wall100: new Array(CONFIG.MAX_POINTS).fill(null),
        index: 0,
        count: 0,
        lastTime: ''
    },
    
    lastValues: {
        mode: -1,
        color: -1,
        time: '',
        baseTemp: null
    },
    
    currentRange: 1800,  // 15м по умолчанию
    chart: null,
    powerState: false,
    targetTemp: 30.0
};

// ============================================
// 3. DOM ЭЛЕМЕНТЫ
// ============================================
const dom = {
    get: (id) => document.getElementById(id),
    
    modeDisplay: document.getElementById('modeDisplay'),
    card0: document.getElementById('card0'),
    card1: document.getElementById('card1'),
    card2: document.getElementById('card2'),
    card3: document.getElementById('card3'),
    panelStatusDot: document.getElementById('panelStatusDot'),
    panelStatusText: document.getElementById('panelStatusText'),
    targetValue: document.getElementById('targetValue'),
    btnPower: document.getElementById('btnPower'),
    debugBuffer: document.getElementById('debugBuffer'),
    debugRange: document.getElementById('debugRange'),
    debugActive: document.getElementById('debugActive'),
    debugIP: document.getElementById('debugIP'),
    minTemp: document.getElementById('minTemp'),
    maxTemp: document.getElementById('maxTemp'),
    modal: document.getElementById('targetModal'),
    modalTarget: document.getElementById('modalTarget'),
    modalSubmit: document.getElementById('modalSubmit'),
    modalClose: document.querySelector('.close')
};

// ============================================
// 4. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
// ============================================
function updateStatus(status, attempts = 0) {
    const dot = dom.panelStatusDot;
    const text = dom.panelStatusText;
    
    if (status === 'online') {
        dot.className = 'panel-status-dot online';
        text.className = 'panel-status-text online';
        text.textContent = 'ON';
        state.reconnectAttempts = 0;
    } 
    else if (status === 'offline') {
        const isYellow = attempts <= CONFIG.RECONNECT.MAX_ATTEMPTS;
        dot.className = `panel-status-dot ${isYellow ? 'offline-yellow' : 'offline'}`;
        text.className = `panel-status-text ${isYellow ? 'offline-yellow' : 'offline'}`;
        text.textContent = 'OFF';
    }
}

function updateModeDisplay(mode, color, timeStr, baseTemp) {
    if (!dom.modeDisplay) return;
    
    let newText, newClass;
    
    if (mode === 0) {
        newClass = 'mode-bar mode0';
        newText = 'СТАБИЛИЗАЦИЯ ' + timeStr;
    } else {
        const colorClass = color === 1 ? 'mode1-yellow' : (color === 2 ? 'mode1-red' : 'mode1-green');
        newClass = `mode-bar ${colorClass}`;
        const baseStr = baseTemp !== null ? baseTemp.toFixed(1) : '--.-';
        newText = `РАБОЧИЙ ${baseStr}° ${timeStr}`;
    }
    
    dom.modeDisplay.className = newClass;
    dom.modeDisplay.textContent = newText;
}

function updateCards(data) {
    if (dom.card0) dom.card0.textContent = data.guild.toFixed(1);
    if (dom.card1) dom.card1.textContent = data.wall50.toFixed(1);
    if (dom.card2) dom.card2.textContent = data.wall75.toFixed(1);
    if (dom.card3) dom.card3.textContent = data.wall100.toFixed(1);
}

function updateTargetDisplay(value) {
    if (dom.targetValue) {
        dom.targetValue.textContent = value.toFixed(1);
    }
    state.targetTemp = value;
}

function updatePowerButton(state) {
    if (dom.btnPower) {
        dom.btnPower.textContent = state ? 'ВЫКЛ' : 'ВКЛ';
        dom.btnPower.style.background = state ? '#f44336' : '#4CAF50';
    }
}

// ============================================
// 5. ГРАФИК
// ============================================
function initChart() {
    const ctx = document.getElementById('tempChart').getContext('2d');
    state.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { label: 'Гильза', data: [], borderColor: '#00FF00', borderWidth: 1.5, tension: 0.3, pointRadius: 0 },
                { label: '50см', data: [], borderColor: '#FFFF00', borderWidth: 1.5, tension: 0.3, pointRadius: 0 },
                { label: '75см', data: [], borderColor: '#00FFFF', borderWidth: 1.5, tension: 0.3, pointRadius: 0 },
                { label: '100см', data: [], borderColor: '#FFA500', borderWidth: 1.5, tension: 0.3, pointRadius: 0 }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 0 },
            scales: {
                y: { 
                    min: 20, max: 70,
                    grid: { color: '#333' },
                    ticks: { color: '#ccc' }
                },
                x: { ticks: { color: '#ccc', maxTicksLimit: 8 } }
            },
            plugins: {
                legend: { display: false },
                zoom: {
                    pan: { enabled: true, mode: 'y' },
                    zoom: { enabled: true, mode: 'y', wheel: { enabled: false } }
                }
            }
        }
    });
}

function updateChart() {
    const desiredPoints = state.currentRange;
    const totalPoints = state.dataBuffer.count;
    
    if (!state.chart) return;
    
    // Обновляем labels
    state.chart.data.labels = new Array(desiredPoints).fill('');
    
    // Подготавливаем данные
    const datasets = [
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null)
    ];
    
    const availableData = Math.min(totalPoints, desiredPoints);
    
    for (let i = 0; i < availableData; i++) {
        const dataIdx = (state.dataBuffer.index - availableData + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availableData + i;
        
        if (state.dataBuffer.time[dataIdx]) {
            state.chart.data.labels[chartIdx] = state.dataBuffer.time[dataIdx];
        }
        
        datasets[0][chartIdx] = state.dataBuffer.guild[dataIdx];
        datasets[1][chartIdx] = state.dataBuffer.wall50[dataIdx];
        datasets[2][chartIdx] = state.dataBuffer.wall75[dataIdx];
        datasets[3][chartIdx] = state.dataBuffer.wall100[dataIdx];
    }
    
    state.chart.data.datasets.forEach((ds, i) => { ds.data = datasets[i]; });
    state.chart.update();
}

// ============================================
// 6. ОБРАБОТКА ДАННЫХ
// ============================================
function processData(data) {
    state.lastDataTime = Date.now();
    
    updateCards(data);
    updateModeDisplay(data.mode, data.color, data.time, data.baseTemp);
    
    // Сохраняем в буфер
    const now = new Date();
    const timeLabel = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
    
    const idx = state.dataBuffer.index;
    state.dataBuffer.time[idx] = timeLabel;
    state.dataBuffer.guild[idx] = data.guild;
    state.dataBuffer.wall50[idx] = data.wall50;
    state.dataBuffer.wall75[idx] = data.wall75;
    state.dataBuffer.wall100[idx] = data.wall100;
    
    state.dataBuffer.index = (idx + 1) % CONFIG.MAX_POINTS;
    if (state.dataBuffer.count < CONFIG.MAX_POINTS) state.dataBuffer.count++;
    
    updateChart();
    updateDebugInfo();
}

function updateDebugInfo() {
    if (dom.debugBuffer) dom.debugBuffer.textContent = `${state.dataBuffer.count}/${CONFIG.MAX_POINTS}`;
    if (dom.debugRange) dom.debugRange.textContent = state.currentRange;
    
    const btnName = Object.keys(CONFIG.RANGES).find(k => CONFIG.RANGES[k] === state.currentRange) || '15м';
    if (dom.debugActive) dom.debugActive.textContent = btnName;
}

// ============================================
// 7. WEBSOCKET
// ============================================
function connectWebSocket() {
    if (state.socket) {
        state.socket.close();
    }
    
    updateStatus('offline', state.reconnectAttempts + 1);
    
    try {
        state.socket = new WebSocket(CONFIG.WS_URL);
    } catch (e) {
        scheduleReconnect();
        return;
    }
    
    state.socket.onopen = function() {
        updateStatus('online');
        state.reconnectAttempts = 0;
        state.lastDataTime = Date.now();
        
        // Запрашиваем IP
        fetch('http://' + window.location.hostname)
            .then(() => { if (dom.debugIP) dom.debugIP.textContent = window.location.hostname; })
            .catch(() => {});
    };
    
    state.socket.onclose = function() {
        scheduleReconnect();
    };
    
    state.socket.onmessage = function(event) {
        try {
            processData(JSON.parse(event.data));
        } catch (e) {}
    };
}

function scheduleReconnect() {
    state.reconnectAttempts++;
    updateStatus('offline', state.reconnectAttempts);
    
    const delay = Math.min(30000, state.reconnectAttempts * 2000);
    
    if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
    state.reconnectTimeout = setTimeout(connectWebSocket, delay);
}

// ============================================
// 8. ОТПРАВКА КОМАНД
// ============================================
function sendCommand(command, value) {
    if (!state.socket || state.socket.readyState !== WebSocket.OPEN) return;
    
    const msg = { command: command };
    
    if (command === 'setTarget') {
        msg.value = value;
    } else if (command === 'setPower') {
        msg.value = value;
    } else if (command === 'setPID') {
        msg.Kp = value.Kp;
        msg.Ki = value.Ki;
        msg.Kd = value.Kd;
    }
    
    state.socket.send(JSON.stringify(msg));
}

// ============================================
// 9. УПРАВЛЕНИЕ
// ============================================
function setupControls() {
    // Кнопки масштаба
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = this.dataset.range;
            if (range && CONFIG.RANGES[range]) {
                state.currentRange = CONFIG.RANGES[range];
                document.querySelectorAll('.scale-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
                updateChart();
            }
        });
    });
    
    // Кнопка питания
    dom.btnPower.addEventListener('click', () => {
        state.powerState = !state.powerState;
        sendCommand('setPower', state.powerState);
        updatePowerButton(state.powerState);
    });
    
    // Кнопка установки уставки
    dom.btnSetTarget.addEventListener('click', () => {
        dom.modalTarget.value = state.targetTemp;
        dom.modal.style.display = 'block';
    });
    
    // Модальное окно
    dom.modalClose.addEventListener('click', () => {
        dom.modal.style.display = 'none';
    });
    
    dom.modalSubmit.addEventListener('click', () => {
        const val = parseFloat(dom.modalTarget.value);
        if (!isNaN(val) && val >= 0 && val <= 70) {
            sendCommand('setTarget', val);
            updateTargetDisplay(val);
        }
        dom.modal.style.display = 'none';
    });
    
    window.addEventListener('click', (e) => {
        if (e.target === dom.modal) dom.modal.style.display = 'none';
    });
    
    // Границы графика
    let minTimeout, maxTimeout;
    dom.minTemp.addEventListener('input', function() {
        clearTimeout(minTimeout);
        minTimeout = setTimeout(() => {
            const val = parseFloat(this.value);
            if (!isNaN(val) && state.chart) {
                state.chart.options.scales.y.min = val;
                state.chart.update();
            }
        }, 300);
    });
    
    dom.maxTemp.addEventListener('input', function() {
        clearTimeout(maxTimeout);
        maxTimeout = setTimeout(() => {
            const val = parseFloat(this.value);
            if (!isNaN(val) && state.chart) {
                state.chart.options.scales.y.max = val;
                state.chart.update();
            }
        }, 300);
    });
}

// ============================================
// 10. ЗАПУСК
// ============================================
window.addEventListener('load', () => {
    initChart();
    setupControls();
    connectWebSocket();
    
    // Устанавливаем активную кнопку масштаба
    document.querySelectorAll('.scale-btn').forEach(btn => {
        if (btn.dataset.range === '15м') btn.classList.add('active');
    });
});