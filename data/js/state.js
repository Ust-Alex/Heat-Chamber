// ============================================
// СОСТОЯНИЕ И DOM ЭЛЕМЕНТЫ
// ============================================

// Состояние приложения
const state = {
    socket: null,
    reconnectAttempts: 0,
    reconnectTimeout: null,
    lastDataTime: Date.now(),
    keepaliveInterval: null,
    
    powerState: false,
    targetTemp: 45.0,
    currentTime: "00:00",
    ntpTime: "00:00:00",
    
    sensors: [0, 0, 0],
    powerValue: 0,
    duty: 0,
    
    buffer: {
        time: new Array(CONFIG.MAX_POINTS).fill(''),
        sensor0: new Array(CONFIG.MAX_POINTS).fill(null),
        sensor1: new Array(CONFIG.MAX_POINTS).fill(null),
        sensor2: new Array(CONFIG.MAX_POINTS).fill(null),
        target: new Array(CONFIG.MAX_POINTS).fill(null),
        index: 0,
        count: 0
    },
    
    currentRange: 30,
    chart: null
};

// Флаг редактирования уставки
let isEditingSetpoint = false;

// DOM элементы
const dom = {
    topSetpoint: document.getElementById('topSetpoint'),
    topTime: document.getElementById('topTime'),
    wifiDot: document.getElementById('wifiDot'),
    ntpTime: document.getElementById('ntpTime'),
    
    sensor1: document.getElementById('sensor1'),
    sensor2: document.getElementById('sensor2'),
    sensor3: document.getElementById('sensor3'),
    
    btnPower: document.getElementById('btnPower'),
    setpointInput: document.getElementById('setpointInput'),
    powerValue: document.getElementById('powerValue'),
    timerValue: document.getElementById('timerValue'),
    wifiDotLeft: document.getElementById('wifiDotLeft'),
    
    debugBuffer: document.getElementById('debugBuffer'),
    debugSensors: document.getElementById('debugSensors'),
    debugMode: document.getElementById('debugMode')
};