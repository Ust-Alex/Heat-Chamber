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
#include "rtos_tasks.h"

// ============================================================================
// ОБЪЕКТЫ
// ============================================================================
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 5, U8X8_PIN_NONE);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
GButton btnUp(BTN_UP_PIN);
GButton btnDown(BTN_DOWN_PIN);
GButton btnPower(BTN_POWER_PIN);
uPID PIDregulator(D_INPUT | I_SATURATE | I_RESET, PID_INTERVAL);  // Добавлен I_RESET

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

float currentPower = 0.0;
uint32_t currentDuty = 0;

// ============================================================================
// КОЛБЭКИ ДЛЯ WEB
// ============================================================================
void onSetTarget(float value) {
  targetTemp = constrain(value, 0, MAX_TEMP);
  PIDregulator.setpoint = targetTemp;  // ← ВАЖНО: синхронизация
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
  Serial.println(F("🔥 HEAT CHAMBER v3.1 (с историей и NTP)"));
  Serial.println(F("========================================"));

  // [1] ДИСПЛЕЙ
  Serial.print(F("[1/15] Display init ... "));
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Starting...");
  u8g2.sendBuffer();
  Serial.println(F("OK"));

  // [2] КНОПКИ
  Serial.print(F("[2/15] Buttons init ... "));
  btnUp.setDebounce(50);
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);
  Serial.println(F("OK"));

  // [3] ДАТЧИКИ
  Serial.print(F("[3/15] Sensors init ... "));
  sensorCount = initSensors(&sensors, sensorAddresses);
  Serial.print(sensorCount);
  Serial.println(F(" found"));

  // [4] ШИМ
  Serial.print(F("[4/15] PWM init ... "));
  setupPWM();
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
  Serial.println(F("OK"));

  // [5] ПИД
  Serial.print(F("[5/15] PID init ... "));
  PIDregulator.Kp = PID_KP;
  PIDregulator.Ki = PID_KI;
  PIDregulator.Kd = PID_KD;
  PIDregulator.setpoint = targetTemp;
  PIDregulator.outMax = 4096;
  PIDregulator.outMin = 0;
  Serial.println(F("OK"));

  // [6] ГРАФИКИ
  Serial.print(F("[6/15] Graph buffers init ... "));
  initGraphBuffers(targetHistory, sensorHistory, targetTemp, MAX_SENSORS);
  Serial.println(F("OK"));

  // [7] WiFi (ТЕПЕРЬ СНАЧАЛА)
  Serial.print(F("[7/15] WiFi init ... "));
  if (initWiFi()) {
    Serial.println(F("CONNECTED"));
  } else {
    Serial.println(F("FAILED (continue without WiFi)"));
  }

  // [8] NTP (ТЕПЕРЬ ПОСЛЕ WiFi)
  Serial.print(F("[8/15] NTP init ... "));
  initNTP();

  // [9] mDNS
  Serial.print(F("[9/15] mDNS init ... "));
  initMDNS();
  Serial.println(F("OK"));

  // [10] HTTP-СЕРВЕР
  Serial.print(F("[10/15] HTTP server init ... "));
  initWebServer();
  Serial.println(F("OK"));

  // [11] WEBSOCKET
  Serial.print(F("[11/15] WebSocket init ... "));
  initWebSocket();
  Serial.println(F("OK"));

  // [12] WEB-ИНТЕРФЕЙС
  Serial.print(F("[12/15] Web callbacks init ... "));
  WebCallbacks callbacks;
  callbacks.onSetTarget = onSetTarget;
  callbacks.onSetPower = onSetPower;
  callbacks.onSetPID = onSetPID;
  callbacks.onReset = onReset;
  initWebInterface(callbacks);
  Serial.println(F("OK"));

  startMillis = millis();

  // [13] ЗАДАЧИ FREERTOS
  createAllTasks(&btnUp, &btnDown, &btnPower, &targetTemp, &systemState, &startMillis);

  // [14] ПРОВЕРКА ШИМ
  Serial.print(F("[14/15] PWM test ... "));
  setHeaterPower(1.0);
  delay(100);
  setHeaterPower(0.0);
  Serial.println(F("OK"));

  // [15] ЗАГРУЗКА ИСТОРИИ
  Serial.print(F("[15/15] History load ... "));
  // TODO: загрузка из файла при необходимости
  Serial.println(F("OK"));

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
  vTaskDelay(pdMS_TO_TICKS(1000));
}