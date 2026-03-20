#include "task_sensors.h"
#include "config.h"
#include "sensors_core.h"
#include "web_server.h"  // для addHistoryPoint

// Внешние переменные из проекта
extern DallasTemperature sensors;
extern DeviceAddress sensorAddresses[MAX_SENSORS];
extern uint8_t sensorCount;
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern unsigned long lastTempUpdate;
extern unsigned long lastHistorySave;

void taskSensors(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[SENSORS] Task started"));
#endif

  unsigned long lastRead = 0;
  unsigned long lastHistorySave = 0;

  while(1) {
    unsigned long now = millis();

    if (now - lastRead >= TEMP_UPDATE_INTERVAL) {
      lastRead = now;

      requestTemperatures(&sensors);

      for (uint8_t i = 0; i < sensorCount; i++) {
        sensorTemps[i] = readTemperature(&sensors, sensorAddresses, i);
      }

      if (now - lastHistorySave >= HISTORY_INTERVAL) {
        addHistoryPoint(sensorTemps[0], sensorTemps[1], sensorTemps[2], targetTemp);
        lastHistorySave = now;
      }

      // Отладка датчиков
      if (DEBUG_SENSORS) {
        static unsigned long lastDebug = 0;
        if (now - lastDebug >= 10000) {
          lastDebug = now;
          Serial.printf("[SENSORS] T0: %.2f, T1: %.2f, T2: %.2f\n",
                        sensorTemps[0], sensorTemps[1], sensorTemps[2]);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}