/*
 * ПИД-регулятор температуры с ШИМ управлением нагревателем и графиками
 * Особенности:
 * - Управление нагревом через ПИД-регулятор (используется библиотека uPID)
 * - Чтение температуры с 3 датчиков DS18B20
 * - Вывод информации на графический дисплей с историей температур
 * - Управление через кнопки (+/- температура, вкл/выкл)
 * - ШИМ управление с периодом 1 секунда (0-100% заполнения)
 * - Реализация на ESP32 с использованием FreeRTOS для обработки кнопок
 */

// ====================== БИБЛИОТЕКИ ======================
#include <OneWire.h>            // Для работы с датчиками DS18B20
#include <DallasTemperature.h>  // Для чтения температуры
#include <U8g2lib.h>            // Графическая библиотека для дисплея
#include <GyverButton.h>        // Удобная работа с кнопками
#include <uPID.h>               // ПИД-регулятор (замена GyverPID)
#include <driver/ledc.h>        // Аппаратный ШИМ (LEDC) для ESP32

// ====================== КОНФИГУРАЦИЯ АППАРАТУРЫ ======================
// Пиновая конфигурация для ESP32 mini D1
#define HEATER_PIN 25    // Пин управления нагревателем (ШИМ)
#define ONE_WIRE_BUS 26  // Пин для датчиков температуры (1-Wire)
#define BTN_UP_PIN 17    // Кнопка увеличения температуры
#define BTN_DOWN_PIN 16  // Кнопка уменьшения температуры
#define BTN_POWER_PIN 2  // Кнопка вкл/выкл системы

// Параметры системы
#define MAX_TEMP 70.0f             // Максимальная допустимая температура (°C)
#define PID_INTERVAL 1000          // Интервал обновления ПИД (мс)
#define TEMP_UPDATE_INTERVAL 1000  // Интервал обновления температуры (мс)

// ====================== НАСТРОЙКИ ГРАФИКОВ ======================
// const int GRAPH_START_X = 67;                             // Начало графика по X (пиксели)
const int GRAPH_START_X = 47;                             // Начало графика по X (пиксели)
const int GRAPH_END_X = 127;                              // Конец графика по X
const int GRAPH_WIDTH = GRAPH_END_X - GRAPH_START_X + 1;  // Ширина графика

// Интервалы обновления графиков
// const long GRAPH_SHIFT_INTERVAL_MS = 1875;  // Интервал сдвига графика график 2.5 мин
// const long GRAPH_SHIFT_INTERVAL_MS = 3750;  // Интервал сдвига графика график 5 мин
// const long GRAPH_SHIFT_INTERVAL_MS = 7500;  // Интервал сдвига графика график 10 мин
const long GRAPH_SHIFT_INTERVAL_MS = 22500;  // Интервал сдвига графика график 30 мин

const long GRAPH_DRAW_INTERVAL_MS = 1000;    // Интервал отрисовки графика (1 сек)

// Буферы истории для графиков
int targetHistory[GRAPH_WIDTH] = { 0 };       // История значений уставки
float sensorHistory[3][GRAPH_WIDTH] = { 0 };  // История значений датчиков (3 датчика)

// ====================== НАСТРОЙКИ ШИМ ======================
#define PWM_FREQ_HZ 1                       // Частота ШИМ 1 Гц (период 1 сек)
#define PWM_RESOLUTION 10                   // Разрешение ШИМ (10 бит = 0-1023)
#define PWM_CHANNEL LEDC_CHANNEL_0          // Канал ШИМ
#define PWM_TIMER LEDC_TIMER_0              // Таймер ШИМ
#define MAX_DUTY (1 << PWM_RESOLUTION) - 1  // Максимальное значение заполнения (1023)

// ====================== ОБЪЕКТЫ И ПЕРЕМЕННЫЕ ======================
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, 5, U8X8_PIN_NONE);  // Дисплей ST7920
OneWire oneWire(ONE_WIRE_BUS);                                // Шина 1-Wire
DallasTemperature sensors(&oneWire);                          // Датчики температуры
GButton btnUp(BTN_UP_PIN);                                    // Кнопка "+"
GButton btnDown(BTN_DOWN_PIN);                                // Кнопка "-"
GButton btnPower(BTN_POWER_PIN);                              // Кнопка питания

// ПИД-регулятор с настройками:
// Режим: дифференцирование по входу (D_INPUT) + антивиндап (I_SATURATE)
// Начальные коэффициенты: Kp=7.0, Ki=0.4, Kd=1.2
uPID PIDregulator(D_INPUT | I_SATURATE, PID_INTERVAL);

// Системные переменные
float targetTemp = 30.0;           // Целевая температура (°C)
bool systemState = false;          // Состояние системы (вкл/выкл)
DeviceAddress sensorAddresses[3];  // Адреса датчиков температуры
uint8_t sensorCount = 0;           // Количество найденных датчиков
float sensorTemps[3] = { 0 };      // Текущие показания датчиков
unsigned long startMillis = 0;     // Время старта системы
TaskHandle_t buttonsTaskHandle;    // Дескриптор задачи обработки кнопок

// Таймеры для управления системой
unsigned long lastTempUpdate = 0;  // Время последнего обновления температуры
unsigned long lastPIDUpdate = 0;   // Время последнего обновления ПИД
unsigned long lastGraphShift = 0;  // Время последнего сдвига графика
unsigned long lastGraphDraw = 0;   // Время последней отрисовки графика

// ====================== НАСТРОЙКА ШИМ ======================
void setupPWM() {
  // Конфигурация таймера ШИМ
  ledc_timer_config_t timer_conf = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = (ledc_timer_bit_t)PWM_RESOLUTION,
    .timer_num = PWM_TIMER,
    .freq_hz = PWM_FREQ_HZ,
    .clk_cfg = LEDC_AUTO_CLK
  };
  ledc_timer_config(&timer_conf);

  // Конфигурация канала ШИМ
  ledc_channel_config_t channel_conf = {
    .gpio_num = HEATER_PIN,
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = PWM_CHANNEL,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = PWM_TIMER,
    .duty = 0,  // Начальное заполнение 0%
    .hpoint = 0
  };
  ledc_channel_config(&channel_conf);
}

// ====================== ФУНКЦИЯ СДВИГА ГРАФИКА ======================
void shiftGraphLeft() {
  // Сдвигаем историю уставки на одну позицию влево
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
    targetHistory[i] = targetHistory[i + 1];
  }

  // Сдвигаем историю датчиков на одну позицию влево
  for (int j = 0; j < 3; j++) {
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      sensorHistory[j][i] = sensorHistory[j][i + 1];
    }
  }

  // Добавляем новые данные в конец массивов
  targetHistory[GRAPH_WIDTH - 1] = (int)targetTemp;  // Последнее значение уставки
  for (int j = 0; j < 3; j++) {
    sensorHistory[j][GRAPH_WIDTH - 1] = sensorTemps[j];  // Последние значения датчиков
  }
}

// ====================== ОТРИСОВКА ГРАФИКОВ ======================
void drawGraphs() {
  // Проходим по всем точкам графика
  for (int x = 0; x < GRAPH_WIDTH; x++) {
    int displayX = x + GRAPH_START_X;  // Вычисляем позицию по X

    // Отрисовка уставки (синяя линия)
    int y = map(targetHistory[x], 10, 70, 63, 0);  // Преобразуем температуру в координату Y
    y = constrain(y, 0, 63) + 10;                  // Ограничиваем и смещаем от верха
    u8g2.drawPixel(displayX, y);                   // Рисуем точку

    // Отрисовка данных датчиков
    for (int j = 0; j < 3; j++) {
      if (!isnan(sensorHistory[j][x])) {              // Если данные валидны
        y = map(sensorHistory[j][x], 10, 70, 63, 0);  // Преобразуем температуру
        y = constrain(y, 0, 63) + 10;                 // Ограничиваем и смещаем
        u8g2.drawPixel(displayX, y);                  // Рисуем точку
      }
    }
  }
}

// ====================== НАСТРОЙКА СИСТЕМЫ ======================
void setup() {
  Serial.begin(115200);

  // Настройка дисплея
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Initializing...");
  u8g2.sendBuffer();

  // Настройка кнопок
  btnUp.setDebounce(50);  // Установка антидребезга 50 мс
  btnDown.setDebounce(50);
  btnPower.setDebounce(50);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
  pinMode(BTN_POWER_PIN, INPUT_PULLUP);

  // Настройка ШИМ
  setupPWM();
  ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);

  // Инициализация датчиков температуры
  sensors.begin();
  sensorCount = sensors.getDeviceCount();

  // Настройка найденных датчиков
  for (uint8_t i = 0; i < min<uint8_t>(sensorCount, 3); i++) {
    if (sensors.getAddress(sensorAddresses[i], i)) {
      sensors.setResolution(sensorAddresses[i], 12);  // Установка максимального разрешения
    }
  }

  // Настройка ПИД-регулятора
  PIDregulator.Kp = 7.0;               // Пропорциональный коэффициент
  PIDregulator.Ki = 0.4;               // Интегральный коэффициент
  PIDregulator.Kd = 1.2;               // Дифференциальный коэффициент
  PIDregulator.setpoint = targetTemp;  // Установка уставки
  PIDregulator.outMax = MAX_DUTY / 2;  // Ограничение максимального выхода
  PIDregulator.outMin = 0;             // Ограничение минимального выхода

  // Создание задачи FreeRTOS для обработки кнопок (выполняется на ядре 0)
  xTaskCreatePinnedToCore(
    buttonsTask,         // Функция задачи
    "buttonsTask",       // Имя задачи
    2048,                // Размер стека
    NULL,                // Параметры
    1,                   // Приоритет
    &buttonsTaskHandle,  // Дескриптор задачи
    0                    // Ядро процессора
  );

  // Инициализация графиков начальными значениями
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    targetHistory[i] = targetTemp;
    for (int j = 0; j < 3; j++) {
      sensorHistory[j][i] = 0;
    }
  }

  startMillis = millis();  // Запоминаем время старта
}

// ====================== ОСНОВНОЙ ЦИКЛ ======================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Обновление температуры (раз в секунду)
  if (currentMillis - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {
    lastTempUpdate = currentMillis;
    sensors.requestTemperatures();  // Запрос температуры со всех датчиков

    // Чтение температуры с каждого датчика
    for (uint8_t i = 0; i < min<uint8_t>(sensorCount, 3); i++) {
      float temp = sensors.getTempC(sensorAddresses[i]);
      sensorTemps[i] = (temp == DEVICE_DISCONNECTED_C) ? NAN : temp;
    }
  }

  // 2. Обновление ПИД и ШИМ (раз в PID_INTERVAL мс)
  if (currentMillis - lastPIDUpdate >= PID_INTERVAL) {
    lastPIDUpdate = currentMillis;

    if (systemState && !isnan(sensorTemps[0])) {
      // Вычисляем выход ПИД-регулятора
      float pidOutput = PIDregulator.compute(sensorTemps[0]);

      // Преобразуем выход ПИД в значение ШИМ (0-MAX_DUTY)
      uint32_t duty = (uint32_t)pidOutput * 2;
      duty = constrain(duty, 0, MAX_DUTY);

      // Устанавливаем заполнение ШИМ
      ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, duty);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
    } else {
      // Если система выключена или ошибка датчика - выключаем нагрев
      ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 0);
      ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
    }
  }

  // 3. Управление графиками (сдвиг раз в 10 секунд)
  if (currentMillis - lastGraphShift >= GRAPH_SHIFT_INTERVAL_MS) {
    lastGraphShift = currentMillis;
    shiftGraphLeft();  // Сдвигаем графики влево
  }

  // 4. Обновление интерфейса (раз в секунду)
  if (currentMillis - lastGraphDraw >= GRAPH_DRAW_INTERVAL_MS) {
    lastGraphDraw = currentMillis;
    drawInterface();  // Отрисовываем интерфейс
  }

}

// ====================== ЗАДАЧА ОБРАБОТКИ КНОПОК (FreeRTOS) ======================
void buttonsTask(void* pvParameters) {
  for (;;) {
    // Обработка состояний кнопок
    btnUp.tick();
    btnDown.tick();
    btnPower.tick();

    // Обработка нажатия кнопки питания
    if (btnPower.isClick()) {
      systemState = !systemState;  // Переключаем состояние системы
      if (systemState) {
        startMillis = millis();  // Сбрасываем таймер при включении
      }
    }

    // Обработка кнопки увеличения температуры
    if (btnUp.isClick() && targetTemp < MAX_TEMP) {
      targetTemp += 1.0;
      PIDregulator.setpoint = targetTemp;  // Обновляем уставку ПИД
    }

    // Обработка кнопки уменьшения температуры
    if (btnDown.isClick() && targetTemp > 0) {
      targetTemp -= 1.0;
      PIDregulator.setpoint = targetTemp;  // Обновляем уставку ПИД
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // Задержка для снижения нагрузки
  }
}

// ====================== ОБНОВЛЕНИЕ ИНТЕРФЕЙСА ======================
void drawInterface() {
  u8g2.clearBuffer();  // Очищаем буфер дисплея

  // 1. Отображение времени работы системы
  u8g2.setFont(u8g2_font_6x10_tf);
  unsigned long elapsed = millis() - startMillis;
  uint8_t hours = (elapsed / 3600000) % 100;
  uint8_t minutes = (elapsed / 60000) % 60;
  uint8_t seconds = (elapsed / 1000) % 60;
  char timeStr[9];
  sprintf(timeStr, "%02u:%02u:%02u", hours, minutes, seconds);
  u8g2.drawStr(80, 8, timeStr);

  // 2. Отображение целевой температуры и статуса системы
  u8g2.setFont(u8g2_font_gb24st_t_3);  // Большой шрифт для температуры
  char tempStr[10];
  dtostrf(targetTemp, 2, 0, tempStr);  // Преобразуем float в строку
  u8g2.drawStr(0, 15, tempStr);

  u8g2.setFont(u8g2_font_6x10_tf);  // Меньший шрифт для статуса
  u8g2.drawStr(25, 8, systemState ? "===ON===" : "   OFF");

  // 3. Отображение температур с датчиков
  u8g2.setFont(u8g2_font_gb24st_t_3);
  for (uint8_t i = 0; i < min<uint8_t>(sensorCount, 3); i++) {
    if (!isnan(sensorTemps[i])) {
      dtostrf(sensorTemps[i], 4, 1, tempStr);  // Форматируем температуру
      u8g2.drawStr(0, 15 * (i + 2), tempStr);  // Выводим с отступом
    } else {
      u8g2.drawStr(0, 15 * (i + 2), "Err");  // Ошибка датчика
    }
  }

  // 4. Отрисовка графиков температур
  drawGraphs();

  u8g2.sendBuffer();  // Отправляем буфер на дисплей
}