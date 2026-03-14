// ============================================================================
// sensors_core.cpp - Реализация работы с DS18B20
// ============================================================================

#include "sensors_core.h"

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================
uint8_t initSensors(DallasTemperature* sensors, DeviceAddress* addresses) {
  sensors->begin();
  
  uint8_t found = sensors->getDeviceCount();
  uint8_t used = 0;

  // Настраиваем не больше MAX_SENSORS датчиков
  for (uint8_t i = 0; i < min(found, (uint8_t)MAX_SENSORS); i++) {
    if (sensors->getAddress(addresses[i], i)) {
      sensors->setResolution(addresses[i], 12);  // максимальное разрешение
      used++;
    }
  }
  
  return used;
}

// ============================================================================
// ЧТЕНИЕ ТЕМПЕРАТУР
// ============================================================================
void requestTemperatures(DallasTemperature* sensors) {
  sensors->requestTemperatures();
}

float readTemperature(DallasTemperature* sensors, DeviceAddress* addresses, uint8_t index) {
  if (index >= MAX_SENSORS) return NAN;
  
  float temp = sensors->getTempC(addresses[index]);
  
  // DEVICE_DISCONNECTED_C возвращается при ошибке (-127)
  return (temp == DEVICE_DISCONNECTED_C) ? NAN : temp;
}

bool isValidTemperature(float temp) {
  return !isnan(temp) && !isinf(temp) && temp > -50.0 && temp < 150.0;
}