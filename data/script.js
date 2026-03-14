// ============================================
// ТЕРМИЧЕСКИЙ РЕФОРМИНГ - WEB ИНТЕРФЕЙС
// ============================================

// ============================================
// 1. КОНФИГУРАЦИЯ
// ============================================
const CONFIG = {
    // Буфер точек: 6 часов с обновлением раз в 3 секунды = 7200 точек
    MAX_POINTS: 7200,
    WS_URL: 'ws://' + window.location.hostname + ':8080',
    
    // Диапазоны отображения (в минутах)
    RANGES: {
        '10': 10,    // 10 минут
        '30': 30,    // 30 минут
        '60': 60,    // 1 час
        '180': 180,  // 3 часа
        '360': 360   // 6 часов
    },
    
    // Периоды обновления (мс)
    UPDATE_INTERVAL: 3000,  // 3 секунды, синхронизировано с дисплеем
    
    // Настройки реконнекта
    RECONNECT: {
        MAX_ATTEMPTS: 5,
        BASE_DELAY: 1000,
        MAX_DELAY: 30000
    }
};

// ============================================
// 2. СОСТОЯНИЕ
// ============================================
const state = {
    // WebSocket
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    
    // Данные (соответствуют формату ESP32)
    powerState: false,      // из state (1/0)
    targetTemp: 45.0,       // из target
    currentTime: "00:00",   // из time
    
    // Датчики (3 штуки)
    sensors: [0, 0, 0],     // sensor0, sensor1, sensor2
    
    // Мощность (пока заглушка, потом добавим)
    powerValue: 0,
    duty: 0,
    
    // Буфер данных для графика
    buffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        sensor0: new Array(CONFIG.MAX_POINTS).fill(null),  // Гильза (зелёный)
        sensor1: new Array(CONFIG.MAX_POINTS).fill(null),  // 50см (жёлтый)
        sensor2: new Array(CONFIG.MAX_POINTS).fill(null),  // 100см (оранжевый)
        target: new Array(CONFIG.MAX_POINTS).fill(null),    // Уставка
        index: 0,
        count: 0
    },
    
    // Отображение
    currentRange: 30,  // 30 минут по умолчанию
    chart: null
};

// ============================================
// 3. DOM ЭЛЕМЕНТЫ
// ============================================
const dom = {
    // Верхняя панель
    topSetpoint: document.getElementById('topSetpoint'),
    topTime: document.getElementById('topTime'),
    wifiDot: document.getElementById('wifiDot'),
    
    // Левая панель - датчики
    sensor1: document.getElementById('sensor1'),
    sensor2: document.getElementById('sensor2'),
    sensor3: document.getElementById('sensor3'),
    
    // Левая панель - управление
    btnPower: document.getElementById('btnPower'),
    setpointInput: document.getElementById('setpointInput'),
    powerValue: document.getElementById('powerValue'),
    timerValue: document.getElementById('timerValue'),
    wifiDotLeft: document.getElementById('wifiDotLeft'),
    
    // Нижняя панель (отладка)
    debugBuffer: document.getElementById('debugBuffer'),
    debugSensors: document.getElementById('debugSensors'),
    debugMode: document.getElementById('debugMode')
};

// ============================================
// 4. ИНИЦИАЛИЗАЦИЯ ГРАФИКА
// ============================================
function initChart() {
    const ctx = document.getElementById('tempChart').getContext('2d');
    
    state.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { 
                    data: [], 
                    borderColor: '#00FF00', 
                    borderWidth: 2,
                    tension: 0.2,
                    pointRadius: 0
                },
                { 
                    data: [], 
                    borderColor: '#FFFF00', 
                    borderWidth: 2,
                    tension: 0.2,
                    pointRadius: 0
                },
                { 
                    data: [], 
                    borderColor: '#FFA500', 
                    borderWidth: 2,
                    tension: 0.2,
                    pointRadius: 0
                },
                { 
                    data: [], 
                    borderColor: '#FFFFFF', 
                    borderWidth: 1.5,
                    borderDash: [5, 5],
                    tension: 0,
                    pointRadius: 0
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 0 },
            scales: {
                y: { 
                    min: 20,
                    max: 70,
                    grid: { color: '#333' },
                    ticks: { color: '#ccc', stepSize: 5 }
                },
                x: { 
                    ticks: { 
                        color: '#ccc',
                        maxTicksLimit: 8,
                        callback: function(val, index) {
                            const label = this.getLabelForValue(val);
                            return label ? label.substring(0, 5) : '';
                        }
                    }
                }
            },
            plugins: {
                legend: { display: false },
                zoom: {
                    pan: { enabled: true, mode: 'y' },
                    wheel: { enabled: false },
                    drag: { enabled: true, mode: 'y' },
                    pinch: { enabled: true, mode: 'y' }
                }
            }
        }
    });
}

// ============================================
// 5. ОБНОВЛЕНИЕ ГРАФИКА
// ============================================
function updateChart() {
    if (!state.chart) return;
    
    // Сколько точек показываем (переводим минуты в точки)
    // 3 секунды на точку = 20 точек в минуту
    const pointsPerMinute = Math.floor(60000 / CONFIG.UPDATE_INTERVAL);
    const desiredPoints = state.currentRange * pointsPerMinute;
    const availablePoints = Math.min(state.buffer.count, desiredPoints);
    
    // Создаём массивы нужной длины
    const labels = new Array(desiredPoints).fill('');
    const datasets = [
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null),
        new Array(desiredPoints).fill(null)
    ];
    
    // Заполняем данными
    for (let i = 0; i < availablePoints; i++) {
        const bufferIdx = (state.buffer.index - availablePoints + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availablePoints + i;
        
        labels[chartIdx] = state.buffer.time[bufferIdx];
        datasets[0][chartIdx] = state.buffer.sensor0[bufferIdx];
        datasets[1][chartIdx] = state.buffer.sensor1[bufferIdx];
        datasets[2][chartIdx] = state.buffer.sensor2[bufferIdx];
        datasets[3][chartIdx] = state.buffer.target[bufferIdx];
    }
    
    state.chart.data.labels = labels;
    state.chart.data.datasets.forEach((ds, i) => {
        ds.data = datasets[i];
    });
    
    state.chart.update();
    
    // Обновляем отладку
    if (dom.debugBuffer) {
        dom.debugBuffer.textContent = `${state.buffer.count}/${CONFIG.MAX_POINTS}`;
    }
}

// ============================================
// 6. ОБНОВЛЕНИЕ ИНТЕРФЕЙСА
// ============================================
function updateUI() {
    // Датчики
    if (dom.sensor1) dom.sensor1.textContent = state.sensors[0].toFixed(1);  // sensor0
    if (dom.sensor2) dom.sensor2.textContent = state.sensors[1].toFixed(1);  // sensor1
    if (dom.sensor3) dom.sensor3.textContent = state.sensors[2].toFixed(1);  // sensor2
    
    // Уставка
    if (dom.topSetpoint) dom.topSetpoint.textContent = state.targetTemp.toFixed(1) + '°C';
    if (dom.setpointInput) dom.setpointInput.value = state.targetTemp.toFixed(1);
    
    // Время (из ESP)
    if (dom.topTime) dom.topTime.textContent = state.currentTime;
    if (dom.timerValue) dom.timerValue.textContent = state.currentTime;
    
    // Кнопка питания
    if (dom.btnPower) {
        dom.btnPower.textContent = state.powerState ? 'ВЫКЛ' : 'ВКЛ';
        dom.btnPower.className = 'power-btn' + (state.powerState ? '' : ' off');
    }
    
    // Мощность (пока заглушка)
    if (dom.powerValue) {
        dom.powerValue.textContent = `${state.powerValue.toFixed(1)}% (${state.duty})`;
    }
    
    // Отладка
    if (dom.debugSensors) dom.debugSensors.textContent = '3';
    if (dom.debugMode) dom.debugMode.textContent = state.powerState ? 'Работа' : 'Стоп';
}

// ============================================
// 7. ОБНОВЛЕНИЕ WiFi СТАТУСА
// ============================================
function updateWiFiStatus(online) {
    const dotClass = online ? 'online' : 'offline-pulse';
    if (dom.wifiDot) {
        dom.wifiDot.className = `wifi-dot ${dotClass}`;
    }
    if (dom.wifiDotLeft) {
        dom.wifiDotLeft.className = `wifi-dot ${dotClass}`;
    }
}

// ============================================
// 8. ОБРАБОТКА ВХОДЯЩИХ ДАННЫХ
// ============================================
function processData(data) {
    console.log('Получены данные:', data);  // Отладка
    
    // Сохраняем состояние (соответствует формату ESP32)
    state.powerState = data.state === 1;      // state: 1=ON, 0=OFF
    state.targetTemp = data.target || 45.0;
    state.currentTime = data.time || '00:00';
    
    // Датчики: sensor0, sensor1, sensor2
    state.sensors = [
        data.sensor0 || 0,
        data.sensor1 || 0,
        data.sensor2 || 0
    ];
    
    // Мощность (пока нет в данных, оставляем заглушку)
    // Когда появится - будет data.power и data.duty
    
    // Добавляем точку в буфер графика
    const now = new Date();
    const timeLabel = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}`;
    
    const idx = state.buffer.index;
    state.buffer.time[idx] = timeLabel;
    state.buffer.sensor0[idx] = data.sensor0 || null;
    state.buffer.sensor1[idx] = data.sensor1 || null;
    state.buffer.sensor2[idx] = data.sensor2 || null;
    state.buffer.target[idx] = state.targetTemp;
    
    state.buffer.index = (idx + 1) % CONFIG.MAX_POINTS;
    if (state.buffer.count < CONFIG.MAX_POINTS) state.buffer.count++;
    
    // Обновляем интерфейс
    updateUI();
    updateChart();
}

// ============================================
// 9. WEBSOCKET
// ============================================
function connectWebSocket() {
    if (state.socket) {
        state.socket.close();
    }
    
    updateWiFiStatus(false);
    
    try {
        state.socket = new WebSocket(CONFIG.WS_URL);
    } catch (e) {
        scheduleReconnect();
        return;
    }
    
    state.socket.onopen = function() {
        updateWiFiStatus(true);
        state.reconnectAttempts = 0;
    };
    
    state.socket.onclose = function() {
        updateWiFiStatus(false);
        scheduleReconnect();
    };
    
    state.socket.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            processData(data);
        } catch (e) {
            console.error('Ошибка парсинга:', e);
        }
    };
}

function scheduleReconnect() {
    state.reconnectAttempts++;
    
    const delay = Math.min(
        CONFIG.RECONNECT.MAX_DELAY,
        CONFIG.RECONNECT.BASE_DELAY * Math.pow(2, state.reconnectAttempts)
    );
    
    if (state.reconnectTimeout) clearTimeout(state.reconnectTimeout);
    state.reconnectTimeout = setTimeout(connectWebSocket, delay);
}

// ============================================
// 10. ОТПРАВКА КОМАНД
// ============================================
function sendCommand(command, value) {
    if (!state.socket || state.socket.readyState !== WebSocket.OPEN) return;
    
    const msg = { command: command };
    
    if (command === 'setPower') {
        msg.value = value;
    } else if (command === 'setTarget') {
        msg.value = value;
    }
    
    state.socket.send(JSON.stringify(msg));
}

// ============================================
// 11. УПРАВЛЕНИЕ
// ============================================
function setupControls() {
    // Кнопка питания
    dom.btnPower.addEventListener('click', () => {
        state.powerState = !state.powerState;
        sendCommand('setPower', state.powerState ? 1 : 0);
        updateUI();
    });
    
    // Изменение уставки
    dom.setpointInput.addEventListener('change', function() {
        const val = parseFloat(this.value);
        if (!isNaN(val) && val >= 40 && val <= 70) {
            state.targetTemp = val;
            sendCommand('setTarget', val);
            updateUI();
        } else {
            this.value = state.targetTemp.toFixed(1);
        }
    });
    
    // Кнопки масштаба графика
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = parseInt(this.dataset.range);
            if (!isNaN(range) && CONFIG.RANGES[range.toString()]) {
                state.currentRange = range;
                
                document.querySelectorAll('.scale-btn').forEach(b => 
                    b.classList.remove('active')
                );
                this.classList.add('active');
                
                updateChart();
            }
        });
    });
}

// ============================================
// 12. ЗАПУСК
// ============================================
window.addEventListener('load', () => {
    initChart();
    setupControls();
    connectWebSocket();
    
    // Устанавливаем активную кнопку (30 минут по умолчанию)
    document.querySelectorAll('.scale-btn').forEach(btn => {
        if (btn.dataset.range === '30') {
            btn.classList.add('active');
        }
    });
    
    // Начальное состояние интерфейса
    updateUI();
    updateWiFiStatus(false);
});