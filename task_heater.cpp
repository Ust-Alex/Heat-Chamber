// ============================================================================
// task_heater.cpp - Задача FreeRTOS для управления нагревателем
// ============================================================================
// Проект: Heat-Chamber (термический реформинг)
// Платформа: ESP32
// ============================================================================

#include "task_heater.h"
#include "config.h"
#include "pwm.h"
#include "settings.h"
#include <uPID.h>

// ============================================================================
// ВНЕШНИЕ ФУНКЦИИ ДЛЯ WEBSERIAL ОТЛАДКИ
// ============================================================================
extern void webSerialPrintf(const char* format, ...);

// ============================================================================
// КОНСТАНТЫ НАСТРОЕК
// ============================================================================

// Порог ограничения мощности (градусы до уставки)
// Теперь anti-windup ограничивает мощность только при ошибке < 0.5°C
#define INTEGRAL_LIMIT_THRESHOLD 0.5

// Аварийный порог перегрева (градусы выше уставки)
#define OVERHEAT_EMERGENCY_LIMIT 2.0

// Порог включения форсажа (градусы ниже уставки)
#define FORCE_MODE_ENABLE_DIFF 2.0

// Порог выключения форсажа (градусы ниже уставки)
#define FORCE_MODE_HOLD_DIFF 5.0

// Желаемый выход ПИД-регулятора после выхода из форсажа (0-4096)
#define DESIRED_PID_OUTPUT 3277.0

// Максимальное значение ШИМ (из config.h)
#define MAX_DUTY 8191

// ============================================================================
// НОВАЯ КОНСТАНТА: МИНИМАЛЬНАЯ МОЩНОСТЬ
// ============================================================================
// Для предотвращения глубокого провала температуры при остывании
#define MIN_POWER 40.0   // Минимальная мощность в процентах

// Порог включения минимальной мощности (градусы ниже уставки)
#define MIN_POWER_TRESHOLD 0.5

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern bool systemState;
extern uPID PIDregulator;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
unsigned long heaterSeconds = 0;

// ============================================================================
// ЗАДАЧА УПРАВЛЕНИЯ НАГРЕВАТЕЛЕМ
// ============================================================================
void taskHeaterControl(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[HEATER] Task started"));
#endif

  unsigned long lastPID = 0;
  bool lastSystemState = false;
  bool forceModeActive = false;
  bool justExitedForce = false;

  static unsigned long lastTimeSave = 0;
  const unsigned long SAVE_INTERVAL = 300000;

  bool integralResetDone = false;
  
  float lastError = 0;
  float lastPower = 0;

  while (1) {
    unsigned long now = millis();

    // --- Обработка включения системы ---
    if (systemState && !lastSystemState) {
      if (sensorTemps[0] < targetTemp - FORCE_MODE_ENABLE_DIFF) {
        forceModeActive = true;
        justExitedForce = false;
        integralResetDone = false;
        webSerialPrintf("[HEATER] FORCE MODE ACTIVATED (100%%, current: %.1f, target: %.1f)\n",
                      sensorTemps[0], targetTemp);
      } else {
        forceModeActive = false;
        webSerialPrintf("[HEATER] Normal mode (current: %.1f, target: %.1f)\n",
                      sensorTemps[0], targetTemp);
      }
    }
    lastSystemState = systemState;

    // --- Подсчёт времени нагрева ---
    static unsigned long lastHeaterSecond = 0;
    if (systemState && millis() - lastHeaterSecond >= 1000) {
      heaterSeconds++;
      lastHeaterSecond = millis();
    }

    // --- Сохранение времени нагрева ---
    if (millis() - lastTimeSave >= SAVE_INTERVAL) {
      lastTimeSave = millis();
      saveHeaterTime();
    }

    // --- ПИД-регулирование ---
    if (now - lastPID >= PID_INTERVAL) {
      lastPID = now;

      if (systemState && !isnan(sensorTemps[0]) && sensorTemps[0] > 0) {

        // ============================================================
        // АВАРИЙНАЯ ЗАЩИТА ОТ ПЕРЕГРЕВА
        // ============================================================
        if (sensorTemps[0] >= targetTemp + OVERHEAT_EMERGENCY_LIMIT) {
          setHeaterPower(0.0);
          forceModeActive = false;
          PIDregulator.integral = 0.0;
          integralResetDone = false;
          webSerialPrintf("[HEATER] EMERGENCY: %.1f >= %.1f, OFF, INTEGRAL RESET\n",
                            sensorTemps[0], targetTemp + OVERHEAT_EMERGENCY_LIMIT);
        }
        // ============================================================
        // РЕЖИМ ФОРСАЖА
        // ============================================================
        else if (forceModeActive) {
          if (sensorTemps[0] < targetTemp - FORCE_MODE_HOLD_DIFF) {
            setHeaterPower(100.0);
            if (DEBUG_HEATER) {
              static unsigned long lastForceDebug = 0;
              if (now - lastForceDebug >= 5000) {
                lastForceDebug = now;
                webSerialPrintf("[HEATER] FORCE: %.1f -> %.1f, 100%%\n",
                              sensorTemps[0], targetTemp);
              }
            }
          } else {
            forceModeActive = false;
            justExitedForce = true;
            integralResetDone = false;
            webSerialPrintf("[HEATER] FORCE MODE OFF at %.1f°C\n", sensorTemps[0]);
          }
        }

        // ============================================================
        // НОРМАЛЬНЫЙ РЕЖИМ (ПИД-РЕГУЛЯТОР)
        // ============================================================
        if (!forceModeActive) {

          PIDregulator.setpoint = targetTemp;

          // --- Подсев интеграла после выхода из форсажа ---
          if (justExitedForce) {
            float error = targetTemp - sensorTemps[0];
            float desiredOutput = DESIRED_PID_OUTPUT;

            if (PIDregulator.Ki > 0) {
              float desiredIntegral = (desiredOutput - PIDregulator.Kp * error) / PIDregulator.Ki;
              float maxIntegral = MAX_DUTY / PIDregulator.Ki;

              if (desiredIntegral < 0) desiredIntegral = 0;
              if (desiredIntegral > maxIntegral) desiredIntegral = maxIntegral;

              PIDregulator.integral = desiredIntegral;

              webSerialPrintf("[HEATER] PID seeded: int=%.1f, err=%.1f\n",
                              desiredIntegral, error);
            }
            justExitedForce = false;
          }

          // ============================================================
          // СБРОС ИНТЕГРАЛА ПРИ ДОСТИЖЕНИИ УСТАВКИ
          // ============================================================
          if (sensorTemps[0] >= targetTemp) {
            if (!integralResetDone) {
              PIDregulator.integral = 0.0;
              integralResetDone = true;
              webSerialPrintf("[HEATER] INTEGRAL RESET at target (%.2f°C)\n", sensorTemps[0]);
            }
          } else {
            integralResetDone = false;
          }

          // ============================================================
          // ВЫЧИСЛЕНИЕ ОШИБКИ
          // ============================================================
          float error = targetTemp - sensorTemps[0];
          
          // ============================================================
          // ПРИНУДИТЕЛЬНОЕ НАКОПЛЕНИЕ ИНТЕГРАЛА
          // ============================================================
          if (error > 0) {
            float dt = PID_INTERVAL / 1000.0;
            PIDregulator.integral += PIDregulator.Ki * error * dt;
            
            float maxIntegral = MAX_DUTY / PIDregulator.Ki;
            if (PIDregulator.integral > maxIntegral) {
              PIDregulator.integral = maxIntegral;
            }
            if (PIDregulator.integral < 0) {
              PIDregulator.integral = 0;
            }
          }
          
          // ============================================================
          // ВЫЧИСЛЕНИЕ СОСТАВЛЯЮЩИХ ПИД
          // ============================================================
          float pPart = PIDregulator.Kp * error;
          float iPart = PIDregulator.integral * PIDregulator.Ki;
          float dPart = PIDregulator.Kd * (error - lastError) / (PID_INTERVAL / 1000.0);
          lastError = error;
          
          float pidOut = pPart + iPart + dPart;
          if (pidOut > MAX_DUTY) pidOut = MAX_DUTY;
          if (pidOut < 0.0) pidOut = 0.0;
          
          float power = (pidOut / MAX_DUTY) * 100.0;

          // ============================================================
          // ANTI-WINDUP: ОГРАНИЧЕНИЕ МОЩНОСТИ ПРИ ПРИБЛИЖЕНИИ
          // Порог изменён с 3.0 на 0.5°C
          // ============================================================
          float errorAbs = fabs(error);
          
          if (errorAbs < INTEGRAL_LIMIT_THRESHOLD && error > 0) {
            float maxPower = (errorAbs / INTEGRAL_LIMIT_THRESHOLD) * 100.0;
            if (maxPower < 0) maxPower = 0;
            if (maxPower > 100) maxPower = 100;
            
            if (power > maxPower) {
              power = maxPower;
              if (DEBUG_HEATER) {
                static unsigned long lastAntiWindupDebug = 0;
                if (now - lastAntiWindupDebug >= 5000) {
                  lastAntiWindupDebug = now;
                  webSerialPrintf("[HEATER] Anti-windup: power limited to %.1f%% (error: %.2f°C)\n", 
                                power, error);
                }
              }
            }
          }
          
          // ============================================================
          // НОВОЕ: МИНИМАЛЬНАЯ МОЩНОСТЬ ДЛЯ ПРЕДОТВРАЩЕНИЯ ПРОВАЛА
          // ============================================================
          // Если система включена и температура ниже уставки более чем
          // на MIN_POWER_TRESHOLD, устанавливаем минимальную мощность MIN_POWER.
          // Это предотвращает глубокое остывание после сброса интеграла.
          // ============================================================
          if (systemState && sensorTemps[0] < targetTemp - MIN_POWER_TRESHOLD) {
            if (power < MIN_POWER) {
              power = MIN_POWER;
              if (DEBUG_HEATER) {
                static unsigned long lastMinPowerDebug = 0;
                if (now - lastMinPowerDebug >= 10000) {
                  lastMinPowerDebug = now;
                  webSerialPrintf("[HEATER] Min power applied: %.1f%% (temp: %.1f°C, target: %.1f°C)\n", 
                                MIN_POWER, sensorTemps[0], targetTemp);
                }
              }
            }
          }
          
          // ============================================================
          // ОТЛАДКА: ВЫВОД В WEBSERIAL (каждые 10 секунд)
          // ============================================================
          static unsigned long lastDebugTime = 0;
          if (now - lastDebugTime >= 10000) {
            lastDebugTime = now;
            
            float maxIntegralCurrent = MAX_DUTY / PIDregulator.Ki;
            
            webSerialPrintf("\n=== PID DEBUG ===\n");
            webSerialPrintf("T=%.2f°C | Target=%.1f°C | Error=%.2f°C\n", 
                            sensorTemps[0], targetTemp, error);
            webSerialPrintf("Power=%.1f%% | Limit=%.1f%%\n", power, 
                            (errorAbs < INTEGRAL_LIMIT_THRESHOLD && error > 0) ? 
                            (errorAbs / INTEGRAL_LIMIT_THRESHOLD) * 100.0 : 100.0);
            webSerialPrintf("P=%.2f | I=%.2f | D=%.2f\n", pPart, iPart, dPart);
            webSerialPrintf("Integral=%.1f / %.1f | Ki=%.2f\n", 
                            PIDregulator.integral, maxIntegralCurrent, PIDregulator.Ki);
            webSerialPrintf("ForceMode=%d | AntiWindupActive=%d\n", 
                            forceModeActive, (errorAbs < INTEGRAL_LIMIT_THRESHOLD && error > 0));
            webSerialPrintf("================\n");
          }

          // Ограничиваем мощность
          if (power > 100.0) power = 100.0;
          if (power < 0.0) power = 0.0;

          setHeaterPower(power);
        }
      } else {
        heaterOff();
        forceModeActive = false;
        justExitedForce = false;
        integralResetDone = false;
        lastError = 0;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}