// ============================================
// КОНФИГУРАЦИЯ
// ============================================
const CONFIG = {
    MAX_POINTS: 2700,  // 3 часа при 4 сек
    WS_URL: 'ws://' + window.location.hostname + ':8080',
    RANGES: {
        '10': 10, '30': 30, '60': 60, '180': 180, '360': 360
    },
    UPDATE_INTERVAL: 1000,
    RECONNECT: {
        MAX_ATTEMPTS: 5,
        BASE_DELAY: 1000,
        MAX_DELAY: 30000
    },
    KEEPALIVE_INTERVAL: 20000,
    DATA_TIMEOUT: 10000
};