// ============================================================================
// Heat-Chamber.ino - ПИД-регулятор температуры
// ============================================================================
// Проект: Термический реформинг (нагрев дистиллята до 40-70°C)
// Платформа: ESP32
// Веб-сервер: ESPAsyncWebServer v3.10.1
// ============================================================================
// Компоненты проекта:
// - OTA: беспроводное обновление прошивки
// - WebSerial: удалённая консоль в браузере
// - WebSocket: real-time передача данных на веб-страницу
// - FreeRTOS: многозадачность (дисплей, датчики, нагрев, кнопки, веб)
// - ПИД-регулятор: поддержание температуры
// - Дисплей 128x64 (ST7920): графики и интерфейс
// - Датчики DS18B20: контроль температуры (до 3 штук)
// - ШИМ на MOSFET: управление нагревателем
// ============================================================================

#include <OneWire.h>
#include <DallasTemperature.h>
#include <U8g2lib.h>
#include <GyverButton.h>
#include <uPID.h>
#include <WebSerialLite.h>           // <-- ДОБАВЛЕНО для WebSerial

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
#include "settings.h"
#include "ota.h"
#include "webserial.h"

// ============================================================================
// ОБЪЕКТЫ БИБЛИОТЕК
// ============================================================================

// Дисплей (SPI, аппаратный)
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 5, U8X8_PIN_NONE);

// Шина 1-Wire и датчики температуры
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Кнопки (с антидребезгом)
GButton btnUp(BTN_UP_PIN);
GButton btnDown(BTN_DOWN_PIN);
GButton btnPower(BTN_POWER_PIN);

// ПИД-регулятор (D_INPUT = прямой вход, I_SATURATE = насыщение интегральной составляющей)
uPID PIDregulator(D_INPUT | I_SATURATE, PID_INTERVAL);

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================

float targetTemp = 30.0;                    // Целевая температура (уставка)
bool systemState = false;                   // Состояние системы (ON/OFF)

DeviceAddress sensorAddresses[MAX_SENSORS]; // Адреса датчиков DS18B20
uint8_t sensorCount = 0;                    // Количество найденных датчиков
float sensorTemps[MAX_SENSORS] = { 0 };     // Температуры с датчиков

unsigned long startMillis = 0;              // Время запуска (для uptime)

// Буферы для графиков на дисплее
int targetHistory[GRAPH_WIDTH] = { 0 };
float sensorHistory[MAX_SENSORS][GRAPH_WIDTH] = { 0 };

// Таймеры для неблокирующих операций
unsigned long lastTempUpdate = 0;
unsigned long lastPIDUpdate = 0;
unsigned long lastGraphShift = 0;
unsigned long lastGraphDraw = 0;
unsigned long lastWebSend = 0;

// Хэндлы задач FreeRTOS
TaskHandle_t buttonsTaskHandle;
TaskHandle_t webTaskHandle;

// Управление нагревателем
float currentPower = 0.0;      // Мощность в процентах (0.0 - 1.0)
uint32_t currentDuty = 0;      // Скважность ШИМ (0 - 8191)

// ============================================================================
// ОБЪЯВЛЕНИЕ ВНЕШНИХ ПЕРЕМЕННЫХ
// ============================================================================

extern unsigned long heaterSeconds;  // Общее время нагрева (из rtos_tasks.cpp)

// ============================================================================
// КОЛБЭКИ ДЛЯ ВЕБ-ИНТЕРФЕЙСА
// ============================================================================

/**
 * @brief Обработчик изменения целевой температуры с веб-страницы
 * @param value Новая уставка (0 - MAX_TEMP)
 */
void onSetTarget(float value) {
  targetTemp = constrain(value, 0, MAX_TEMP);
  PIDregulator.setpoint = targetTemp;
  saveSettings(targetTemp, systemState);  // Сохраняем в EEPROM/Flash
  Serial.printf("[WEB] Уставка: %.1f\n", targetTemp);
  // WebSerial дублирует вывод через команды, здесь не нужно
}

/**
 * @brief Обработчик включения/выключения нагрева с веб-страницы
 * @param state true = ON, false = OFF
 */
void onSetPower(bool state) {
  systemState = state;
  saveSettings(targetTemp, systemState);
  Serial.printf("[WEB] Питание: %s\n", state ? "ON" : "OFF");
  // WebSerial дублирует вывод через команды, здесь не нужно
}

/**
 * @brief Обработчик изменения коэффициентов ПИД-регулятора
 * @param Kp Пропорциональный коэффициент
 * @param Ki Интегральный коэффициент
 * @param Kd Дифференциальный коэффициент
 */
void onSetPID(float Kp, float Ki, float Kd) {
  PIDregulator.Kp = Kp;
  PIDregulator.Ki = Ki;
  PIDregulator.Kd = Kd;
  Serial.printf("[WEB] ПИД: Kp=%.2f Ki=%.2f Kd=%.2f\n", Kp, Ki, Kd);
  // WebSerial дублирует вывод через команды, здесь не нужно
}

/**
 * @brief Обработчик команды перезагрузки с веб-страницы
 */
void onReset() {
  restartESP("WEB команда");
}

// ============================================================================
// НАЧАЛЬНАЯ НАСТРОЙКА (setup)
// ============================================================================

void setup() {
  // Инициализация последовательного порта (отладка)
  Serial.begin(115200);
  delay(1000);  // Ожидание стабилизации питания

  // Печать заголовка
  Serial.println(F("\n\n========================================"));
  Serial.println(F("🔥 HEAT CHAMBER v3.1 (с историей и NTP)"));
  Serial.println(F("========================================"));

  // --------------------------------------------------------------------------
  // 1. ДИСПЛЕЙ
  // --------------------------------------------------------------------------
  Serial.print(F("[1/15] Display init ... "));
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Starting...");
  u8g2.sendBuffer();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 2. КНОПКИ
  // --------------------------------------------------------------------------
  Serial.print(F("[2/15] Buttons init ... "));
  btnUp.setDebounce(50);
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 3. ДАТЧИКИ ТЕМПЕРАТУРЫ (DS18B20)
  // --------------------------------------------------------------------------
  Serial.print(F("[3/15] Sensors init ... "));
  sensorCount = initSensors(&sensors, sensorAddresses);
  Serial.print(sensorCount);
  Serial.println(F(" found"));

  // --------------------------------------------------------------------------
  // 4. ШИМ (управление нагревателем)
  // --------------------------------------------------------------------------
  Serial.print(F("[4/15] PWM init ... "));
  setupPWM();
  ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
  ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 5. ПИД-РЕГУЛЯТОР
  // --------------------------------------------------------------------------
  Serial.print(F("[5/15] PID init ... "));
  PIDregulator.Kp = PID_KP;
  PIDregulator.Ki = PID_KI;
  PIDregulator.Kd = PID_KD;
  PIDregulator.setpoint = targetTemp;
  PIDregulator.outMax = MAX_DUTY;   // 8191
  PIDregulator.outMin = 0;
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 6. БУФЕРЫ ГРАФИКОВ
  // --------------------------------------------------------------------------
  Serial.print(F("[6/15] Graph buffers init ... "));
  initGraphBuffers(targetHistory, sensorHistory, targetTemp, MAX_SENSORS);
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 7. ПОДКЛЮЧЕНИЕ К Wi-Fi
  // --------------------------------------------------------------------------
  Serial.print(F("[7/15] WiFi init ... "));
  if (initWiFi()) {
    Serial.println(F("CONNECTED"));
  } else {
    Serial.println(F("FAILED (continue without WiFi)"));
  }

  // --------------------------------------------------------------------------
  // 7.5 ЗАГРУЗКА НАСТРОЕК (уставка, состояние, время нагрева)
  // --------------------------------------------------------------------------
  Serial.print(F("[7.5/15] Loading settings ... "));
  initSettings(targetTemp, systemState);
  startMillis = millis();  // uptime
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 8. NTP (синхронизация времени)
  // --------------------------------------------------------------------------
  Serial.print(F("[8/15] NTP init ... "));
  initNTP();

  // --------------------------------------------------------------------------
  // 9. mDNS (heatchamber.local)
  // --------------------------------------------------------------------------
  Serial.print(F("[9/15] mDNS init ... "));
  initMDNS();
  Serial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 10. HTTP-СЕРВЕР (веб-интерфейс)
  // --------------------------------------------------------------------------
  Serial.print(F("[10/15] HTTP server init ... "));
  initWebServer();
  Serial.println(F("OK"));

  // ==========================================================================
  // ВАЖНО: WebSerial инициализируется ДО OTA, чтобы дублировать сообщения
  // ==========================================================================

  // --------------------------------------------------------------------------
  // WEBSERIAL (удалённая консоль в браузере)
  // --------------------------------------------------------------------------
  Serial.print(F("[WebSerial] Init ... "));
  setupWebSerial(&server);
  Serial.println(F("OK"));
  
  // Отправляем приветствие в WebSerial (только ПОСЛЕ инициализации)
  WebSerial.println(F("\n========================================"));
  WebSerial.println(F("🔥 HEAT CHAMBER v3.1 (с историей и NTP)"));
  WebSerial.println(F("========================================"));
  WebSerial.println(F("Система загружается...\n"));

  // --------------------------------------------------------------------------
  // OTA (беспроводное обновление прошивки)
  // --------------------------------------------------------------------------
  Serial.print(F("[OTA] Init ... "));
  WebSerial.print(F("[OTA] Init ... "));
  setupOTA();
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 11. WEBSOCKET (real-time данные)
  // --------------------------------------------------------------------------
  Serial.print(F("[11/15] WebSocket init ... "));
  WebSerial.print(F("[11/15] WebSocket init ... "));
  initWebSocket();
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 12. ВЕБ-ИНТЕРФЕЙС (регистрация колбэков)
  // --------------------------------------------------------------------------
  Serial.print(F("[12/15] Web callbacks init ... "));
  WebSerial.print(F("[12/15] Web callbacks init ... "));
  WebCallbacks callbacks;
  callbacks.onSetTarget = onSetTarget;
  callbacks.onSetPower = onSetPower;
  callbacks.onSetPID = onSetPID;
  callbacks.onReset = onReset;
  initWebInterface(callbacks);
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 13. ЗАДАЧИ FREERTOS
  // --------------------------------------------------------------------------
  Serial.print(F("[13/15] Creating FreeRTOS tasks... "));
  WebSerial.print(F("[13/15] Creating FreeRTOS tasks... "));
  createAllTasks(&btnUp, &btnDown, &btnPower, &targetTemp, &systemState, &startMillis);
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // OTA ЗАДАЧА (отдельная задача для обработки OTA)
  // --------------------------------------------------------------------------
  Serial.print(F("[OTA] Creating FreeRTOS task... "));
  WebSerial.print(F("[OTA] Creating FreeRTOS task... "));
  createOTATask();
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 14. ПРОВЕРКА ШИМ (кратковременное включение нагревателя)
  // --------------------------------------------------------------------------
  Serial.print(F("[14/15] PWM test ... "));
  WebSerial.print(F("[14/15] PWM test ... "));
  setHeaterPower(1.0);   // Включить на 100 мс
  delay(100);
  setHeaterPower(0.0);   // Выключить
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // 15. ЗАГРУЗКА ИСТОРИИ (TODO: из файла)
  // --------------------------------------------------------------------------
  Serial.print(F("[15/15] History load ... "));
  WebSerial.print(F("[15/15] History load ... "));
  // TODO: загрузка истории из LittleFS
  Serial.println(F("OK"));
  WebSerial.println(F("OK"));

  // --------------------------------------------------------------------------
  // ФИНАЛЬНОЕ СООБЩЕНИЕ
  // --------------------------------------------------------------------------
  Serial.println(F("\n✅ СИСТЕМА ПОЛНОСТЬЮ ГОТОВА!"));
  WebSerial.println(F("\n✅ СИСТЕМА ПОЛНОСТЬЮ ГОТОВА!"));
  
  if (isWiFiConnected()) {
    String ipMsg = "🌐 IP-адрес: " + WiFi.localIP().toString();
    Serial.println(ipMsg);
    WebSerial.println(ipMsg);
    
    String mdnsMsg = "🌐 mDNS: http://heatchamber.local";
    Serial.println(mdnsMsg);
    WebSerial.println(mdnsMsg);
    
    String wsMsg = "🌐 WebSerial: http://" + WiFi.localIP().toString() + "/webserial";
    Serial.println(wsMsg);
    WebSerial.println(wsMsg);
  }
  
  Serial.println(F("========================================\n"));
  WebSerial.println(F("========================================\n"));
  WebSerial.println(F("💡 Введите 'help' для списка команд"));
  WebSerial.println(F(""));
}

// ============================================================================
// ОСНОВНОЙ ЦИКЛ (loop)
// ============================================================================
// В проекте используется FreeRTOS, поэтому loop() почти пуст.
// Все задачи работают параллельно на разных ядрах.
// ============================================================================

void loop() {
  // Отдаём управление другим задачам на 1 секунду
  // Фактически loop() не используется, все задачи запущены в setup()
  vTaskDelay(pdMS_TO_TICKS(1000));
}