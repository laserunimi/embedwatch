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

typedef struct lcd{
  int pins[6];
  int ch;
  int line;
  char line1[32];
  char line2[32];
}lcd_t;

void lcd_init(lcd_t lcd, int pin1, int pin2, int pin3, int pin4, int pin5, int pin6){
  lcd.pins[0] = pin1;
  lcd.pins[1] = pin2;
  lcd.pins[2] = pin3;
  lcd.pins[3] = pin4;
  lcd.pins[4] = pin5;
  lcd.pins[5] = pin6;
}

void lcd_begin(lcd_t lcd, int ch, int line){
  printf("Fake begin, every lcd will be init 32x2");
}

void pinMode(int pin, int status){
  printf("Now pin %d is %d\n", pin, status);
}

void analogWrite(int pin, int value){
  printf("Write analog %x to pin %d", value, pin);
}

void Serial_begin(int boud_rate){
  printf("Serial init boud rate: %d", boud_rate);
}

int Serial_available(){
  return 1;
}

void Serial_write(char *msg){
  printf("%s\n", msg);
}

void Serial_readString(char *buffer){
  scanf("%s", buffer);
  //strcpy(buffer,"dummy");
  usleep(1000000);
}

void lcd_clear(lcd_t lcd){
  strcpy(lcd.line1, "");
  strcpy(lcd.line2, "");
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

void lcd_setCursor(lcd_t lcd, int line, int ch){
  if(line <= 1 && ch <= 31){
    lcd.ch = ch;
    lcd.line = line;
  }
}

int digitalRead(int pin){
  return HIGH;
}

int analogRead(int pin){
  return 12;
}

void delay(int msec){
  printf("Fake delay of %dmsec\n", msec);
}

int map(int v0, int v1, int v2, int v3, int v4){
  return v0 + v1 + v2 + v3 + v4;
}

double previous = 0;
int pos = 0;
int delta = 1;
int previousVolume = -1;
lcd_t lcd;

void setup() {

  lcd_init(lcd, 12, 11, 5, 4, 3, 2);

  pinMode(NEXT_PIN, INPUT_PULLUP);
  pinMode(TOGGLE_PIN, INPUT_PULLUP);
  pinMode(PREVIOUS_PIN, INPUT_PULLUP);
  
  lcd_begin(lcd, 16, 2);

  analogWrite(A0, 127);
  Serial_begin(9600);
}

void loop() {

  char *message[20];

  if (Serial_available()) {
    Serial_readString(message);
    pos = 0;
    lcd_clear(lcd);
    for (int i = 0; i < 20; i++) {
      if (message[i] != '\n') {
        lcd_write(&lcd, message[i]);
      } else {
        lcd_setCursor(lcd ,0, 0);
      }
    }
  }

  if (digitalRead(NEXT_PIN) == LOW) {
    Serial_write("N");
    delay(500);
  } else if (digitalRead(PREVIOUS_PIN) == LOW) {
    Serial_write("P");
    delay(500);
  } else if (digitalRead(TOGGLE_PIN) == LOW) {
    Serial_write("T");
    delay(500);
  }

  int volume = map(analogRead(A1), 0, 1023, 0, 100);
  if (abs(volume - previousVolume) > 5) {
    if (previousVolume > 0) {
      Serial_write("V");
      Serial_write(volume);
    }
    previousVolume = volume;
    delay(200);
  }

}

int main(int argc, char **argv){

    struct  timeval t1,t2;
    TIME_S(t1)

  setup();
  for(int i = 0; i < 10; i++)
    loop();

  TIME_F(t1,t2)

  return 0;
}
