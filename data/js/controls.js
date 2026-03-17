// ============================================
// УПРАВЛЕНИЕ (КНОПКИ, ВВОД)
// ============================================

// ============================================================================
// ⚠️ ВНИМАНИЕ: Здесь можно менять только то, что помечено "МОЖНО МЕНЯТЬ"
// ============================================================================

function setupControls() {
    
    // --- КНОПКА ПИТАНИЯ (НЕ ТРОГАТЬ - логика) ---
    dom.btnPower.addEventListener('click', () => {
        state.powerState = !state.powerState;
        sendCommand('setPower', state.powerState ? 1 : 0);
        updateUI();
    });
    
    // --- УПРАВЛЕНИЕ УСТАВКОЙ ---
    
    // Фокус на поле ввода (НЕ ТРОГАТЬ)
    dom.setpointInput.addEventListener('focus', () => isEditingSetpoint = true);
    
    // Обработка нажатий клавиш (МОЖНО МЕНЯТЬ разрешённые символы)
    dom.setpointInput.addEventListener('keydown', (e) => {
        // Разрешаем управляющие клавиши (НЕ ТРОГАТЬ)
        if (e.key === 'Backspace' || e.key === 'Delete' || e.key === 'Tab' || 
            e.key === 'Escape' || e.key === 'Enter' || e.key.startsWith('Arrow')) {
            return;
        }
        // МОЖНО: разрешить ввод десятичной точки, если нужно
        // if (e.key === '.') return;
        
        // Запрещаем всё, кроме цифр (МОЖНО добавить другие символы)
        if (!/^[0-9]$/.test(e.key)) {
            e.preventDefault();
        }
    });
    
    // Обработка Enter (НЕ ТРОГАТЬ)
    dom.setpointInput.addEventListener('keypress', (e) => {
        if (e.key === 'Enter') {
            e.target.blur();
        }
    });
    
    // Завершение редактирования (МОЖНО МЕНЯТЬ ДИАПАЗОН ТЕМПЕРАТУР)
    dom.setpointInput.addEventListener('blur', function() {
        const val = parseInt(this.value);
        const currentVal = Math.round(state.targetTemp);
        
        if (this.value.trim() === '' || isNaN(val)) {
            this.value = currentVal;
            isEditingSetpoint = false;
            return;
        }
        
        // 🔧 МОЖНО МЕНЯТЬ: диапазон допустимых температур
        // Сейчас: от 30 до 70
        // Хочешь от 20 до 80? Меняй здесь:
        if (val >= 30 && val <= 70) {  // ← ИЗМЕНИТЬ ЗНАЧЕНИЯ ЗДЕСЬ
            if (val !== currentVal) {
                state.targetTemp = val;
                sendCommand('setTarget', val);
            } else {
                this.value = currentVal;
            }
        } else {
            this.value = currentVal;
        }
        isEditingSetpoint = false;
    });
    
    // --- КНОПКИ МАСШТАБА ГРАФИКА (НЕ ТРОГАТЬ - логика) ---
    document.querySelectorAll('.scale-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const range = this.dataset.range;
            
            if (range === 'half' || range === 'quarter') {
                state.currentRange = range;
            } else {
                state.currentRange = parseInt(range);
            }
            
            document.querySelectorAll('.scale-btn').forEach(b => b.classList.remove('active'));
            this.classList.add('active');
            updateChart();
        });
    });
}

// ============================================================================
// ✅ КОРОТКО: ЧТО МОЖНО МЕНЯТЬ В ЭТОМ ФАЙЛЕ
// ============================================================================
// 1. ДИАПАЗОН ТЕМПЕРАТУР: в блоке if (val >= 30 && val <= 70)
// 2. РАЗРЕШЁННЫЕ СИМВОЛЫ: в keydown добавить разрешение для точки
//
// ⚠️ НЕ МЕНЯТЬ: логику отправки команд, обработку Enter, логику кнопок масштаба
// ============================================================================