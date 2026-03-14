// ============================================================================
// network.h - Сетевые утилиты и хелперы
// ============================================================================

#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <IPAddress.h>
#include <WiFi.h>  // Явно подключаем WiFi

// Форматирование IP-адреса в строку
String ipToString(const IPAddress& ip);

// Получение статуса подключения в виде строки
const char* wifiStatusToString(uint8_t status);  // Заменил wl_status_t на uint8_t

// Проверка, подключён ли WiFi
bool isWiFiConnected();

// Безопасное получение MAC-адреса
String getMacAddress();

// Перезагрузка ESP с сообщением
void restartESP(const char* reason);

// Получение времени работы в формате "дни ЧЧ:ММ:СС"
String getUptimeString(unsigned long startMillis);

#endif