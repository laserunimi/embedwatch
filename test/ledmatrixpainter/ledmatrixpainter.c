#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

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

typedef struct serial{
  int boud_rate;
}serial_t;

typedef struct matrix{
  int intensity;
  int m[5][5];
}matrix_t;

void Serial_begin(serial_t *serial, int value){
  serial->boud_rate = value;
}

char Serial_read(serial_t *serial){
  char buf[1];
  scanf("%s", buf);
  usleep(10000);
  return buf[0];
}

void matrix_setIntensity(matrix_t *matrix, int intensity){
  matrix->intensity = intensity;
}

void matrix_fillScreen(matrix_t *matrix, int value){
  for(int i = 0; i < 5; i++)
    for(int j = 0; j < 5; j++)
      matrix->m[i][j] = value;
}

void matrix_drawPixel(matrix_t *matrix, int x, int y, int value){
  matrix->m[x][y] = value;
}

void  matrix_write(matrix_t *matrix){
  printf("-------------\n");
  for(int i = 0; i < 5; i++){
    for(int j = 0; j < 5; j++){
      if(matrix->m[i][j] == LOW)
        printf("- ");
      else
        printf("* ");
    }
    printf("\n");
  }
  printf("-------------\n");
}

void setup(serial_t *serial, matrix_t *matrix) {
  Serial_begin(serial, 115200);
  matrix_setIntensity(matrix, 15);
  matrix_fillScreen(matrix, LOW);
  matrix_write(matrix);
}

int Serial_available(serial_t *serial){
  printf("Serial_available: ignoring serial and returning true\n");
  return 1;
}

int main(int argc, char **argv) {

  struct  timeval t1,t2;
  TIME_S(t1)

  int bytesRecieved = 0;
  char bytes[2];
  int pinCS = 10;
  int numberOfHorizontalDisplays = 5;
  int numberOfVerticalDisplays = 1;

  serial_t serial;
  matrix_t matrix;

  setup(&serial, &matrix);

  for(int i = 0; i < 10; i ++){

    if (Serial_available(&serial)) {
      while (bytesRecieved < 2) {
        bytes[bytesRecieved++] = Serial_read(&serial);
      }

      bytesRecieved = 0;

      if (bytes[0] == 0xff && bytes[1] == 0xff) {
        matrix_fillScreen(&matrix, LOW);
        matrix_write(&matrix);
        bytes[0] = 0x00;
        bytes[1] = 0x00;
        return 1;
      }

      unsigned int val = bytes[1] * 256 + bytes[0];

      int state = val >= 1000 ? HIGH : LOW;
      if (val >= 1000) {
        val -= 1000;
      }

      int y = (val / (numberOfHorizontalDisplays * 8)) % 10;
      int x = (val % (numberOfHorizontalDisplays * 8)) % 10;
      matrix_drawPixel(&matrix, x, y, state);
      matrix_write(&matrix);
    }

    usleep(10000);
  }

  TIME_F(t1,t2);
  hook_exit();

  return 0;
}

