#pragma once
#include <cstdint>
#include <string>
using String = std::string;
#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
extern bool critical;
#define portENTER_CRITICAL(x) (critical = true)
#define portEXIT_CRITICAL(x) (critical = false)
void pinMode(int, int);
void digitalWrite(int, int);
int digitalRead(int);
void delayMicroseconds(unsigned);
unsigned long millis();
void vTaskDelay(unsigned);
int xTaskCreate(void (*)(void *), const char *, unsigned, void *, unsigned, void *);
