// ============================================
// УПРАВЛЕНИЕ (КНОПКИ, ВВОД)
// ============================================

function setupControls() {
    // Кнопка питания
    dom.btnPower.addEventListener('click', () => {
        state.powerState = !state.powerState;
        sendCommand('setPower', state.powerState ? 1 : 0);
        updateUI();
    });
    
    // Управление уставкой
    dom.setpointInput.addEventListener('focus', () => isEditingSetpoint = true);
    
    dom.setpointInput.addEventListener('keydown', (e) => {
        // Разрешаем управляющие клавиши
        if (e.key === 'Backspace' || e.key === 'Delete' || e.key === 'Tab' || 
            e.key === 'Escape' || e.key === 'Enter' || e.key.startsWith('Arrow')) {
            return;
        }
        // Запрещаем всё, кроме цифр
        if (!/^[0-9]$/.test(e.key)) {
            e.preventDefault();
        }
    });
    
    // Обработка Enter (ИСПРАВЛЕНО)
    dom.setpointInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            e.target.blur();  // Теперь работает
        }
    });
    
    // Завершение редактирования
    dom.setpointInput.addEventListener('blur', function() {
        const val = parseInt(this.value);
        const currentVal = Math.round(state.targetTemp);
        
        // Если поле пустое — возвращаем текущее значение
        if (this.value.trim() === '' || isNaN(val)) {
            this.value = currentVal;
            isEditingSetpoint = false;
            return;
        }
        
        // Проверяем диапазон
        if (val >= 30 && val <= 70) {
            // Если значение изменилось — отправляем на сервер
            if (val !== currentVal) {
                state.targetTemp = val;
                sendCommand('setTarget', val);
            } else {
                this.value = currentVal;
            }
        } else {
            // Если вне диапазона — возвращаем старое
            this.value = currentVal;
        }
        isEditingSetpoint = false;
    });
    
    // Кнопки масштаба графика
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = parseInt(this.dataset.range);
            if (!isNaN(range) && CONFIG.RANGES[range.toString()]) {
                state.currentRange = range;
                document.querySelectorAll('.scale-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
                updateChart();
            }
        });
    });
}