// ============================================================================
// pwm.cpp - Реализация управления ШИМ
// ============================================================================

#include "pwm.h"

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ ШИМ
// ============================================================================
void setupPWM() {
  delay(10);
  // 1. Настройка таймера: частота 1 Гц, разрешение 10 бит
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = PWM_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);
  delay(5);

  // 2. Настройка канала: привязываем таймер к пину HEATER_PIN
  ledc_channel_config_t channel_conf = {
    .gpio_num = HEATER_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = PWM_TIMER,
    .duty = 0,  // начинаем с выключенного
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);
  delay(5);
}


















// ============================================================================
// УПРАВЛЕНИЕ МОЩНОСТЬЮ
// ============================================================================
void setHeaterPower(float power) {
  // Защита от дурака: мощность строго 0-100%
  power = constrain(power, 0.0, 100.0);

  // Пересчёт процентов в заполнение ШИМ (0..MAX_DUTY)
  uint32_t duty = (uint32_t)((power / 100.0) * MAX_DUTY);

  ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}

void heaterOff() {
  setHeaterPower(0.0);  // просто вызываем с нулём
}