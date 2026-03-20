// ============================================================================
// settings.h - Сохранение настроек в Preferences
// ============================================================================

#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <Preferences.h>

// ============================================================================
// ВНЕШНИЕ ПЕРЕМЕННЫЕ (для отладки сохранения)
// ============================================================================
extern unsigned long heaterSeconds;  // общее время нагрева

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ И ЧТЕНИЕ НАСТРОЕК
// ============================================================================
inline void initSettings(float &targetTemp, bool &systemState) {
  Preferences preferences;
  preferences.begin("heater", false);
  
  targetTemp = preferences.getFloat("target", targetTemp);
  systemState = preferences.getBool("state", systemState);
  
  // Загружаем время нагрева (если нет — 0)
  heaterSeconds = preferences.getULong("heaterTime", 0);
  
  preferences.end();
  
  Serial.println(F("[SETTINGS] Настройки загружены:"));
  Serial.printf("[SETTINGS]   Уставка: %.1f°C\n", targetTemp);
  Serial.printf("[SETTINGS]   Режим: %s\n", systemState ? "ON" : "OFF");
  
  // Форматируем время для вывода
  unsigned long hours = heaterSeconds / 3600;
  unsigned long minutes = (heaterSeconds % 3600) / 60;
  unsigned long seconds = heaterSeconds % 60;
  Serial.printf("[SETTINGS]   Время нагрева: %02lu:%02lu:%02lu\n", hours, minutes, seconds);
}

// ============================================================================
// СОХРАНЕНИЕ НАСТРОЕК (с периодичностью)
// ============================================================================
inline void saveSettings(float targetTemp, bool systemState) {
  static float lastSavedTarget = -1;
  static bool lastSavedState = false;
  
  if (targetTemp != lastSavedTarget || systemState != lastSavedState) {
    Preferences preferences;
    preferences.begin("heater", false);
    
    preferences.putFloat("target", targetTemp);
    preferences.putBool("state", systemState);
    
    preferences.end();
    
    lastSavedTarget = targetTemp;
    lastSavedState = systemState;
    
    Serial.printf("[SETTINGS] Настройки сохранены: %.1f°C, %s\n", 
                  targetTemp, systemState ? "ON" : "OFF");
  }
}

// ============================================================================
// СОХРАНЕНИЕ ВРЕМЕНИ НАГРЕВА (вызывать отдельно, раз в 5 минут)
// ============================================================================
inline void saveHeaterTime() {
  static unsigned long lastSavedTime = 0;
  
  // Сохраняем только если значение изменилось
  if (heaterSeconds != lastSavedTime) {
    Preferences preferences;
    preferences.begin("heater", false);
    
    preferences.putULong("heaterTime", heaterSeconds);
    
    preferences.end();
    
    lastSavedTime = heaterSeconds;
    
    // Форматируем время для отладки
    unsigned long hours = heaterSeconds / 3600;
    unsigned long minutes = (heaterSeconds % 3600) / 60;
    unsigned long seconds = heaterSeconds % 60;
    Serial.printf("[SETTINGS] Время нагрева сохранено: %02lu:%02lu:%02lu\n", 
                  hours, minutes, seconds);
  }
}

#endif // SETTINGS_H