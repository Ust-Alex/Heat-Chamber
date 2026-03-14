// ============================================================================
// pwm.cpp - Реализация управления ШИМ
// ============================================================================
// Проект: Heat-Chamber
// Особенности:
// - Атомарная инициализация через критическую секцию FreeRTOS
// - Инициализация при первом использовании
// - Защита от повторной инициализации
// ============================================================================

#include "pwm.h"
#include <driver/ledc.h>

static bool pwm_initialized = false;  // флаг инициализации
static portMUX_TYPE pwm_mux = portMUX_INITIALIZER_UNLOCKED;  // мьютекс для критической секции

// ============================================================================
// ВНУТРЕННЯЯ ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ (ВЫЗЫВАЕТСЯ АВТОМАТИЧЕСКИ)
// ============================================================================
static void init_pwm_once() {
  if (pwm_initialized) return;  // уже инициализировано
  
  Serial.println(F("[PWM] First-time initialization..."));
  
  // ==========================================================================
  // КРИТИЧЕСКАЯ СЕКЦИЯ - гарантирует атомарность инициализации
  // Отключает планировщик и прерывания на время настройки LEDC
  // ==========================================================================
  taskENTER_CRITICAL(&pwm_mux);
  
  // 1. Настройка таймера ШИМ
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = PWM_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  // 2. Настройка канала ШИМ
  ledc_channel_config_t channel_conf = {
    .gpio_num = HEATER_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = PWM_TIMER,
    .duty = 0,        // начинаем с выключенного
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);
  
  // Выход из критической секции
  taskEXIT_CRITICAL(&pwm_mux);
  
  // Маленькая задержка для стабилизации аппаратуры
  vTaskDelay(pdMS_TO_TICKS(10));
  
  pwm_initialized = true;
  Serial.println(F("[PWM] Initialized OK"));
}

// ============================================================================
// "ПУСТАЯ" ФУНКЦИЯ ДЛЯ СОВМЕСТИМОСТИ
// ============================================================================
void setupPWM() {
  Serial.println(F("[PWM] setupPWM() skipped - will init on first use"));
}

// ============================================================================
// УПРАВЛЕНИЕ МОЩНОСТЬЮ
// ============================================================================
void setHeaterPower(float power) {
  // Инициализация при первом реальном вызове
  init_pwm_once();
  
  // Защита от дурака: мощность строго 0-100%
  power = constrain(power, 0.0, 100.0);

  // Пересчёт процентов в заполнение ШИМ (0..MAX_DUTY)
  uint32_t duty = (uint32_t)((power / 100.0) * MAX_DUTY);

  // Установка заполнения ШИМ
  ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
  
  // Отладка при изменении мощности
  static float lastPower = -1.0;
  if (abs(power - lastPower) > 0.1) {
    Serial.printf("[PWM] Power: %.1f%% (duty: %u)\n", power, duty);
    lastPower = power;
  }
}

void heaterOff() {
  setHeaterPower(0.0);  // просто вызываем с нулём
}

// ============================================================================
// pwm.h - Управление ШИМ (нагревателем)
// ============================================================================
#ifndef PWM_H
#define PWM_H

#include <Arduino.h>
#include "config.h"

// Инициализация ШИМ: настраивает таймер и канал LEDC для управления нагревателем
void setupPWM();

// Установка мощности нагревателя в процентах (0.0 .. 100.0)
// Автоматически пересчитывает проценты в заполнение ШИМ с учётом MAX_DUTY
void setHeaterPower(float power);

// Быстрое выключение нагревателя (вызывает setHeaterPower(0))
void heaterOff();

#endif // PWM_H