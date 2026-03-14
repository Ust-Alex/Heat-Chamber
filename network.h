// ============================================================================
// network.h - Сетевые утилиты (максимально просто)
// ============================================================================

#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <IPAddress.h>

// Форматирование IP-адреса в строку
String ipToString(const IPAddress& ip);

// Проверка, подключён ли WiFi (простая обертка)
bool isWiFiConnected();

// Перезагрузка ESP с сообщением
void restartESP(const char* reason);

// Получение времени работы в формате "ЧЧ:ММ:СС"
String getUptimeString(unsigned long startMillis);

#endif