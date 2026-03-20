// ============================================================================
// wifi_static.cpp - Реализация работы с WiFi (статический IP)
// ============================================================================
// Подробный отладочный вывод на каждом шаге для отслеживания проблем.
// ============================================================================

#include "wifi_static.h"

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WiFi СО СТАТИЧЕСКИМ IP
// ============================================================================
bool initWiFi() {
  Serial.println(F("\n========================================"));
  Serial.println(F("🌐 ИНИЦИАЛИЗАЦИЯ WiFi (СТАТИЧЕСКИЙ IP)"));
  Serial.println(F("========================================"));
  
  // --------------------------------------------------------------------------
  // ШАГ 1: Режим станции
  // --------------------------------------------------------------------------
  Serial.print(F("📡 Режим: STA (клиент) ... "));
  WiFi.mode(WIFI_STA);
  Serial.println(F("OK"));
  
  // --------------------------------------------------------------------------
  // ШАГ 2: Отключаем автоматическое подключение к предыдущим сетям
  // --------------------------------------------------------------------------
  Serial.print(F("🔄 Сброс авто-подключения ... "));
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  Serial.println(F("OK"));
  
  // --------------------------------------------------------------------------
  // ШАГ 3: Вывод настроек из config.h
  // --------------------------------------------------------------------------
  Serial.println(F("\n📋 НАСТРОЙКИ (из config.h):"));
  Serial.print(F("   SSID: ")); Serial.println(WIFI_SSID);
  
  Serial.print(F("   IP:   "));
  Serial.print(WIFI_IP[0]); Serial.print(F("."));
  Serial.print(WIFI_IP[1]); Serial.print(F("."));
  Serial.print(WIFI_IP[2]); Serial.print(F("."));
  Serial.println(WIFI_IP[3]);
  
  Serial.print(F("   GW:   "));
  Serial.print(WIFI_GATEWAY[0]); Serial.print(F("."));
  Serial.print(WIFI_GATEWAY[1]); Serial.print(F("."));
  Serial.print(WIFI_GATEWAY[2]); Serial.print(F("."));
  Serial.println(WIFI_GATEWAY[3]);
  
  Serial.print(F("   Mask: "));
  Serial.print(WIFI_SUBNET[0]); Serial.print(F("."));
  Serial.print(WIFI_SUBNET[1]); Serial.print(F("."));
  Serial.print(WIFI_SUBNET[2]); Serial.print(F("."));
  Serial.println(WIFI_SUBNET[3]);
  
  // --------------------------------------------------------------------------
  // ШАГ 4: Настройка статического IP
  // --------------------------------------------------------------------------
  Serial.print(F("\n🔧 Применяем статический IP ... "));
  
  // ВАЖНО: .config() нужно вызывать ДО .begin()
  if (!WiFi.config(WIFI_IP, WIFI_GATEWAY, WIFI_SUBNET)) {
    Serial.println(F("ОШИБКА!"));
    Serial.println(F("   Не удалось настроить IP. Возможные причины:"));
    Serial.println(F("   - Неверный IP/маска/шлюз"));
    Serial.println(F("   - Конфликт с другими настройками"));
    Serial.println(F("   Продолжаем с DHCP..."));
  } else {
    Serial.println(F("OK"));
  }
  
  // --------------------------------------------------------------------------
  // ШАГ 5: Подключение к роутеру
  // --------------------------------------------------------------------------
  Serial.print(F("\n🔄 Подключаемся к "));
  Serial.print(WIFI_SSID);
  Serial.print(F(" ... "));
  
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  // --------------------------------------------------------------------------
  // ШАГ 6: Ожидание подключения (с таймаутом)
  // --------------------------------------------------------------------------
  const int MAX_ATTEMPTS = 40;  // 40 * 500мс = 20 секунд
  int attempts = 0;
  
  while (WiFi.status() != WL_CONNECTED && attempts < MAX_ATTEMPTS) {
    delay(500);
    Serial.print(F("."));
    attempts++;
    
    // Каждые 10 попыток выводим статус
    if (attempts % 10 == 0) {
      Serial.print(F("[статус:"));
      Serial.print(WiFi.status());
      Serial.print(F("]"));
    }
  }
  Serial.println(F(""));
  
  // --------------------------------------------------------------------------
  // ШАГ 7: Проверка результата
  // --------------------------------------------------------------------------
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(F("\n✅ WiFi ПОДКЛЮЧЕН УСПЕШНО!"));
    Serial.print(F("   SSID: ")); Serial.println(WiFi.SSID());
    Serial.print(F("   IP:   ")); Serial.println(WiFi.localIP());
    Serial.print(F("   GW:   ")); Serial.println(WiFi.gatewayIP());
    Serial.print(F("   MAC:  ")); Serial.println(WiFi.macAddress());
    Serial.print(F("   RSSI: ")); Serial.print(WiFi.RSSI()); Serial.println(F(" dBm"));
    
    return true;
  } else {
    Serial.println(F("\n❌ ОШИБКА ПОДКЛЮЧЕНИЯ!"));
    Serial.print(F("   Статус: ")); Serial.println(WiFi.status());
    Serial.println(F("   Возможные причины:"));
    Serial.println(F("   - Неверный SSID или пароль"));
    Serial.println(F("   - Роутер недоступен"));
    Serial.println(F("   - Неправильные настройки IP"));
    return false;
  }
}

// ============================================================================
// ПРОВЕРКА СТАТУСА
// ============================================================================
bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ============================================================================
// ПОЛУЧЕНИЕ IP-АДРЕСА
// ============================================================================
String getWiFiIP() {
  return WiFi.localIP().toString();
}

// ============================================================================
// ПОЛУЧЕНИЕ SSID
// ============================================================================
String getWiFiSSID() {
  return WiFi.SSID();
}

// ============================================================================
// ПОЛУЧЕНИЕ УРОВНЯ СИГНАЛА
// ============================================================================
int getWiFiRSSI() {
  return WiFi.RSSI();
}

// ============================================================================
// ПЕРЕЗАГРУЗКА ESP С ОТЛАДКОЙ
// ============================================================================
void restartESP(const char* reason) {
  Serial.println(F("\n========================================"));
  Serial.println(F("🔄 ПЕРЕЗАГРУЗКА"));
  Serial.println(F("========================================"));
  Serial.print(F("Причина: ")); Serial.println(reason);
  Serial.println(F("Ожидание 100 мс перед перезагрузкой..."));
  
  delay(100);  // Даём время вывести сообщение
  
  Serial.println(F("🔄 ESP.restart()..."));
  ESP.restart();
  
  // Никогда не выполнится, но для ясности
  while(1) { delay(1000); }
}