// ============================================
// СОСТОЯНИЕ И DOM ЭЛЕМЕНТЫ
// ============================================

// ============================================================================
// ⚠️ ВНИМАНИЕ: Здесь можно менять только то, что помечено "МОЖНО МЕНЯТЬ"
// ============================================================================

const state = {
    // --- WebSocket соединение (НЕ ТРОГАТЬ) ---
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    lastDataTime: Date.now(),
    keepaliveInterval: null,
    
    // --- Данные с ESP32 (НЕ ТРОГАТЬ - приходят с сервера) ---
    powerState: false,
    targetTemp: 45.0,            // ВРЕМЕННОЕ ЗНАЧЕНИЕ ДО ПРИХОДА ДАННЫХ С ESP
    currentTime: "00:00",
    ntpTime: "00:00:00",
    
    // --- Датчики (НЕ ТРОГАТЬ - приходят с сервера) ---
    sensors: [0, 0, 0],
    
    // --- Мощность нагрева (НЕ ТРОГАТЬ - приходит с сервера) ---
    powerValue: 0,
    duty: 0,
    
    // --- Буфер данных для графика (НЕ ТРОГАТЬ - заполняется автоматически) ---
    buffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        sensor0: new Array(CONFIG.MAX_POINTS).fill(null),
        sensor1: new Array(CONFIG.MAX_POINTS).fill(null),
        sensor2: new Array(CONFIG.MAX_POINTS).fill(null),
        target: new Array(CONFIG.MAX_POINTS).fill(null),
        index: 0,
        count: 0
    },
    
    // --- НАСТРОЙКИ ГРАФИКА (МОЖНО МЕНЯТЬ) ---
    currentRange: 45,      // КАКОЙ МАСШТАБ ВКЛЮЧЕН ПРИ ЗАГРУЗКЕ
                           // Доступные варианты:
                           // 10  - 10 минут (600 точек)
                           // 30  - 30 минут (1800 точек)
                           // 45  - 45 минут (2700 точек)
                           // 'half'    - половинное сжатие
                           // 'quarter' - четвертное сжатие
    
    chart: null            // Ссылка на график (НЕ ТРОГАТЬ)
};

// ============================================================================
// ФЛАГ РЕДАКТИРОВАНИЯ (НЕ ТРОГАТЬ - управляется автоматически)
// ============================================================================
let isEditingSetpoint = false;

// ============================================================================
// DOM ЭЛЕМЕНТЫ (НЕ ТРОГАТЬ - ссылки на страницу)
// ============================================================================
const dom = {
    // Верхняя панель
    topSetpoint: document.getElementById('topSetpoint'),
    topTime: document.getElementById('topTime'),
    wifiDot: document.getElementById('wifiDot'),
    ntpTime: document.getElementById('ntpTime'),
    
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

// ============================================================================
// ✅ КОРОТКО: ЧТО МОЖНО МЕНЯТЬ
// ============================================================================
// 1. currentRange - масштаб графика при загрузке (10, 30, 45, 'half', 'quarter')
// 2. targetTemp - начальная уставка (если нужно)
// 3. powerState - начальное состояние нагрева (true/false)
//
// ⚠️ ВСЁ ОСТАЛЬНОЕ - НЕ ТРОГАТЬ!
// ============================================================================