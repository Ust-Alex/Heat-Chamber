// ============================================================================
// rtos_tasks.cpp - Реализация всех задач FreeRTOS
// ============================================================================

#include "rtos_tasks.h"
#include "web_server.h"
#include "serial_commands.h"
#include "config.h"

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
  
  while(1) {
    unsigned long now = millis();
    
    // ===== ОТСЛЕЖИВАНИЕ ВКЛЮЧЕНИЯ НАГРЕВА =====
    if (systemState && !lastSystemState) {
      // Нагрев только что включили — активируем форсаж
      forceModeActive = true;
      justExitedForce = false;          // Сбрасываем флаг выхода
      Serial.println(F("[HEATER] FORCE MODE ACTIVATED (100% power until near target)"));
    }
    lastSystemState = systemState;
    
    if (now - lastPID >= PID_INTERVAL) {
      lastPID = now;
      
      if (systemState && !isnan(sensorTemps[0]) && sensorTemps[0] > 0) {
        
        // ===== РЕЖИМ ФОРСИРОВАННОГО СТАРТА =====
        if (forceModeActive) {
          // Проверяем, пора ли выключать форсаж (когда температура приблизится к уставке)
          if (sensorTemps[0] < targetTemp - 5.0) {
            // Всё ещё форсируем — подаём 100% мощности
            setHeaterPower(100.0);
            
            // Отладка форсажа
            if (DEBUG_HEATER) {
              static unsigned long lastForceDebug = 0;
              if (now - lastForceDebug >= 10000) {
                lastForceDebug = now;
                Serial.printf("[HEATER] FORCE: Current: %.2f, Target: %.1f, Power: 100%%\n", 
                              sensorTemps[0], targetTemp);
              }
            }
          } else {
            // Достигли целевого диапазона — выключаем форсаж
            forceModeActive = false;
            justExitedForce = true;      // Запоминаем, что только что вышли из форсажа
            Serial.printf("[HEATER] FORCE MODE OFF at %.2f°C, handing over to PID\n", sensorTemps[0]);
          }
        }
        
        // ===== ОБЫЧНЫЙ РЕЖИМ ПИД =====
        // Этот блок выполняется, если форсаж выключен
        if (!forceModeActive) {
          // Синхронизируем уставку
          PIDregulator.setpoint = targetTemp;
          
          // ===== ПЛАВНАЯ ПЕРЕДАЧА УПРАВЛЕНИЯ ПОСЛЕ ФОРСАЖА =====
          if (justExitedForce) {
            // Рассчитываем, какой интеграл нужен, чтобы ПИД продолжил с ~80% мощности
            float error = targetTemp - sensorTemps[0];  // Ошибка сейчас (около 5°C)
            
            // Желаемый выход ПИД в условных единицах (хотим около 80% мощности)
            // 80% от 4096 = 3277
            float desiredOutput = 3277.0;
            
            // Формула ПИД: output = Kp * error + Ki * integral + Kd * deriv
            // Допускаем, что deriv = 0 (температура растёт равномерно)
            // Тогда integral = (desiredOutput - Kp * error) / Ki
            
            if (PIDregulator.Ki > 0) {  // Защита от деления на ноль
              float desiredIntegral = (desiredOutput - PIDregulator.Kp * error) / PIDregulator.Ki;
              
              // Ограничиваем интеграл разумными пределами
              if (desiredIntegral < 0) desiredIntegral = 0;
              
              // Максимальный интеграл, соответствующий 100% мощности при error=0
              float maxIntegral = 4096.0 / PIDregulator.Ki;  // При Ki=0.08 это 51200
              if (desiredIntegral > maxIntegral) desiredIntegral = maxIntegral;
              
              PIDregulator.integral = desiredIntegral;
              
              if (DEBUG_HEATER) {
                Serial.printf("[HEATER] PID integral seeded to %.1f for smooth handover (error=%.1f)\n", 
                              desiredIntegral, error);
              }
            }
            justExitedForce = false;  // Сбрасываем флаг
          }
          
          // Вычисляем ПИД
          float pidOut = PIDregulator.compute(sensorTemps[0]);
          float power = (pidOut / 4096.0) * 100.0;
          
          // Ограничиваем мощность (на всякий случай)
          if (power > 100.0) power = 100.0;
          if (power < 0.0) power = 0.0;
          
          setHeaterPower(power);
          
          // Отладка нагрева
          if (DEBUG_HEATER) {
            static unsigned long lastDebug = 0;
            if (now - lastDebug >= 30000) {
              lastDebug = now;
              Serial.printf("[HEATER] Target: %.1f, Current: %.2f, Power: %.1f%% (PID out: %.1f)\n", 
                            targetTemp, sensorTemps[0], power, pidOut);
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
  // Эти сообщения важны при старте, оставим их всегда
  Serial.println(F("\n[RTOS] Creating all tasks..."));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 1: КНОПКИ (ядро 0, приоритет 1)
  // --------------------------------------------------------------------------
  Serial.print(F("   Button task (core 0, prio 1, stack 4096) ... "));
  buttonsTaskHandle = createButtonsTask(
    btnUp, btnDown, btnPower,
    targetTempPtr, systemStatePtr, startMillisPtr,
    MAX_TEMP,
    [](float newTarget) { PIDregulator.setpoint = newTarget; });
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 2: ДИСПЛЕЙ (ядро 0, приоритет 2)
  // --------------------------------------------------------------------------
  Serial.print(F("   Display task (core 0, prio 2, stack 4096) ... "));
  xTaskCreatePinnedToCore(
    taskDisplay,
    "displayTask", 
    4096,
    NULL, 
    2,
    NULL, 
    0);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 3: WiFi/ВЕБ (ядро 1, приоритет 3)
  // --------------------------------------------------------------------------
  Serial.print(F("   WiFi task (core 1, prio 3, stack 8192) ... "));
  xTaskCreatePinnedToCore(
    taskWebServer, 
    "webTask", 
    8192,
    NULL, 
    3,
    &webTaskHandle, 
    1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 4: ДАТЧИКИ (ядро 1, приоритет 3)
  // --------------------------------------------------------------------------
  Serial.print(F("   Sensor task (core 1, prio 3, stack 4096) ... "));
  xTaskCreatePinnedToCore(
    taskSensors,
    "sensorTask", 
    4096,
    NULL, 
    3,
    NULL, 
    1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 5: УПРАВЛЕНИЕ НАГРЕВОМ (ядро 1, приоритет 3)
  // --------------------------------------------------------------------------
  Serial.print(F("   Heater task (core 1, prio 3, stack 4096) ... "));
  xTaskCreatePinnedToCore(
    taskHeaterControl,
    "heaterTask", 
    4096,
    NULL, 
    3,
    NULL, 
    1);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ЗАДАЧА 6: SERIAL (ядро 0, приоритет 1)
  // --------------------------------------------------------------------------
  Serial.print(F("   Serial task (core 0, prio 1, stack 4096) ... "));
  xTaskCreatePinnedToCore(
    taskSerialCommands,
    "serialTask", 
    4096,
    NULL, 
    1,
    NULL, 
    0);
  Serial.println(F("OK"));

  Serial.println(F("[RTOS] All tasks created"));
}