#ifndef TASK_HEATER_H
#define TASK_HEATER_H

#include <Arduino.h>

// Глобальная переменная времени нагрева (объявлена как extern)
extern unsigned long heaterSeconds;

// Прототип задачи
void taskHeaterControl(void* pvParameters);

#endif