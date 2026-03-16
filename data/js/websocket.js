// ============================================
// WEBSOCKET И ОБМЕН ДАННЫМИ
// ============================================

function connectWebSocket() {
    if (state.socket) state.socket.close();
    
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
        startKeepalive();
        state.socket.send(JSON.stringify({ command: 'getHistory' }));
    };
    
    state.socket.onclose = function() {
        updateWiFiStatus(false);
        if (state.keepaliveInterval) clearInterval(state.keepaliveInterval);
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

function startKeepalive() {
    if (state.keepaliveInterval) clearInterval(state.keepaliveInterval);
    state.keepaliveInterval = setInterval(() => {
        if (state.socket && state.socket.readyState === WebSocket.OPEN) {
            state.socket.send(JSON.stringify({ command: 'ping' }));
        }
    }, CONFIG.KEEPALIVE_INTERVAL);
}

function checkDataTimeout() {
    const now = Date.now();
    if (now - state.lastDataTime > CONFIG.DATA_TIMEOUT) {
        console.log('Таймаут данных, переподключаемся...');
        if (state.socket) state.socket.close();
    }
}

function sendCommand(command, value) {
    if (!state.socket || state.socket.readyState !== WebSocket.OPEN) return;
    const msg = { command: command };
    if (command === 'setPower') msg.value = value;
    else if (command === 'setTarget') msg.value = value;
    state.socket.send(JSON.stringify(msg));
}

function processData(data) {
    state.lastDataTime = Date.now();
    
    // История
    if (data.history && Array.isArray(data.history)) {
        console.log(`Загрузка истории: ${data.history.length} точек`);
        
        state.buffer.index = 0;
        state.buffer.count = 0;
        state.buffer.time.fill('');
        state.buffer.sensor0.fill(null);
        state.buffer.sensor1.fill(null);
        state.buffer.sensor2.fill(null);
        state.buffer.target.fill(null);
        
        data.history.forEach(point => {
            const idx = state.buffer.index;
            const date = new Date(point.time * 1000);
            const timeLabel = `${date.getHours().toString().padStart(2,'0')}:${date.getMinutes().toString().padStart(2,'0')}`;
            
            state.buffer.time[idx] = timeLabel;
            state.buffer.sensor0[idx] = point.sensor0;
            state.buffer.sensor1[idx] = point.sensor1;
            state.buffer.sensor2[idx] = point.sensor2;
            state.buffer.target[idx] = point.target;
            
            state.buffer.index = (idx + 1) % CONFIG.MAX_POINTS;
            if (state.buffer.count < CONFIG.MAX_POINTS) state.buffer.count++;
        });
        
        updateChart();
        return;
    }
    
    // Обычное обновление
    state.powerState = data.state === 1;
    state.targetTemp = data.target || 45.0;
    state.currentTime = data.time || '00:00';
    state.ntpTime = data.ntpTime || '00:00:00';
    
    state.sensors = [
        data.sensor0 || 0,
        data.sensor1 || 0,
        data.sensor2 || 0
    ];
    
    if (data.power !== undefined) {
        state.powerValue = data.power;
        state.duty = data.duty;
    }
    
    const nowDate = new Date();
    const timeLabel = `${nowDate.getHours().toString().padStart(2,'0')}:${nowDate.getMinutes().toString().padStart(2,'0')}`;
    
    const idx = state.buffer.index;
    state.buffer.time[idx] = timeLabel;
    state.buffer.sensor0[idx] = data.sensor0 || null;
    state.buffer.sensor1[idx] = data.sensor1 || null;
    state.buffer.sensor2[idx] = data.sensor2 || null;
    state.buffer.target[idx] = state.targetTemp;
    
    state.buffer.index = (idx + 1) % CONFIG.MAX_POINTS;
    if (state.buffer.count < CONFIG.MAX_POINTS) state.buffer.count++;
    
    updateUI();
    updateChart();
}