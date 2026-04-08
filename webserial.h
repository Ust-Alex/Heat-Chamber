/**
 * @file webserial.h
 * @brief Заголовочный файл модуля WebSerial
 * 
 * Обеспечивает удалённый доступ к консоли ESP32 через веб-браузер.
 * Позволяет отправлять команды и видеть вывод Serial.
 * 
 * @note Доступен по адресу: http://IP_ESP32/webserial
 */

#ifndef WEBSERIAL_H
#define WEBSERIAL_H

#include <ESPAsyncWebServer.h>

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ WEBSERIAL
// ============================================================================
/**
 * @brief Настройка WebSerial и веб-сервера
 * 
 * Подключается к существующему веб-серверу и добавляет эндпоинт /webserial.
 * Должна вызываться после запуска веб-сервера.
 * 
 * @param server Указатель на существующий AsyncWebServer
 */
void setupWebSerial(AsyncWebServer* server);

// ============================================================================
// ОТПРАВКА СООБЩЕНИЙ
// ============================================================================
/**
 * @brief Отправка сообщения в WebSerial (с переводом строки)
 * @param msg Текст сообщения
 */
void webSerialPrintln(const char* msg);

/**
 * @brief Отправка сообщения в WebSerial (без перевода строки)
 * @param msg Текст сообщения
 */
void webSerialPrint(const char* msg);

/**
 * @brief Отправка форматированного сообщения (аналог printf)
 * @param format Строка формата
 * @param ... Аргументы для форматирования
 */
void webSerialPrintf(const char* format, ...);

#endif // WEBSERIAL_H