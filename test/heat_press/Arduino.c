#include "Arduino.h"
#include <time.h>
#include <stdlib.h>


void pinMode(int pin, uint32_t value){ pins_state[pin] = value; }
void digitalWrite(int pin, uint32_t value){ pins[pin] = value; }

bool startWaveform(int pin, int v1, int refresh_interval, int v2){ printf("start waveform on pin %d\n", pin); return true; }
bool startWaveform2(int pin, int v1, int refresh_interval, int v2, int phaseReference){ printf("start waveform on pin %d\n", pin); return true; }
void delay(int refresh_interval){ usleep(refresh_interval * 1000 ); }
bool stopWaveform(int pin){ printf("end waveform on pin %d\n", pin); }
int max(int v1, int v2){ return v1 >= v2 ? v1 : v2; }
int min(int v1, int v2){ return v1 >= v2 ? v2 : v1; }
int constrain(int value, int v1, int v2){ }

int analogRead(int pin){ 
    srand(time(NULL));
    return rand();
}

long map(long x, long in_min, long in_max, long out_min, long out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

unsigned long millis_ctr = 0;

unsigned long millis(){
    millis_ctr++;
    return millis_ctr;
}

