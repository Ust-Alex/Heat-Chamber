// ============================================================================
// pwm.cpp - Реализация управления ШИМ
// ============================================================================
// Проект: Heat-Chamber
// Особенности:
// - Явная инициализация в setup()
// - Атомарные операции через критическую секцию
// - Защита от повторной инициализации
// ============================================================================

#include "pwm.h"
#include <driver/ledc.h>

static bool pwm_initialized = false;
static portMUX_TYPE pwm_mux = portMUX_INITIALIZER_UNLOCKED;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ШИМ (ВЫЗЫВАТЬ В SETUP!)
// ============================================================================
void setupPWM() {
  if (pwm_initialized) {
    Serial.println(F("[PWM] Already initialized, skipping"));
    return;
  }
  
  Serial.println(F("[PWM] Initializing..."));
  
  // Критическая секция для атомарной настройки
  taskENTER_CRITICAL(&pwm_mux);
  
  // 1. Настройка таймера ШИМ
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_HIGH_SPEED_MODE,      // HIGH_SPEED надёжнее
    .duty_resolution = LEDC_TIMER_13_BIT,    // 0-8191
    .timer_num = LEDC_TIMER_0,
    .freq_hz = 5000,                          // 5 кГц
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  // 2. Настройка канала ШИМ
  ledc_channel_config_t channel_conf = {
    .gpio_num = HEATER_PIN,
    .speed_mode = LEDC_HIGH_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = LEDC_TIMER_0,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);
  
  taskEXIT_CRITICAL(&pwm_mux);
  
  // Небольшая задержка для стабилизации
  delay(10);
  
  pwm_initialized = true;
  Serial.println(F("[PWM] Initialized OK"));
}

// ============================================================================
// УПРАВЛЕНИЕ МОЩНОСТЬЮ
// ============================================================================
void setHeaterPower(float power) {
  if (!pwm_initialized) {
    Serial.println(F("[PWM] ERROR: Not initialized! Call setupPWM() first."));
    return;
  }
  
  // Защита от дурака: 0-100%
  power = constrain(power, 0.0, 100.0);

  // Пересчёт процентов в заполнение ШИМ (0..8191)
  uint32_t duty = (uint32_t)((power / 100.0) * 8191);

  // Критическая секция для установки
  taskENTER_CRITICAL(&pwm_mux);
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
  taskEXIT_CRITICAL(&pwm_mux);
  
  // Отладка при изменении мощности
  static float lastPower = -1.0;
  if (abs(power - lastPower) > 0.1) {
    Serial.printf("[PWM] Power: %.1f%% (duty: %u)\n", power, duty);
    lastPower = power;
  }
}

void heaterOff() {
  setHeaterPower(0.0);
}