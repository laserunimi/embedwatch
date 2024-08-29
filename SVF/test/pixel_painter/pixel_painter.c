#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Place for configuration
#define PIN             4   // Where's that data in pin at?
#define NUMPIXELS      60   // How many pixels to you have? <- This is also how many pixels high your image is
#define COLUMN_DELAY  100   // For how long should 1 column be visible?
#define COUNTDOWN_SECS  5   // How many seconds should the countdown be?
#define IMAGE_COLUMNS  78   // How many pixels wide is your image?
#define OLED_RESET 4
#define SSD1306_SWITCHCAPVCC 0
#define WHITE 0xffffff

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

typedef struct neopixel{
  int numpixels;
  int pin;
  int other;
} neopixel_t;

typedef struct display{
  char* line1[20];
  char* line2[20]
} display_t;

neopixel_t pixels;
display_t display;

void Adafruit_NeoPixel(int numpixels, int pin, int other){
  printf("Init NeoPixel\n");
  pixels.numpixels = numpixels;
  pixels.pin = pin;
  pixels.other = other;
}

void begin(display_t *d, int num1, int num2){
  printf("The display has been inited\n");
  strcpy(d->line1, "Init");
}

void show(display_t *d){
  printf("[LINE 1] %s", d->line1);
  printf("[LINE 2] %s", d->line2);
}

void setTextSize(display_t *d, int num){ printf("fake setTextSize\n"); }
void setTextColor(display_t *d, int num){ printf("fake setTextColor\n"); }
void setCursor(display_t *d, int num1, int num2){ printf("fake setCursor\n"); }
void clearDisplay(display_t *d){
  strcpy(d->line1, "");
  strcpy(d->line2, "");
}
void println(display_t *d, const char* str){
  strcpy(d->line1, str);
}

void showImage(neopixel_t p){

}

int delay(int n){
  usleep(n*100);
}

void setPixelColor(neopixel_t p, int y){
  printf("Set new color\n");
}

// Nothing really worth mentioning here
void setup() {

  // Setup that neopixel
  Adafruit_NeoPixel(NUMPIXELS, PIN, 5);

  begin(&display, SSD1306_SWITCHCAPVCC, 0x3C);
  show(&display);

  setTextSize(&display, 2);
  setTextColor(&display, WHITE);
  setCursor(&display, 10,0);
  clearDisplay(&display);
  println(&display, "Running");
  show(&display);
}

void loop() {

  char *buffer[100];

  // Turn off all pixels
  for (int i = 0; i < NUMPIXELS; i++) {
    setPixelColor(pixels, i);
  }
  show(&display);
  delay(3000);

  // Ready, set, go!
  for (int i = 0; i < COUNTDOWN_SECS; i++) {
    // Set both top and bottom (to allow easier alignment in camera)
    setPixelColor(pixels, i);
    setPixelColor(pixels, NUMPIXELS - 1 - i);
    showImage(pixels);
    delay(1000);
  }

  FILE *fp = fopen("image.png", "r");;

  if (fp == NULL)
      exit(0);

  int c = 100;
  int i = 0;
  while(c > 0x20){

    c = fgetc(fp); // read char from image

    // Do some computer science stuff to the color
    // (This is bitshifting, check it out - it's a great read. Seriously)
    int r = (c & 0xFF0000) >> 16;
    int g = (c & 0xFF00) >> 8;
    int b = (c & 0xFF);

    // Finally set the color of that particular pixel
    setPixelColor(pixels, 10);
    
    showImage(pixels);
    buffer[i] = c;
    i++;
  }
  fclose(fp);
}

int main(int argc, char **argv){

  struct  timeval t1,t2;
  TIME_S(t1)

  setup();
  loop();

  TIME_F(t1,t2)

  return 0;
}
