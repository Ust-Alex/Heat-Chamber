// ============================================
// ИНТЕРФЕЙС
// ============================================

function updateUI() {
    // Датчики
    if (dom.sensor1) dom.sensor1.textContent = state.sensors[0].toFixed(1);
    if (dom.sensor2) dom.sensor2.textContent = state.sensors[1].toFixed(1);
    if (dom.sensor3) dom.sensor3.textContent = state.sensors[2].toFixed(1);
    
    // Уставка
    if (dom.topSetpoint) dom.topSetpoint.textContent = Math.round(state.targetTemp) + '°C';
    
    // Поле ввода уставки (только если не редактируется)
    if (dom.setpointInput && !isEditingSetpoint) {
        dom.setpointInput.value = Math.round(state.targetTemp);
    }
    
    // Время работы (из ESP)
    if (dom.topTime) dom.topTime.textContent = state.currentTime;
    if (dom.timerValue) dom.timerValue.textContent = state.currentTime;
    
    // Живое время (приоритет: NTP от ESP, если нет - браузерное)
    if (dom.ntpTime) {
        if (state.ntpTime && state.ntpTime !== '00:00:00') {
            dom.ntpTime.textContent = state.ntpTime;
        } else {
            const now = new Date();
            const timeStr = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
            dom.ntpTime.textContent = timeStr;
        }
    }
    
    // Кнопка питания
    if (dom.btnPower) {
        dom.btnPower.textContent = state.powerState ? 'ВЫКЛ' : 'ВКЛ';
        dom.btnPower.className = 'power-btn' + (state.powerState ? '' : ' off');
    }
    
    // Мощность
    if (dom.powerValue) {
        dom.powerValue.textContent = `${state.powerValue.toFixed(1)}% (${state.duty})`;
    }
    
    // Отладка
    if (dom.debugSensors) dom.debugSensors.textContent = '3';
    if (dom.debugMode) dom.debugMode.textContent = state.powerState ? 'Работа' : 'Стоп';
}

function updateWiFiStatus(online) {
    const dotClass = online ? 'online' : 'offline-pulse';
    if (dom.wifiDot) dom.wifiDot.className = `wifi-dot ${dotClass}`;
    if (dom.wifiDotLeft) dom.wifiDotLeft.className = `wifi-dot ${dotClass}`;
}