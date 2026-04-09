/**
 * @file webserial.cpp
 * @brief Реализация модуля WebSerial с командами управления
 */

#include <Arduino.h>
#include <WebSerialLite.h>
#include <uPID.h>
#include <stdarg.h>
#include "webserial.h"
#include "config.h"

// ============================================================================
// ПОДКЛЮЧЕНИЕ НЕОБХОДИМЫХ ЗАГОЛОВКОВ ДЛЯ ДАТЧИКОВ
// ============================================================================
#include <OneWire.h>
#include <DallasTemperature.h>
#include "sensors_core.h"

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ (из основного скетча)
// ============================================================================
extern float targetTemp;
extern bool systemState;
extern float sensorTemps[MAX_SENSORS];
extern uint8_t sensorCount;
extern float currentPower;
extern uPID PIDregulator;

// ============================================================================
// ОБЪЯВЛЕНИЕ ВНЕШНИХ ФУНКЦИЙ
// ============================================================================
extern void onSetTarget(float value);
extern void onSetPower(bool state);
extern void onSetPID(float Kp, float Ki, float Kd);
extern void restartESP(const char* reason);

// Объект датчиков (из main) и функция инициализации
extern DallasTemperature sensors;
extern DeviceAddress sensorAddresses[MAX_SENSORS];

// ============================================================================
// ОТПРАВКА СТРОКИ С ПРИНУДИТЕЛЬНЫМ ОБНОВЛЕНИЕМ
// ============================================================================
static void webSerialPrintlnSafe(const char* msg) {
  WebSerial.println(msg);
  delay(5);
}

static void webSerialPrintfSafe(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  webSerialPrintlnSafe(buffer);
}

// ============================================================================
// ОБРАБОТЧИК КОМАНД
// ============================================================================
static void handleReceivedMessage(uint8_t* data, size_t len) {
  String received = String((char*)data);
  received.trim();

  Serial.print("📩 [WebSerial] Получено: ");
  Serial.println(received);

  String cmd = received;
  cmd.toLowerCase();

  // ========== СПРАВКА ==========
  if (cmd == "help" || cmd == "h" || cmd == "?") {
    webSerialPrintlnSafe("\n=== КОМАНДЫ ===");
    webSerialPrintlnSafe("help, h, ?       - справка");
    webSerialPrintlnSafe("info, i          - инфо об устройстве");
    webSerialPrintlnSafe("status, st       - статус системы");
    webSerialPrintlnSafe("heat_on/off      - вкл/выкл нагрев");
    webSerialPrintlnSafe("set_temp X       - уставка (0-70°C)");
    webSerialPrintlnSafe("temp             - показать уставку");
    webSerialPrintlnSafe("pid, pid_reset   - показать (сбросить) ПИД");
    webSerialPrintlnSafe("pid Kp Ki Kd     - установить ПИД");
    webSerialPrintlnSafe("sensors          - все датчики");
    webSerialPrintlnSafe("scan             - пересканировать");
    webSerialPrintlnSafe("heap             - свободная память");
    webSerialPrintlnSafe("restart, reboot  - перезагрузка");
    webSerialPrintlnSafe("clear, cls       - очистить экран");
    webSerialPrintlnSafe("ping, uptime, up - связь/время работы");
    webSerialPrintlnSafe("===============\n");
  }

  // ========== ИНФОРМАЦИЯ ==========
  else if (cmd == "info" || cmd == "i") {
    webSerialPrintlnSafe("\n=== ИНФОРМАЦИЯ ОБ УСТРОЙСТВЕ ===");
    webSerialPrintfSafe("Wi-Fi сеть:       %s", WIFI_SSID);
    webSerialPrintfSafe("IP-адрес:         %s", WiFi.localIP().toString().c_str());
    webSerialPrintfSafe("Уровень сигнала:  %d dBm", WiFi.RSSI());
    webSerialPrintfSafe("MAC-адрес:        %s", WiFi.macAddress().c_str());
    webSerialPrintfSafe("Свободно RAM:     %u байт", ESP.getFreeHeap());
    webSerialPrintfSafe("Частота CPU:      %u MHz", ESP.getCpuFreqMHz());

    unsigned long uptimeSec = millis() / 1000;
    webSerialPrintfSafe("Время работы:     %lu дней, %02lu:%02lu:%02lu",
                        uptimeSec / 86400,
                        (uptimeSec % 86400) / 3600,
                        (uptimeSec % 3600) / 60,
                        uptimeSec % 60);
    webSerialPrintlnSafe("=============================\n");
  }

  // ========== СТАТУС ==========
  else if (cmd == "status" || cmd == "st") {
    webSerialPrintlnSafe("\n=== СТАТУС СИСТЕМЫ ===");
    webSerialPrintfSafe("Wi-Fi:            %s", WiFi.status() == WL_CONNECTED ? "ПОДКЛЮЧЁН" : "НЕТ");
    webSerialPrintfSafe("Нагрев:           %s", systemState ? "ВКЛЮЧЁН" : "ВЫКЛЮЧЁН");
    webSerialPrintfSafe("Уставка:          %.1f°C", targetTemp);
    webSerialPrintfSafe("Мощность:         %.1f%%", currentPower * 100);
    webSerialPrintfSafe("Датчиков:         %d", sensorCount);
    webSerialPrintlnSafe("===================\n");
  }

  // ========== УПРАВЛЕНИЕ НАГРЕВОМ ==========
  else if (cmd == "heat_on") {
    onSetPower(true);
    webSerialPrintlnSafe("✅ Нагрев включён");
  } else if (cmd == "heat_off") {
    onSetPower(false);
    webSerialPrintlnSafe("✅ Нагрев выключён");
  }

  // ========== УСТАВКА ТЕМПЕРАТУРЫ ==========
  else if (cmd.startsWith("set_temp ")) {
    float newTemp = received.substring(9).toFloat();
    if (newTemp >= 0 && newTemp <= MAX_TEMP) {
      onSetTarget(newTemp);
      webSerialPrintfSafe("🌡️ Уставка: %.1f°C", newTemp);
    } else {
      webSerialPrintfSafe("❌ Ошибка: уставка должна быть от 0 до %.0f°C", MAX_TEMP);
    }
  } else if (cmd == "temp") {
    webSerialPrintfSafe("🌡️ Текущая уставка: %.1f°C", targetTemp);
  }

  // ========== ПИД-РЕГУЛЯТОР ==========
  else if (cmd == "pid") {
    webSerialPrintfSafe("📊 ПИД: Kp=%.2f Ki=%.2f Kd=%.2f",
                        PIDregulator.Kp, PIDregulator.Ki, PIDregulator.Kd);
  } else if (cmd == "pid_reset") {
    PIDregulator.Kp = PID_KP;
    PIDregulator.Ki = PID_KI;
    PIDregulator.Kd = PID_KD;
    webSerialPrintfSafe("✅ ПИД сброшен: Kp=%.2f Ki=%.2f Kd=%.2f",
                        PID_KP, PID_KI, PID_KD);
  } else if (cmd.startsWith("pid ")) {
    float Kp, Ki, Kd;
    char buf[64];
    strcpy(buf, received.c_str());
    char* p = strtok(buf, " ");
    p = strtok(NULL, " ");
    if (p) Kp = atof(p);
    p = strtok(NULL, " ");
    if (p) Ki = atof(p);
    p = strtok(NULL, " ");
    if (p) Kd = atof(p);
    onSetPID(Kp, Ki, Kd);
    webSerialPrintfSafe("✅ ПИД: Kp=%.2f Ki=%.2f Kd=%.2f", Kp, Ki, Kd);
  }

  // ========== ДАТЧИКИ ==========
  else if (cmd == "sensors") {
    webSerialPrintlnSafe("\n=== ДАТЧИКИ ТЕМПЕРАТУРЫ ===");
    for (int i = 0; i < sensorCount && i < MAX_SENSORS; i++) {
      webSerialPrintfSafe("T%d: %.1f°C", i, sensorTemps[i]);
    }
    webSerialPrintlnSafe("=========================\n");
  } else if (cmd == "scan") {
    webSerialPrintlnSafe("🔍 Сканирование датчиков...");
    sensorCount = initSensors(&sensors, sensorAddresses);
    webSerialPrintfSafe("✅ Найдено датчиков: %d", sensorCount);
  }

  // ========== ПАМЯТЬ ==========
  else if (cmd == "heap") {
    webSerialPrintfSafe("💾 Свободно RAM: %u байт (%.1f%%)",
                        ESP.getFreeHeap(), (ESP.getFreeHeap() * 100.0 / ESP.getHeapSize()));
#ifdef ESP32
    webSerialPrintfSafe("💾 Свободно PSRAM: %u байт", ESP.getFreePsram());
#endif
  }

  // ========== ПЕРЕЗАГРУЗКА ==========
  else if (cmd == "restart" || cmd == "reboot") {
    webSerialPrintlnSafe("🔄 Перезагрузка ESP32 через 1 секунду...");
    delay(1000);
    ESP.restart();
  }

  // ========== ОЧИСТКА ЭКРАНА ==========
  else if (cmd == "clear" || cmd == "cls") {
    webSerialPrintlnSafe("<div style='clear: both;'></div><hr>🧹 Экран очищен<hr>");
  }

  // ========== ПРОВЕРКА СВЯЗИ ==========
  else if (cmd == "ping") {
    webSerialPrintfSafe("🏓 pong! (время с запуска: %lu мс)", millis());
  }

  // ========== ВРЕМЯ РАБОТЫ ==========
  else if (cmd == "uptime" || cmd == "up") {
    unsigned long uptimeSec = millis() / 1000;
    webSerialPrintfSafe("⏱️ Время работы: %lu дней, %02lu:%02lu:%02lu",
                        uptimeSec / 86400,
                        (uptimeSec % 86400) / 3600,
                        (uptimeSec % 3600) / 60,
                        uptimeSec % 60);
  }

  // ========== НЕИЗВЕСТНАЯ КОМАНДА ==========
  else if (received.length() > 0) {
    webSerialPrintfSafe("❌ Неизвестная команда: \"%s\"", received.c_str());
    webSerialPrintlnSafe("   Введите 'help' для списка команд");
  }
}

// ============================================================================
// ПУБЛИЧНЫЕ ФУНКЦИИ
// ============================================================================

void setupWebSerial(AsyncWebServer* server) {
  if (server == nullptr) {
    Serial.println("[WebSerial] ОШИБКА: передан нулевой указатель на сервер");
    return;
  }

  Serial.println("\n========================================");
  Serial.println("🌐 НАСТРОЙКА WEBSERIAL");
  Serial.println("========================================");

  WebSerial.begin(server);
  WebSerial.onMessage(handleReceivedMessage);

  Serial.println("✅ WebSerial готов!");
  Serial.print("   Доступен по адресу: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/webserial");
  Serial.println("========================================\n");

  webSerialPrintlnSafe("\n=== ESP32 WebSerial Console ===");
  webSerialPrintlnSafe("Введите 'help' для списка команд");
  webSerialPrintlnSafe("===============================\n");
}

// ============================================================================
// ФУНКЦИЯ ДЛЯ ФОРМАТИРОВАННОГО ВЫВОДА ИЗ ДРУГИХ МОДУЛЕЙ
// ============================================================================
void webSerialPrintf(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  WebSerial.print(buffer);
}