#include "task_serial.h"
#include "config.h"
#include <uPID.h>  // ← ДОБАВЛЕНО

// Внешние переменные
extern float sensorTemps[MAX_SENSORS];
extern float targetTemp;
extern bool systemState;
extern unsigned long heaterSeconds;
extern float currentPower;
extern uint32_t currentDuty;
extern uPID PIDregulator;  // ← теперь компилятор знает, что такое uPID

void taskSerialCommands(void* pvParameters) {
  String inputString = "";
  bool stringComplete = false;
  unsigned long lastSerialTime = 0;

#if DEBUG_SERIAL
  Serial.println(F("[SERIAL] Task started"));
  Serial.println(F("Available commands:"));
  Serial.println(F("  temp X.X     - set target temperature"));
  Serial.println(F("  pid Kp Ki Kd - set PID coefficients"));
  Serial.println(F("  on           - heater ON"));
  Serial.println(F("  off          - heater OFF"));
  Serial.println(F("  status       - show current status"));
  Serial.println(F("  reset        - reset heater timer"));
#endif

  while (1) {
    if (DEBUG_SERIAL && millis() - lastSerialTime > 5000) {
      lastSerialTime = millis();
      Serial.println("\n=== System Status ===");
      Serial.printf("Temperatures: %.2f, %.2f, %.2f\n",
                    sensorTemps[0], sensorTemps[1], sensorTemps[2]);
      Serial.printf("Target: %.1f, Heater: %s\n",
                    targetTemp, systemState ? "ON" : "OFF");
      Serial.printf("Heater time: %lu seconds\n", heaterSeconds);
      Serial.printf("Power: %.1f%% (duty: %u)\n", currentPower, currentDuty);
      Serial.println("=====================\n");
    }

    while (Serial.available()) {
      char inChar = (char)Serial.read();
      if (inChar == '\n') {
        stringComplete = true;
      } else if (inChar != '\r') {
        inputString += inChar;
      }
    }

    if (stringComplete) {
      inputString.trim();

      if (inputString == "status") {
        Serial.printf("T:%.2f,%.2f,%.2f,TRG:%.1f,HEATER:%s,TIME:%lu,PWR:%.1f\n",
                      sensorTemps[0], sensorTemps[1], sensorTemps[2],
                      targetTemp, systemState ? "ON" : "OFF",
                      heaterSeconds, currentPower);
      }
      else if (inputString.startsWith("temp ")) {
        float newTemp = inputString.substring(5).toFloat();
        if (newTemp > 0 && newTemp <= MAX_TEMP) {
          targetTemp = newTemp;
          PIDregulator.setpoint = targetTemp;
          Serial.printf("Target temperature set to: %.1f\n", targetTemp);
        } else {
          Serial.printf("Invalid temperature (0 < temp <= %.1f)\n", MAX_TEMP);
        }
      }
      else if (inputString.startsWith("pid ")) {
        int firstSpace = inputString.indexOf(' ');
        int secondSpace = inputString.indexOf(' ', firstSpace + 1);
        int thirdSpace = inputString.indexOf(' ', secondSpace + 1);

        if (firstSpace > 0 && secondSpace > 0 && thirdSpace > 0) {
          float Kp = inputString.substring(firstSpace + 1, secondSpace).toFloat();
          float Ki = inputString.substring(secondSpace + 1, thirdSpace).toFloat();
          float Kd = inputString.substring(thirdSpace + 1).toFloat();

          PIDregulator.Kp = Kp;
          PIDregulator.Ki = Ki;
          PIDregulator.Kd = Kd;

          Serial.printf("PID set to: Kp=%.2f, Ki=%.2f, Kd=%.2f\n", Kp, Ki, Kd);
        } else {
          Serial.println("Usage: pid Kp Ki Kd");
        }
      }
      else if (inputString == "on") {
        systemState = true;
        Serial.println("Heater ON");
      }
      else if (inputString == "off") {
        systemState = false;
        Serial.println("Heater OFF");
      }
      else if (inputString == "reset") {
        heaterSeconds = 0;
        Serial.println("Heater timer reset");
      }
      else if (inputString == "help") {
        Serial.println("Available commands:");
        Serial.println("  temp X.X     - set target temperature");
        Serial.println("  pid Kp Ki Kd - set PID coefficients");
        Serial.println("  on           - heater ON");
        Serial.println("  off          - heater OFF");
        Serial.println("  status       - show current status");
        Serial.println("  reset        - reset heater timer");
        Serial.println("  help         - this message");
      }
      else if (inputString.length() > 0) {
        Serial.println("Unknown command. Type 'help' for list.");
      }

      inputString = "";
      stringComplete = false;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}