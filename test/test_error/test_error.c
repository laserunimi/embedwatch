#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h> 
#include <time.h>
#include <unistd.h>

#define NEXT_PIN 6
#define TOGGLE_PIN 7
#define PREVIOUS_PIN 8
#define INPUT_PULLUP 1
#define INPUT_PULLDOWN 0

#define A0 1
#define A1 2

#define LOW 0
#define HIGH 1

//Timing Stuff
#include <time.h>
#include <sys/time.h>
#define TIME_S(tv_s) {\
                gettimeofday(&tv_s,NULL);\
            }
    
#define TIME_F(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Elapsed: %.6f\n", elapsed);\
                }

void hook_exit(){ return; }

typedef struct lcd{
  int pins[6];
  int ch;
  int line;
  char line1[32];
  char line2[32];
}lcd_t;

void Serial_readString(char *buffer){
  scanf("%s", buffer);
}

void lcd_write(lcd_t *lcd, char c){
  if(lcd->line == 0){
    lcd->line1[lcd->ch] = c;
  }else{
    lcd->line2[lcd->ch] = c;
  }
  lcd->ch++;

  if(lcd->ch >= 32){
    if(lcd->line == 1){
      lcd->line = 0;
      lcd->ch = 0;
    }
    lcd->line = 1;
    lcd->ch = 0;

  }
}

lcd_t lcd;

int main(int argc, char **argv){

  struct  timeval t1,t2;
  TIME_S(t1)

  char message[20];
  int i = 5;

  Serial_readString(message);
  lcd_write(&lcd, message[i]);

  TIME_F(t1,t2);
  hook_exit();

  return 0;
}
