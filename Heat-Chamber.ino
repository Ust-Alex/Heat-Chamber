// ============================================================================
// Heat-Chamber.ino - ПИД-регулятор температуры
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
#include "wifi_static.h"
#include "web_server.h"
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

uPID PIDregulator(D_INPUT | I_SATURATE, PID_INTERVAL);

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
float targetTemp = 30.0;
bool systemState = false;
DeviceAddress sensorAddresses[MAX_SENSORS];
uint8_t sensorCount = 0;
float sensorTemps[MAX_SENSORS] = { 0 };
unsigned long startMillis = 0;

TaskHandle_t buttonsTaskHandle;
TaskHandle_t webTaskHandle;

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
// КОЛБЭКИ ДЛЯ WEB
// ============================================================================
void onSetTarget(float value) {
  targetTemp = constrain(value, 0, MAX_TEMP);
  PIDregulator.setpoint = targetTemp;
  Serial.printf("[WEB] Уставка: %.1f\n", targetTemp);
}

void onSetPower(bool state) {
  systemState = state;
  if (systemState) startMillis = millis();
  Serial.printf("[WEB] Питание: %s\n", state ? "ON" : "OFF");
}

void onSetPID(float Kp, float Ki, float Kd) {
  PIDregulator.Kp = Kp;
  PIDregulator.Ki = Ki;
  PIDregulator.Kd = Kd;
  Serial.printf("[WEB] ПИД: Kp=%.2f Ki=%.2f Kd=%.2f\n", Kp, Ki, Kd);
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

  Serial.println(F("\n\n========================================"));
  Serial.println(F("🔥 HEAT CHAMBER v3.0"));
  Serial.println(F("========================================"));

  // ==========================================================================
  // [1] ДИСПЛЕЙ
  // ==========================================================================
  Serial.print(F("[1/12] Display init ... "));
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Starting...");
  u8g2.sendBuffer();
  Serial.println(F("OK"));

  // ==========================================================================
  // [2] КНОПКИ
  // ==========================================================================
  Serial.print(F("[2/12] Buttons init ... "));
  btnUp.setDebounce(50);
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);
  Serial.println(F("OK"));

  // ==========================================================================
  // [3] ДАТЧИКИ DS18B20
  // ==========================================================================
  Serial.print(F("[3/12] Sensors init ... "));
  sensorCount = initSensors(&sensors, sensorAddresses);
  Serial.print(sensorCount);
  Serial.println(F(" found"));

  // ==========================================================================
  // [4] ПИД-РЕГУЛЯТОР
  // ==========================================================================
  Serial.print(F("[4/12] PID init ... "));
  PIDregulator.Kp = PID_KP;
  PIDregulator.Ki = PID_KI;
  PIDregulator.Kd = PID_KD;
  PIDregulator.setpoint = targetTemp;
  PIDregulator.outMax = MAX_DUTY / 2;
  PIDregulator.outMin = 0;
  Serial.println(F("OK"));

  // ==========================================================================
  // [5] ГРАФИКИ (БУФЕРЫ)
  // ==========================================================================
  Serial.print(F("[5/12] Graph buffers init ... "));
  initGraphBuffers(targetHistory, sensorHistory, targetTemp, MAX_SENSORS);
  Serial.println(F("OK"));

  // ==========================================================================
  // [6] WiFi (СТАТИЧЕСКИЙ IP)
  // ==========================================================================
  Serial.print(F("[6/12] WiFi init ... "));
  if (initWiFi()) {
    Serial.println(F("CONNECTED"));
  } else {
    Serial.println(F("FAILED (continue without WiFi)"));
  }

  // ==========================================================================
  // [7] mDNS (heatchamber.local)
  // ==========================================================================
  Serial.print(F("[7/12] mDNS init ... "));
  initMDNS();
  Serial.println(F("OK"));

  // ==========================================================================
  // [8] ВЕБ-СЕРВЕР (HTTP)
  // ==========================================================================
  Serial.print(F("[8/12] HTTP server init ... "));
  initWebServer();
  Serial.println(F("OK"));

  // ==========================================================================
  // [9] WEBSOCKET
  // ==========================================================================
  Serial.print(F("[9/12] WebSocket init ... "));
  initWebSocket();
  Serial.println(F("OK"));

  // ==========================================================================
  // [10] КОЛБЭКИ ДЛЯ WEB
  // ==========================================================================
  Serial.print(F("[10/12] Web callbacks init ... "));
  WebCallbacks callbacks;
  callbacks.onSetTarget = onSetTarget;
  callbacks.onSetPower = onSetPower;
  callbacks.onSetPID = onSetPID;
  callbacks.onReset = onReset;
  initWebInterface(callbacks);
  Serial.println(F("OK"));

  // ==========================================================================
  // [11] ЗАДАЧИ FREERTOS
  // ==========================================================================
  Serial.print(F("[11/12] FreeRTOS tasks init ... "));

  // Задача кнопок (ядро 0)
  buttonsTaskHandle = createButtonsTask(
    &btnUp, &btnDown, &btnPower,
    &targetTemp, &systemState, &startMillis,
    MAX_TEMP,
    [](float newTarget) { PIDregulator.setpoint = newTarget; });

  // Задача веб-сервера (ядро 1)
  xTaskCreatePinnedToCore(
    taskWebServer, "webTask", 8192, NULL, 1, &webTaskHandle, 1);

  Serial.println(F("OK"));

  // ==========================================================================
  // [12] ШИМ (НАГРЕВАТЕЛЬ) — ПОСЛЕ ЗАДАЧ!
  // ==========================================================================
  Serial.print(F("[12/12] PWM init ... "));
  // setupPWM();
  // ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0);
  // ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
  Serial.println(F("OK"));

  startMillis = millis();

  // ==========================================================================
  // ФИНАЛЬНЫЙ ВЫВОД
  // ==========================================================================
  Serial.println(F("\n✅ СИСТЕМА ГОТОВА!"));
  if (isWiFiConnected()) {
    Serial.print(F("🌐 IP: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("🌐 http://heatchamber.local"));
  }
  Serial.println(F("========================================\n"));
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

    // ИСПРАВЛЕНО: убрали min(), просто идём по реальному количеству датчиков
    for (uint8_t i = 0; i < sensorCount; i++) {
      sensorTemps[i] = readTemperature(&sensors, sensorAddresses, i);
    }
  }

  // 2. ПИД и нагрев
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

  // 4. Отрисовка дисплея
  if (now - lastGraphDraw >= GRAPH_DRAW_INTERVAL_MS) {
    lastGraphDraw = now;
    drawInterface(
      &u8g2, startMillis, targetTemp, systemState,
      sensorTemps, sensorCount,
      targetHistory, sensorHistory, MAX_SENSORS);
  }

  // 5. Отправка данных в веб
  if (now - lastWebSend >= 1000) {
    lastWebSend = now;

    if (isWebClientConnected()) {
      unsigned long elapsed = millis() - startMillis;
      uint8_t hours = (elapsed / 3600000) % 100;
      uint8_t minutes = (elapsed / 60000) % 60;
      char timeStr[6];
      snprintf(timeStr, sizeof(timeStr), "%02u:%02u", hours, minutes);

      sendWebData(
        sensorTemps[0], sensorTemps[1], sensorTemps[2],
        targetTemp, systemState, timeStr);
    }
  }
}