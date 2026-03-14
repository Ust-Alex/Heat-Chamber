// ============================================================================
// sensors_core.h - Работа с датчиками температуры DS18B20
// ============================================================================

#ifndef SENSORS_CORE_H
#define SENSORS_CORE_H

#include <Arduino.h>
#include <DallasTemperature.h>
#include "config.h"

// Инициализация датчиков: поиск устройств, настройка разрешения
// Возвращает количество найденных датчиков (не более MAX_SENSORS)
uint8_t initSensors(DallasTemperature* sensors, DeviceAddress* addresses);

// Запрос температуры у всех датчиков (асинхронный)
void requestTemperatures(DallasTemperature* sensors);

// Чтение температуры конкретного датчика по индексу (0..MAX_SENSORS-1)
// Возвращает температуру в °C или NAN при ошибке
float readTemperature(DallasTemperature* sensors, DeviceAddress* addresses, uint8_t index);

// Проверка валидности температуры (не NAN и не бесконечность)
bool isValidTemperature(float temp);

#endif // SENSORS_CORE_H