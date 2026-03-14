// ============================================================================
// pwm.cpp - Реализация управления ШИМ
// ============================================================================

#include "pwm.h"

static bool pwm_initialized = false;  // флаг инициализации

// ============================================================================
// ВНУТРЕННЯЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ (ВЫЗЫВАЕТСЯ АВТОМАТИЧЕСКИ)
// ============================================================================
static void init_pwm_once() {
  if (pwm_initialized) return;  // уже инициализировано
  
  Serial.println(F("[PWM] First-time initialization..."));
  
  // Настройка таймера
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = PWM_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  // Настройка канала
  ledc_channel_config_t channel_conf = {
    .gpio_num = HEATER_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = PWM_TIMER,
    .duty = 0,
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);
  
  pwm_initialized = true;
  Serial.println(F("[PWM] Initialized OK"));
}

// ============================================================================
// "ПУСТАЯ" ФУНКЦИЯ ДЛЯ СОВМЕСТИМОСТИ
// ============================================================================
void setupPWM() {
  // Ничего не делаем при старте!
  // Инициализация произойдёт при первом включении нагрева
  Serial.println(F("[PWM] setupPWM() skipped - will init on first use"));
}

// ============================================================================
// УПРАВЛЕНИЕ МОЩНОСТЬЮ
// ============================================================================
void setHeaterPower(float power) {
  init_pwm_once();  // инициализация при первом вызове
  
  power = constrain(power, 0.0, 100.0);
  uint32_t duty = (uint32_t)((power / 100.0) * MAX_DUTY);
  
  ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}

void heaterOff() {
  setHeaterPower(0.0);
}