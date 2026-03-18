// ============================================
// ГРАФИК
// ============================================

function initChart() {
    const ctx = document.getElementById('tempChart').getContext('2d');
    
    state.chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [
                { data: [], borderColor: '#00FF00', borderWidth: 2, tension: 0.2, pointRadius: 0 },
                { data: [], borderColor: '#FFFF00', borderWidth: 2, tension: 0.2, pointRadius: 0 },
                { data: [], borderColor: '#FFA500', borderWidth: 2, tension: 0.2, pointRadius: 0 },
                { data: [], borderColor: '#FFFFFF', borderWidth: 1.5, borderDash: [5, 5], tension: 0, pointRadius: 0 }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: { duration: 0 },
            scales: {
                y: { min: 20, max: 70, grid: { color: '#333' }, ticks: { color: '#ccc', stepSize: 5 } },
                x: { 
                    ticks: { 
                        color: '#ccc',
                        maxTicksLimit: 8,
                        callback: function(val) {
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

function updateChart() {
    if (!state.chart) return;
    
    const pointsPerMinute = Math.floor(60000 / CONFIG.UPDATE_INTERVAL);
    let desiredPoints;
    let availablePoints;
    
    // --- ОПРЕДЕЛЯЕМ РЕЖИМ ОТОБРАЖЕНИЯ ---
    if (state.currentRange === 'half') {
        // Для 1/2 показываем ПОЛОВИНУ буфера (1350 точек)
        // desiredPoints = Math.floor(CONFIG.MAX_POINTS / 2);
        desiredPoints = Math.floor(CONFIG.MAX_POINTS * 2);
		
		
		
		
		availablePoints = Math.min(state.buffer.count, desiredPoints);
        state.chart.options.scales.x.ticks.maxTicksLimit = 12;
        
    } else if (state.currentRange === 'quarter') {
        // Для 1/4 показываем ЧЕТВЕРТЬ буфера (675 точек)
        // desiredPoints = Math.floor(CONFIG.MAX_POINTS / 4);
        desiredPoints = Math.floor(CONFIG.MAX_POINTS * 4);
		
		
		
		availablePoints = Math.min(state.buffer.count, desiredPoints);
        state.chart.options.scales.x.ticks.maxTicksLimit = 16;
        
    } else {
        // Для обычных режимов (10, 30, 45) - обрезаем по времени
        desiredPoints = state.currentRange * pointsPerMinute;
        availablePoints = Math.min(state.buffer.count, desiredPoints);
        
        // Настройка меток для обычных режимов
        if (state.currentRange <= 30) {
            state.chart.options.scales.x.ticks.maxTicksLimit = 10;
        } else {
            state.chart.options.scales.x.ticks.maxTicksLimit = 8;
        }
    }
    
    // Подготавливаем массивы нужной длины
    const labels = new Array(desiredPoints).fill('');
    const datasets = [ [], [], [], [] ];
    
    // Заполняем данными (всегда справа налево)
    for (let i = 0; i < availablePoints; i++) {
        const bufferIdx = (state.buffer.index - availablePoints + i + CONFIG.MAX_POINTS) % CONFIG.MAX_POINTS;
        const chartIdx = desiredPoints - availablePoints + i;
        
        labels[chartIdx] = state.buffer.time[bufferIdx];
        datasets[0][chartIdx] = state.buffer.sensor0[bufferIdx];
        datasets[1][chartIdx] = state.buffer.sensor1[bufferIdx];
        datasets[2][chartIdx] = state.buffer.sensor2[bufferIdx];
        datasets[3][chartIdx] = state.buffer.target[bufferIdx];
    }
    
    // Применяем к графику
    state.chart.data.labels = labels;
    state.chart.data.datasets.forEach((ds, i) => {
        ds.data = datasets[i];
    });
    
    state.chart.update();
    
    if (dom.debugBuffer) {
        dom.debugBuffer.textContent = `${state.buffer.count}/${CONFIG.MAX_POINTS}`;
    }
}