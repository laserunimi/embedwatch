#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

#define SDA 12
#define SCL 13

#define A0 14
#define A1 15
#define A2 16
#define A3 17
#define A4 18
#define A5 19

uint8_t pins_state[1024];
uint32_t pins[1024];

void pinMode(int pin, uint32_t value);
void digitalWrite(int pin, uint32_t value);

bool startWaveform(int pin, int v1, int refresh_interval, int v2);
bool startWaveform2(int pin, int v1, int refresh_interval, int v2, int phaseReference);
void delay(int refresh_interval); // long enough to complete active period under all circumstances.
bool stopWaveform(int pin);
int max(int v1, int v2);
int min(int v1, int v2);
int constrain(int value, int v1, int v2);

// Mocked
int analogRead(int);
int pulseIn(int, int, int);
long map(long x, long in_min, long in_max, long out_min, long out_max);

unsigned long millis();