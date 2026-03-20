// ============================================================================
// rtos_tasks.cpp - Реализация всех задач FreeRTOS
// ============================================================================

#include "rtos_tasks.h"
#include "web_server.h"
#include "serial_commands.h"
#include "config.h"
#include "settings.h"  // добавлено для saveHeaterTime()

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ
// ============================================================================
extern U8G2_ST7920_128X64_F_HW_SPI u8g2;
extern OneWire oneWire;
extern DallasTemperature sensors;
extern DeviceAddress sensorAddresses[MAX_SENSORS];
extern uint8_t sensorCount;
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern bool systemState;
extern unsigned long startMillis;
extern int targetHistory[GRAPH_WIDTH];
extern float sensorHistory[MAX_SENSORS][GRAPH_WIDTH];
extern uPID PIDregulator;

extern unsigned long lastGraphShift;
extern unsigned long lastGraphDraw;
extern unsigned long lastTempUpdate;
extern unsigned long lastPIDUpdate;

extern float currentPower;

// ============================================================================
// ДЕСКРИПТОРЫ ЗАДАЧ
// ============================================================================
extern TaskHandle_t buttonsTaskHandle;
extern TaskHandle_t webTaskHandle;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ДЛЯ ТАЙМЕРА НАГРЕВА
// ============================================================================
unsigned long heaterSeconds = 0;  // общее время нагрева в секундах

// ============================================================================
// ПРОТОТИП задачи для Serial
// ============================================================================
void taskSerialCommands(void* pvParameters);

// ============================================================================
// ЗАДАЧА: ДИСПЛЕЙ (ядро 0, приоритет 2)
// ============================================================================
void taskDisplay(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[DISPLAY] Task started on core 0, priority 2"));
#endif
  
  while(1) {
    unsigned long now = millis();
    
    if (now - lastGraphShift >= GRAPH_SHIFT_INTERVAL_MS) {
      lastGraphShift = now;
      shiftGraphsLeft(targetHistory, sensorHistory, targetTemp, sensorTemps, MAX_SENSORS);
    }
    
    if (now - lastGraphDraw >= GRAPH_DRAW_INTERVAL_MS) {
      lastGraphDraw = now;
      drawInterface(
        &u8g2, startMillis, targetTemp, systemState,
        sensorTemps, sensorCount,
        targetHistory, sensorHistory, MAX_SENSORS
      );
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// ЗАДАЧА: ОПРОС ДАТЧИКОВ (ядро 1, приоритет 3)
// ============================================================================
void taskSensors(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[SENSORS] Task started on core 1, priority 3"));
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
      
      // Отладка датчиков по отдельному флагу
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

// ============================================================================
// ЗАДАЧА: УПРАВЛЕНИЕ НАГРЕВОМ (ПИД) (ядро 1, приоритет 3)
// ============================================================================
void taskHeaterControl(void* pvParameters) {
#if DEBUG_SERIAL
  Serial.println(F("[HEATER] Task started on core 1, priority 3"));
#endif
  
  unsigned long lastPID = 0;
  bool lastSystemState = false;        // Для отслеживания момента включения
  bool forceModeActive = false;        // Флаг форсированного режима
  bool justExitedForce = false;        // Флаг только что завершённого форсажа
  
  // ===== ПЕРИОДИЧЕСКОЕ СОХРАНЕНИЕ ВРЕМЕНИ НАГРЕВА =====
  static unsigned long lastTimeSave = 0;
  const unsigned long SAVE_INTERVAL = 300000;  // 5 минут в миллисекундах
  
  while(1) {
    unsigned long now = millis();
    
    // ===== ОТСЛЕЖИВАНИЕ ВКЛЮЧЕНИЯ НАГРЕВА =====
    if (systemState && !lastSystemState) {
      // Нагрев только что включили — активируем форсаж (но только если температура ниже уставки)
      if (sensorTemps[0] < targetTemp - 2.0) {  // Небольшой гистерезис
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

    // ===== ОБНОВЛЕНИЕ СЧЁТЧИКА ВРЕМЕНИ НАГРЕВА =====
    static unsigned long lastHeaterSecond = 0;
    if (systemState && millis() - lastHeaterSecond >= 1000) {
      heaterSeconds++;
      lastHeaterSecond = millis();
    }
    
    // ===== ПЕРИОДИЧЕСКОЕ СОХРАНЕНИЕ ВРЕМЕНИ НАГРЕВА =====
    if (millis() - lastTimeSave >= SAVE_INTERVAL) {
      lastTimeSave = millis();
      saveHeaterTime();  // сохраняем текущее значение heaterSeconds
    }
    
    if (now - lastPID >= PID_INTERVAL) {
      lastPID = now;
      
      if (systemState && !isnan(sensorTemps[0]) && sensorTemps[0] > 0) {
        
        // ===== ПРОВЕРКА: НЕ ГРЕТЬ, ЕСЛИ УЖЕ ГОРЯЧО =====
        if (sensorTemps[0] >= targetTemp) {
          // Достигли или превысили уставку — выключаем нагрев
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
        // ===== РЕЖИМ ФОРСИРОВАННОГО СТАРТА =====
        else if (forceModeActive) {
          // Проверяем, пора ли выключать форсаж
          if (sensorTemps[0] < targetTemp - 5.0) {
            // Всё ещё форсируем
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
            // Достигли диапазона — выключаем форсаж
            forceModeActive = false;
            justExitedForce = true;
            Serial.printf("[HEATER] FORCE MODE OFF at %.1f°C\n", sensorTemps[0]);
          }
        }
        
        // ===== ОБЫЧНЫЙ РЕЖИМ ПИД =====
        // Выполняется, если форсаж выключен И температура ниже уставки
        if (!forceModeActive && sensorTemps[0] < targetTemp) {
          
          PIDregulator.setpoint = targetTemp;
          
          // ===== ПЛАВНАЯ ПЕРЕДАЧА УПРАВЛЕНИЯ ПОСЛЕ ФОРСАЖА =====
          if (justExitedForce) {
            float error = targetTemp - sensorTemps[0];
            float desiredOutput = 3277.0;  // 80% мощности
            
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
          
          // Вычисляем ПИД
          float pidOut = PIDregulator.compute(sensorTemps[0]);
          float power = (pidOut / 4096.0) * 100.0;
          
          if (power > 100.0) power = 100.0;
          if (power < 0.0) power = 0.0;
          
          setHeaterPower(power);
          
          // Отладка
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
        // Нагрев выключен или датчик неисправен
        heaterOff();
        forceModeActive = false;
        justExitedForce = false;
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ============================================================================
// ФУНКЦИЯ СОЗДАНИЯ ВСЕХ ЗАДАЧ
// ============================================================================
void createAllTasks(
  GButton* btnUp, 
  GButton* btnDown, 
  GButton* btnPower,
  float* targetTempPtr,
  bool* systemStatePtr,
  unsigned long* startMillisPtr
) {
  Serial.println(F("\n[RTOS] Creating all tasks..."));

  // ЗАДАЧА 1: КНОПКИ
  Serial.print(F("   Button task ... "));
  buttonsTaskHandle = createButtonsTask(
    btnUp, btnDown, btnPower,
    targetTempPtr, systemStatePtr, startMillisPtr,
    MAX_TEMP,
    [](float newTarget) { PIDregulator.setpoint = newTarget; });
  Serial.println(F("OK"));

  // ЗАДАЧА 2: ДИСПЛЕЙ
  Serial.print(F("   Display task ... "));
  xTaskCreatePinnedToCore(taskDisplay, "displayTask", 4096, NULL, 2, NULL, 0);
  Serial.println(F("OK"));

  // ЗАДАЧА 3: WiFi/ВЕБ
  Serial.print(F("   WiFi task ... "));
  xTaskCreatePinnedToCore(taskWebServer, "webTask", 8192, NULL, 3, &webTaskHandle, 1);
  Serial.println(F("OK"));

  // ЗАДАЧА 4: ДАТЧИКИ
  Serial.print(F("   Sensor task ... "));
  xTaskCreatePinnedToCore(taskSensors, "sensorTask", 4096, NULL, 3, NULL, 1);
  Serial.println(F("OK"));

  // ЗАДАЧА 5: НАГРЕВ
  Serial.print(F("   Heater task ... "));
  xTaskCreatePinnedToCore(taskHeaterControl, "heaterTask", 4096, NULL, 3, NULL, 1);
  Serial.println(F("OK"));

  // ЗАДАЧА 6: SERIAL
  Serial.print(F("   Serial task ... "));
  xTaskCreatePinnedToCore(taskSerialCommands, "serialTask", 4096, NULL, 1, NULL, 0);
  Serial.println(F("OK"));

  // ЗАДАЧА 7: WiFi KEEPALIVE (если файл добавлен)
  #ifdef WIFI_KEEPALIVE_H
  extern TaskHandle_t wifiKeepaliveTaskHandle;
  extern void taskWiFiKeepalive(void*);
  Serial.print(F("   WiFi keepalive task ... "));
  xTaskCreatePinnedToCore(taskWiFiKeepalive, "wifiKeepalive", 2048, NULL, 1, &wifiKeepaliveTaskHandle, 0);
  Serial.println(F("OK"));
  #endif

  Serial.println(F("[RTOS] All tasks created"));
}