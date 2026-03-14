// ============================================================================
// display_graph.h - Отрисовка графиков температур на дисплее
// ============================================================================

#ifndef DISPLAY_GRAPH_H
#define DISPLAY_GRAPH_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

// Инициализация буферов графиков начальными значениями
void initGraphBuffers(int* targetHist, float sensorHist[][GRAPH_WIDTH], 
                      float targetTemp, int numSensors);

// Сдвиг графиков влево (добавление новых данных в конец)
void shiftGraphsLeft(int* targetHist, float sensorHist[][GRAPH_WIDTH],
                     float targetTemp, float* sensorTemps, int numSensors);

// Отрисовка всех графиков на дисплее
void drawAllGraphs(U8G2* u8g2, int* targetHist, float sensorHist[][GRAPH_WIDTH],
                   int numSensors);

#endif