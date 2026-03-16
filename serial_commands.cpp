// ============================================================================
// serial_commands.cpp - Управление через Serial
// ============================================================================

#include "serial_commands.h"
#include "config.h"

extern float sensorTemps[MAX_SENSORS];
extern float currentPower;
extern float targetTemp;
extern bool systemState;

void taskSerialCommands(void* pvParameters) {
    // Убрал лишнее сообщение о старте задачи, его и так видно в rtos_tasks
    String input = "";
    
    while(1) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                input.trim();
                
                if (input == "help") {
                    Serial.println(F("\n📋 Доступные команды:"));
                    Serial.println(F("  help        - этот список"));
                    Serial.println(F("  status      - текущее состояние"));
                    Serial.println(F("  set_target N- установка уставки (30-70)"));
                    Serial.println(F("  power on/off- включить/выключить нагрев"));
                    Serial.println(F("  reset       - перезагрузка ESP"));
                    
                } else if (input == "status") {
                    Serial.printf("\n📊 Состояние:\n");
                    Serial.printf("  Уставка: %.1f°C\n", targetTemp);
                    Serial.printf("  Система: %s\n", systemState ? "ON" : "OFF");
                    Serial.printf("  Мощность: %.1f%%\n", currentPower);
                    Serial.printf("  Датчики: T0=%.2f T1=%.2f T2=%.2f\n", 
                                  sensorTemps[0], sensorTemps[1], sensorTemps[2]);
                    
                } else if (input.startsWith("set_target")) {
                    int val = input.substring(11).toInt();
                    if (val >= 30 && val <= 70) {
                        targetTemp = val;
                        Serial.printf("✅ Уставка установлена: %d°C\n", val);
                    } else {
                        Serial.println("❌ Диапазон 30-70°C");
                    }
                    
                } else if (input == "power on") {
                    systemState = true;
                    Serial.println("✅ Нагрев включён");
                } else if (input == "power off") {
                    systemState = false;
                    Serial.println("✅ Нагрев выключён");
                    
                } else if (input == "reset") {
                    Serial.println("🔄 Перезагрузка...");
                    delay(100);
                    ESP.restart();
                    
                } else if (input.length() > 0) {
                    Serial.println("❌ Неизвестная команда. Введите 'help'");
                }
                
                input = "";
            } else {
                input += c;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}