// ============================================
// ИНТЕРФЕЙС (ОТОБРАЖЕНИЕ ДАННЫХ)
// ============================================

// ============================================================================
// ⚠️ ВНИМАНИЕ: Здесь можно менять только то, что помечено "МОЖНО МЕНЯТЬ"
// ============================================================================

// ============================================================================
// updateUI() - ОБНОВЛЕНИЕ ВСЕГО ИНТЕРФЕЙСА
// Вызывается каждый раз, когда приходят новые данные с ESP
// ============================================================================
function updateUI() {
    
    // --- ДАТЧИКИ (НЕ ТРОГАТЬ ЛОГИКУ, но можно менять формат) ---
    if (dom.sensor1) dom.sensor1.textContent = state.sensors[0].toFixed(1);  // МОЖНО: toFixed(1) → toFixed(0) если не нужны десятые
    if (dom.sensor2) dom.sensor2.textContent = state.sensors[1].toFixed(1);  // МОЖНО: toFixed(1) → toFixed(0)
    if (dom.sensor3) dom.sensor3.textContent = state.sensors[2].toFixed(1);  // МОЖНО: toFixed(1) → toFixed(0)
    
    // --- УСТАВКА (МОЖНО МЕНЯТЬ ФОРМАТ) ---
    if (dom.topSetpoint) dom.topSetpoint.textContent = Math.round(state.targetTemp) + '°C';  // МОЖНО: убрать Math.round(), добавить "°C" или "C"
    
    // --- ПОЛЕ ВВОДА УСТАВКИ (НЕ ТРОГАТЬ - защита от редактирования) ---
    if (dom.setpointInput && !isEditingSetpoint) {
        dom.setpointInput.value = Math.round(state.targetTemp);
    }
    
    // --- ВРЕМЯ РАБОТЫ (МОЖНО МЕНЯТЬ ФОРМАТ) ---
    if (dom.topTime) dom.topTime.textContent = state.currentTime;        // МОЖНО: добавить "сек" или изменить формат
    if (dom.timerValue) dom.timerValue.textContent = state.currentTime;  // МОЖНО: то же самое
    
    // --- ЖИВОЕ ВРЕМЯ (МОЖНО МЕНЯТЬ ФОРМАТ) ---
    if (dom.ntpTime) {
        if (state.ntpTime && state.ntpTime !== '00:00:00') {
            dom.ntpTime.textContent = state.ntpTime;  // МОЖНО: показать только ЧЧ:ММ вместо ЧЧ:ММ:СС
        } else {
            const now = new Date();
            // МОЖНО: изменить формат (например, убрать секунды)
            const timeStr = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
            dom.ntpTime.textContent = timeStr;
        }
    }
    
    // --- КНОПКА ПИТАНИЯ (НЕ ТРОГАТЬ - логика) ---
    if (dom.btnPower) {
        dom.btnPower.textContent = state.powerState ? 'ВЫКЛ' : 'ВКЛ';  // МОЖНО: поменять текст (например, "OFF"/"ON")
        dom.btnPower.className = 'power-btn' + (state.powerState ? '' : ' off');
    }
    
    // --- МОЩНОСТЬ (МОЖНО МЕНЯТЬ ФОРМАТ) ---
    if (dom.powerValue) {
        // МОЖНО: убрать (duty), оставить только проценты
        dom.powerValue.textContent = `${state.powerValue.toFixed(1)}% (${state.duty})`;
        // Альтернатива: `${state.powerValue.toFixed(1)}%` - без duty
    }
    
    // --- ОТЛАДКА (НЕ ТРОГАТЬ) ---
    if (dom.debugSensors) dom.debugSensors.textContent = '3';
    if (dom.debugMode) dom.debugMode.textContent = state.powerState ? 'Работа' : 'Стоп';  // МОЖНО: поменять текст
}

// ============================================================================
// updateWiFiStatus() - ОБНОВЛЕНИЕ ИНДИКАТОРА WiFi
// НЕ ТРОГАТЬ - критично для отображения статуса подключения
// ============================================================================
function updateWiFiStatus(online) {
    const dotClass = online ? 'online' : 'offline-pulse';
    if (dom.wifiDot) dom.wifiDot.className = `wifi-dot ${dotClass}`;
    if (dom.wifiDotLeft) dom.wifiDotLeft.className = `wifi-dot ${dotClass}`;
}

// ============================================================================
// ✅ КОРОТКО: ЧТО МОЖНО МЕНЯТЬ В ЭТОМ ФАЙЛЕ
// ============================================================================
// 1. Форматы чисел: toFixed(1) → toFixed(0) (убрать десятые)
// 2. Тексты: 'ВКЛ' → 'ON', 'Работа' → 'Active'
// 3. Отображение мощности: убрать (duty) если не нужно
// 4. Формат времени: показать только ЧЧ:ММ вместо ЧЧ:ММ:СС
//
// ⚠️ НЕ МЕНЯТЬ: логику обновления, названия функций, структуру if-ов
// ============================================================================