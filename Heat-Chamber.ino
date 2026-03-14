// ============================================================================
// Heat-Chamber.ino - ПИД-регулятор температуры с графическим дисплеем и WEB
// ============================================================================

#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <GyverButton.h>
#include <uPID.h>

#include "config.h"
#include "pwm.h"
#include "sensors_core.h"
#include "display_graph.h"
#include "display_ui.h"
#include "rtos_buttons.h"
#include "wifi_webserver.h"
#include "web_interface.h"

// ============================================================================
// ОБЪЕКТЫ
// ============================================================================
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 5, U8X8_PIN_NONE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
GButton btnUp(BTN_UP_PIN);
GButton btnDown(BTN_DOWN_PIN);
GButton btnPower(BTN_POWER_PIN);

// ПИД-регулятор
uPID PIDregulator(D_INPUT | I_SATURATE, PID_INTERVAL);

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
float targetTemp = 30.0;                // Целевая температура
bool systemState = false;               // Вкл/Выкл
DeviceAddress sensorAddresses[MAX_SENSORS];
uint8_t sensorCount = 0;
float sensorTemps[MAX_SENSORS] = { 0 };
unsigned long startMillis = 0;

// Режимы и цвета (для WEB)
int currentMode = 1;                     // 0 - стабилизация, 1 - рабочий
int currentColor = 0;                     // 0 - зеленый, 1 - желтый, 2 - красный
float baseTemp = 30.0;                    // Базовая температура для режима 2

TaskHandle_t buttonsTaskHandle;
TaskHandle_t wifiTaskHandle;

// Буферы графиков
int targetHistory[GRAPH_WIDTH] = { 0 };
float sensorHistory[MAX_SENSORS][GRAPH_WIDTH] = { 0 };

// Таймеры
unsigned long lastTempUpdate = 0;
unsigned long lastPIDUpdate = 0;
unsigned long lastGraphShift = 0;
unsigned long lastGraphDraw = 0;
unsigned long lastWebSend = 0;

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================
void restartESP(const char* reason) {
  Serial.printf("Перезагрузка: %s\n", reason);
  delay(100);
  ESP.restart();
}

// ============================================================================
// КОЛБЭКИ ДЛЯ WEB
// ============================================================================
void onSetTarget(float value) {
  targetTemp = constrain(value, 0, MAX_TEMP);
  PIDregulator.setpoint = targetTemp;
}

void onSetPower(bool state) {
  systemState = state;
  if (systemState) {
    startMillis = millis();
  } else {
    heaterOff();
  }
}

void onSetPID(float Kp, float Ki, float Kd) {
  PIDregulator.Kp = Kp;
  PIDregulator.Ki = Ki;
  PIDregulator.Kd = Kd;
}

void onReset() {
  restartESP("WEB команда");
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== Heat Chamber v2.0 ===");

  // Дисплей
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Starting...");
  u8g2.sendBuffer();

  // Кнопки (настройка пинов)
  btnUp.setDebounce(50);
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);

  // ШИМ
  setupPWM();
  heaterOff();

  // Датчики
  sensorCount = initSensors(&sensors, sensorAddresses);
  Serial.printf("Датчиков найдено: %d\n", sensorCount);

  // ПИД
  PIDregulator.Kp = PID_KP;
  PIDregulator.Ki = PID_KI;
  PIDregulator.Kd = PID_KD;
  PIDregulator.setpoint = targetTemp;
  PIDregulator.outMax = MAX_DUTY / 2;
  PIDregulator.outMin = 0;

  // Инициализация графиков
  initGraphBuffers(targetHistory, sensorHistory, targetTemp, MAX_SENSORS);

  // ==========================================================================
  // НАСТРОЙКА КОЛБЭКОВ ДЛЯ WEB
  // ==========================================================================
  WebCallbacks callbacks;
  callbacks.onSetTarget = onSetTarget;
  callbacks.onSetPower = onSetPower;
  callbacks.onSetPID = onSetPID;
  callbacks.onReset = onReset;
  
  initWebInterface(callbacks);

  // ==========================================================================
  // ЗАПУСК ЗАДАЧ FREERTOS
  // ==========================================================================
  
  // Задача кнопок (ядро 0)
  buttonsTaskHandle = createButtonsTask(
    &btnUp, &btnDown, &btnPower,
    &targetTemp, &systemState, &startMillis,
    MAX_TEMP,
    [](float newTarget) { PIDregulator.setpoint = newTarget; }
  );

  // Задача WiFi (ядро 1)
  xTaskCreatePinnedToCore(
    taskWiFi, "wifiTask", 4096, NULL, 1, &wifiTaskHandle, 1
  );

  startMillis = millis();
  Serial.println("Система готова");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Чтение температуры
  if (now - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {
    lastTempUpdate = now;
    requestTemperatures(&sensors);

    for (uint8_t i = 0; i < min(sensorCount, (uint8_t)MAX_SENSORS); i++) {
      sensorTemps[i] = readTemperature(&sensors, sensorAddresses, i);
    }
  }

  // 2. ПИД и управление нагревателем
  if (now - lastPIDUpdate >= PID_INTERVAL) {
    lastPIDUpdate = now;

    if (systemState && isValidTemperature(sensorTemps[0])) {
      float pidOut = PIDregulator.compute(sensorTemps[0]);
      float power = (pidOut / (MAX_DUTY / 2)) * 100.0;
      setHeaterPower(power);
    } else {
      heaterOff();
    }
  }

  // 3. Сдвиг графиков
  if (now - lastGraphShift >= GRAPH_SHIFT_INTERVAL_MS) {
    lastGraphShift = now;
    shiftGraphsLeft(targetHistory, sensorHistory, targetTemp, sensorTemps, MAX_SENSORS);
  }

  // 4. Отрисовка интерфейса на дисплее
  if (now - lastGraphDraw >= GRAPH_DRAW_INTERVAL_MS) {
    lastGraphDraw = now;
    drawInterface(
      &u8g2, startMillis, targetTemp, systemState,
      sensorTemps, sensorCount,
      targetHistory, sensorHistory, MAX_SENSORS
    );
  }

  // 5. Отправка данных веб-клиентам (раз в секунду)
  if (now - lastWebSend >= 1000) {
    lastWebSend = now;
    
    if (isWebClientConnected()) {
      // Формируем время ЧЧ:ММ
      unsigned long elapsed = millis() - startMillis;
      uint8_t hours = (elapsed / 3600000) % 100;
      uint8_t minutes = (elapsed / 60000) % 60;
      char timeStr[6];
      snprintf(timeStr, sizeof(timeStr), "%02u:%02u", hours, minutes);
      
      sendWebData(
        sensorTemps[3],      // гильза
        sensorTemps[0],      // 100см
        sensorTemps[1],      // 75см
        sensorTemps[2],      // 50см
        currentMode,
        currentColor,
        timeStr,
        baseTemp
      );
    }
  }
}