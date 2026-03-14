// ============================================================================
// network.cpp - Реализация сетевых утилит
// ============================================================================

#include "network.h"
#include <WiFi.h>

// ============================================================================
// IP-АДРЕСА
// ============================================================================
String ipToString(const IPAddress& ip) {
  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return String(buffer);
}

// ============================================================================
// СТАТУСЫ WiFi (используем wl_status_t из WiFi.h)
// ============================================================================
const char* wifiStatusToString(uint8_t status) {
  switch (status) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:    return "NO_SSID";
    case WL_SCAN_COMPLETED:   return "SCAN_DONE";
    case WL_CONNECTED:        return "CONNECTED";
    case WL_CONNECT_FAILED:   return "CONNECT_FAIL";
    case WL_CONNECTION_LOST:  return "CONN_LOST";
    case WL_DISCONNECTED:     return "DISCONNECTED";
    default:                  return "UNKNOWN";
  }
}

bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

// ============================================================================
// MAC-АДРЕС
// ============================================================================
String getMacAddress() {
  uint8_t mac[6];
  char macStr[18] = {0};
  
  // WiFi.macAddress() возвращает uint8_t*
  uint8_t* macPtr = WiFi.macAddress(mac);
  if (macPtr) {
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
  }
  return "00:00:00:00:00:00";
}

// ============================================================================
// УПРАВЛЕНИЕ СИСТЕМОЙ
// ============================================================================
void restartESP(const char* reason) {
  Serial.printf("Перезагрузка: %s\n", reason);
  delay(100);
  ESP.restart();
}

// ============================================================================
// ВРЕМЯ РАБОТЫ
// ============================================================================
String getUptimeString(unsigned long startMillis) {
  unsigned long elapsed = millis() - startMillis;
  
  unsigned long days = elapsed / 86400000UL;
  unsigned long hours = (elapsed % 86400000UL) / 3600000UL;
  unsigned long minutes = (elapsed % 3600000UL) / 60000UL;
  unsigned long seconds = (elapsed % 60000UL) / 1000UL;
  
  char buffer[32];
  if (days > 0) {
    snprintf(buffer, sizeof(buffer), "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
  } else {
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
  }
  
  return String(buffer);
}