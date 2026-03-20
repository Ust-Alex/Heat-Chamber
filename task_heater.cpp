#include "task_heater.h"
#include "config.h"
#include "pwm.h"
#include "settings.h"
#include <uPID.h>  // ← ДОБАВЛЕНО

// Внешние переменные из проекта
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern bool systemState;
extern uPID PIDregulator;  // ← теперь компилятор знает, что такое uPID

// Определение глобальной переменной (ТОЛЬКО ЗДЕСЬ!)
unsigned long heaterSeconds = 0;

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

  while(1) {
    unsigned long now = millis();

    if (systemState && !lastSystemState) {
      if (sensorTemps[0] < targetTemp - 2.0) {
        forceModeActive = true;
        justExitedForce = false;
        Serial.printf("[HEATER] FORCE MODE ACTIVATED (100%%, current: %.1f, target: %.1f)\n",
                      sensorTemps[0], targetTemp);
      } else {
        forceModeActive = false;
        Serial.printf("[HEATER] Normal mode (current: %.1f, target: %.1f)\n",
                      sensorTemps[0], targetTemp);
      }
    }
    lastSystemState = systemState;

    static unsigned long lastHeaterSecond = 0;
    if (systemState && millis() - lastHeaterSecond >= 1000) {
      heaterSeconds++;
      lastHeaterSecond = millis();
    }

    if (millis() - lastTimeSave >= SAVE_INTERVAL) {
      lastTimeSave = millis();
      saveHeaterTime();
    }

    if (now - lastPID >= PID_INTERVAL) {
      lastPID = now;

      if (systemState && !isnan(sensorTemps[0]) && sensorTemps[0] > 0) {

        if (sensorTemps[0] >= targetTemp) {
          setHeaterPower(0.0);
          forceModeActive = false;

          if (DEBUG_HEATER) {
            static unsigned long lastDebug = 0;
            if (now - lastDebug >= 10000) {
              lastDebug = now;
              Serial.printf("[HEATER] Target reached: %.1f >= %.1f, heater OFF\n",
                            sensorTemps[0], targetTemp);
            }
          }
        }
        else if (forceModeActive) {
          if (sensorTemps[0] < targetTemp - 5.0) {
            setHeaterPower(100.0);

            if (DEBUG_HEATER) {
              static unsigned long lastForceDebug = 0;
              if (now - lastForceDebug >= 5000) {
                lastForceDebug = now;
                Serial.printf("[HEATER] FORCE: %.1f -> %.1f, 100%%\n",
                              sensorTemps[0], targetTemp);
              }
            }
          } else {
            forceModeActive = false;
            justExitedForce = true;
            Serial.printf("[HEATER] FORCE MODE OFF at %.1f°C\n", sensorTemps[0]);
          }
        }

        if (!forceModeActive && sensorTemps[0] < targetTemp) {

          PIDregulator.setpoint = targetTemp;

          if (justExitedForce) {
            float error = targetTemp - sensorTemps[0];
            float desiredOutput = 3277.0;

            if (PIDregulator.Ki > 0) {
              float desiredIntegral = (desiredOutput - PIDregulator.Kp * error) / PIDregulator.Ki;
              float maxIntegral = 4096.0 / PIDregulator.Ki;

              if (desiredIntegral < 0) desiredIntegral = 0;
              if (desiredIntegral > maxIntegral) desiredIntegral = maxIntegral;

              PIDregulator.integral = desiredIntegral;

              if (DEBUG_HEATER) {
                Serial.printf("[HEATER] PID seeded: int=%.1f, err=%.1f\n",
                              desiredIntegral, error);
              }
            }
            justExitedForce = false;
          }

          float pidOut = PIDregulator.compute(sensorTemps[0]);
          float power = (pidOut / 4096.0) * 100.0;

          if (power > 100.0) power = 100.0;
          if (power < 0.0) power = 0.0;

          setHeaterPower(power);

          if (DEBUG_HEATER) {
            static unsigned long lastDebug = 0;
            if (now - lastDebug >= 30000) {
              lastDebug = now;
              Serial.printf("[HEATER] PID: %.1f->%.1f, P=%.1f%%\n",
                            sensorTemps[0], targetTemp, power);
            }
          }
        }
      } else {
        heaterOff();
        forceModeActive = false;
        justExitedForce = false;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}