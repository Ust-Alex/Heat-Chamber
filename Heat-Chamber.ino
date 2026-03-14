// ============================================================================
// Heat-Chamber.ino - ПИД-регулятор температуры
// ============================================================================
// Теперь только инициализация и запуск задач
// Все задачи вынесены в rtos_tasks.h/cpp
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
#include "rtos_tasks.h"  // <- ВСЕ ЗАДАЧИ ЗДЕСЬ

// ============================================================================
// ОБЪЕКТЫ (глобальные, для доступа из задач)
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

int targetHistory[GRAPH_WIDTH] = { 0 };
float sensorHistory[MAX_SENSORS][GRAPH_WIDTH] = { 0 };

unsigned long lastTempUpdate = 0;
unsigned long lastPIDUpdate = 0;
unsigned long lastGraphShift = 0;
unsigned long lastGraphDraw = 0;
unsigned long lastWebSend = 0;

TaskHandle_t buttonsTaskHandle;
TaskHandle_t webTaskHandle;

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

  // --------------------------------------------------------------------------
  // [1] ДИСПЛЕЙ
  // --------------------------------------------------------------------------
  Serial.print(F("[1/12] Display init ... "));
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Starting...");
  u8g2.sendBuffer();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [2] КНОПКИ
  // --------------------------------------------------------------------------
  Serial.print(F("[2/12] Buttons init ... "));
  btnUp.setDebounce(50);
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [3] ДАТЧИКИ
  // --------------------------------------------------------------------------
  Serial.print(F("[3/12] Sensors init ... "));
  sensorCount = initSensors(&sensors, sensorAddresses);
  Serial.print(sensorCount);
  Serial.println(F(" found"));

  // --------------------------------------------------------------------------
  // [4] ПИД
  // --------------------------------------------------------------------------
  Serial.print(F("[4/12] PID init ... "));
  PIDregulator.Kp = PID_KP;
  PIDregulator.Ki = PID_KI;
  PIDregulator.Kd = PID_KD;
  PIDregulator.setpoint = targetTemp;
  PIDregulator.outMax = MAX_DUTY / 2;
  PIDregulator.outMin = 0;
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [5] ГРАФИКИ
  // --------------------------------------------------------------------------
  Serial.print(F("[5/12] Graph buffers init ... "));
  initGraphBuffers(targetHistory, sensorHistory, targetTemp, MAX_SENSORS);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [6] WiFi
  // --------------------------------------------------------------------------
  Serial.print(F("[6/12] WiFi init ... "));
  if (initWiFi()) {
    Serial.println(F("CONNECTED"));
  } else {
    Serial.println(F("FAILED (continue without WiFi)"));
  }

  // --------------------------------------------------------------------------
  // [7] mDNS
  // --------------------------------------------------------------------------
  Serial.print(F("[7/12] mDNS init ... "));
  initMDNS();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [8] HTTP-СЕРВЕР
  // --------------------------------------------------------------------------
  Serial.print(F("[8/12] HTTP server init ... "));
  initWebServer();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [9] WEBSOCKET
  // --------------------------------------------------------------------------
  Serial.print(F("[9/12] WebSocket init ... "));
  initWebSocket();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // [10] WEB-ИНТЕРФЕЙС
  // --------------------------------------------------------------------------
  Serial.print(F("[10/12] Web callbacks init ... "));
  WebCallbacks callbacks;
  callbacks.onSetTarget = onSetTarget;
  callbacks.onSetPower = onSetPower;
  callbacks.onSetPID = onSetPID;
  callbacks.onReset = onReset;
  initWebInterface(callbacks);
  Serial.println(F("OK"));

  startMillis = millis();

  // --------------------------------------------------------------------------
  // [11] ЗАДАЧИ FREERTOS (через новый модуль)
  // --------------------------------------------------------------------------
  createAllTasks(&btnUp, &btnDown, &btnPower, &targetTemp, &systemState, &startMillis);

  // --------------------------------------------------------------------------
  // ФИНАЛ
  // --------------------------------------------------------------------------
  Serial.println(F("\n✅ СИСТЕМА ГОТОВА!"));
  if (isWiFiConnected()) {
    Serial.print(F("🌐 IP: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("🌐 http://heatchamber.local"));
  }
  Serial.println(F("========================================\n"));
}

// ============================================================================
// LOOP - ПУСТОЙ
// ============================================================================
void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}