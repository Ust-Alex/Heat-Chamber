// ============================================
// ЗАПУСК
// ============================================
window.addEventListener('load', () => {
    initChart();
    setupControls();
    connectWebSocket();
    
    setInterval(() => checkDataTimeout(), 5000);
    
    setInterval(() => {
        if (dom.ntpTime && (!state.ntpTime || state.ntpTime === '00:00:00')) {
            const now = new Date();
            const timeStr = `${now.getHours().toString().padStart(2,'0')}:${now.getMinutes().toString().padStart(2,'0')}:${now.getSeconds().toString().padStart(2,'0')}`;
            dom.ntpTime.textContent = timeStr;
        }
    }, 1000);
    
    document.querySelectorAll('.scale-btn').forEach(btn => {
        if (btn.dataset.range === '45') btn.classList.add('active');
    });
    
    updateUI();
    updateWiFiStatus(false);
});