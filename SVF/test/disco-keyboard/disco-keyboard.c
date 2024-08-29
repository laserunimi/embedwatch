#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>
#include <unistd.h>

#define PIN            9
#define NUMPIXELS      10

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

// Change the values to represent you own keyboard
int keyboard[4][14] = {
  {167, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 43, 180, 8},
  {9, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 229, 168, 10},
  {9, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'o', 228, 39, 10},
  {9, 60, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 9, 10},
};

typedef struct pixel{
  int numpixels;
  int pin;
  int mat[10][10];
} pixel_t;

typedef struct display{
  char* line1[20];
  char* line2[20];
} display_t;

void Serial_read(char *buf){
  printf("Serial:");
  scanf("%s", buf);
  //int ret;
  //char *p = myscanf("%s", buf, &ret);
  usleep(1000000);
}

// wscanf("%d %s", int, char*) non posso restituire due valori

void pixel_matrix_init(pixel_t *pixel, int num, int pin){
  pixel->pin = pin;
  for(int i = 0; i < num; i++)
    for(int j = 0; j < num; j++)
      pixel->mat[i][j] = 0;
}

void setup(pixel_t *pixel) {
  pixel_matrix_init(pixel, NUMPIXELS, PIN);
}

int color(int r, int g, int b){
  return r + g + b;
}

void setPixelColor(pixel_t *pixel, int pos, int color){
  pixel->mat[pos % NUMPIXELS][pos %NUMPIXELS] = color;
}

void show(pixel_t *pixel){
  for(int i = 0; i < pixel->numpixels; i++){
    for(int j = 0; j < pixel->numpixels; j++)
      printf("%d", pixel->mat[i][j]);
    printf("\n");
  }
}

void delay(int time){
  printf("Fake delay %d\n", time);
}

int main(int argc, char **argv) {

  struct  timeval t1,t2;
  TIME_S(t1)

  char *buf[5];
  pixel_t pixel;
  
  setup(&pixel);
  Serial_read(buf);
  int r = atoi(buf);

  // Handle space slightly different
  if (r == 32) {
    for (int i = 0; i < NUMPIXELS; i++) {
      setPixelColor(&pixel, i, color(100, 100, 100));
    }
    show(&pixel);
    delay(20);
    return 1;
  }

  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 14; j++) {
      if (r == keyboard[i][j]) {
        Serial_read(buf);
        int r = atoi(buf);
        Serial_read(buf);
        int g = atoi(buf);
        Serial_read(buf);
        int b = atoi(buf);

        setPixelColor(&pixel, j, color(r, g, b));
        show(&pixel);
      }
    }
  }

  delay(20);

  TIME_F(t1,t2)

  return 0;
}
