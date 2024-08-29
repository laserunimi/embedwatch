#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>

#define SECRET_SSID "<SSID HERE>"
#define SECRET_PASS "<PASSWORD HERE>"

#define BUTTON_PIN 2
#define GREEN_PIN 4
#define RED_PIN 3
#define BUSY_PIN 5

#define INPUT_PULLUP 0x2
#define INPUT 1
#define OUTPUT 0

#define WL_CONNECTED 0xf1f1f1f1
#define WL_NO_SHIELD 0x54545454

#define HIGH 1
#define LOW 0
#define WL_IDLE_STATUS 0xffffffff

#define MP3_IDLE 0
#define MP3_PLAY 1
#define MP3_PAUSE 2
#define MP3_LOAD 3

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

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;
int fake_write_reg = 0;

const int GMT = 2; //change this to adapt it to your time zone

typedef struct wifi{
  char ssid[64];
  char password[32];
}wifi_t;

typedef struct mp3{
  int status;
  int freq;
  int volume;
  char buf[200];
  char path[100];
  char song_name[40];
}mp3_t;


void pinMode(int pin, int status){
  fake_write_reg = status;
  printf("Now pin %d is %d\n", pin, fake_write_reg);
}

void digitalWrite(int pin, int value){
  fake_write_reg = value;
  printf("Write digital %x to pin %d", fake_write_reg, pin);
}

void Serial_begin(int boud_rate){
  fake_write_reg = boud_rate;
  printf("Serial init boud rate: %d", boud_rate);
}

void Serial_print(char *msg){
  printf("%s", msg);
}

void Serial_println(char *msg){
  printf("%s\n", msg);
}

void delay(int msec){
  printf("Fake delay of %dmsec\n", msec);
  usleep(msec*100);
}

int WiFi_status(wifi_t *wifi){
  int r = rand() % 10;

  if(r < 8)
    return WL_CONNECTED;
  return WL_NO_SHIELD;
}

void WiFi_begin(wifi_t *wifi, char *ssid, char *password){
  strcpy(wifi->ssid, ssid);
  strcpy(wifi->password, password);
}

void printWiFiStatus(wifi_t *wifi){
  printf("*****************\n");
  printf("Connected: %s\n", wifi->ssid);
  printf("Wifi status: OK\n");
  printf("*****************\n");
}

void setupWifi(wifi_t *wifi) {
  Serial_println("==== Setting up wifi...");
  int attempt = 0;

  // attempt to connect to WiFi network:
  do {
    Serial_print("Attempting to connect to SSID: ");
    Serial_println(ssid);
    WiFi_begin(wifi, ssid, pass);
    delay(10000);
    if (++attempt == 4) {
      digitalWrite(RED_PIN, HIGH);
      while(1) {
        delay(1000);
      }
    }

  }while( WiFi_status(wifi) != WL_CONNECTED);

  digitalWrite(GREEN_PIN, HIGH);
  delay(1000);
  digitalWrite(GREEN_PIN, LOW);

  printWiFiStatus(wifi);
}

void myDFPlayer_begin(mp3_t *mp3){
  mp3->volume = 5;
  mp3->freq = 44100;
  mp3->status = MP3_IDLE;
}

void myDFPlayer_readState(mp3_t *mp3){
  printf("*****************\n");
  printf("MP3 status: %d\n", mp3->status);
  printf("Volume: %d\n", mp3->volume);
  if(mp3->status == MP3_PLAY){
    printf("Song: %s\n", mp3->song_name);
  }
  printf("*****************\n");
}

int myDFPlayer_readVolume(mp3_t *mp3){
  return mp3->volume;
}

void myDFPlayer_volume(mp3_t *mp3, int value){
  mp3->volume = value;
} 

int myDFPlayer_readFileCounts(mp3){

  size_t count = 0;
  struct dirent *res;
  struct stat sb;
  const char *path = ".";

  if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)){
      DIR *folder = opendir ( path );

      if (access ( path, F_OK ) != -1 ){
          if ( folder ){
              while ( ( res = readdir ( folder ) ) ){
                  if ( strcmp( res->d_name, "." ) && strcmp( res->d_name, ".." ) ){
                      printf("%zu) - %s\n", count + 1, res->d_name);
                      count++;
                  }
              }

              closedir ( folder );
          }else{
              perror ( "Could not open the directory" );
              exit( EXIT_FAILURE);
          }
      }

  }else{
      printf("The %s it cannot be opened or is not a directory\n", path);
      exit( EXIT_FAILURE);
  }

  printf( "\n\tFound %zu Files\n", count );
  return count;
}

void myDFPlayer_select(mp3_t *mp3){
  strcpy(mp3->path, "song.mp3");
}

void myDFPlayer_play(mp3_t *mp3){

  printf("*** open: %s ***\n", mp3->path);

  FILE *f = fopen(mp3->path, "rb");
  if (f == NULL)
    exit(0);

  int c = 100;
  int i = 0;
  while(c > 0x20){
      c = fgetc(f);
      mp3->buf[i] = c;
      i++;
  }
  fclose(f);

  printf("*** play ***\n");

  for(int i = 0; i < 1024; i++){
    fake_write_reg += mp3->buf[i] % 10000;
    fake_write_reg = fake_write_reg *4;
  }

  printf("*** play finish ***\n");
}

void SetupMP3(mp3_t *mp3) {
  Serial_print("MP3:Init");

  myDFPlayer_begin(mp3);
  Serial_print("OK ");
  myDFPlayer_readState(mp3);

  Serial_println("MP3:Config");

  //----Set volume----
  myDFPlayer_volume(mp3, 3);  

  //----Set different EQ----
  //myDFPlayer_EQ(DFPLAYER_EQ_NORMAL);

  //----Read imformation----
  Serial_print("MP3:readState:\n");
  myDFPlayer_readState(mp3); //read mp3 state
  Serial_print("MP3:readFC:");
  myDFPlayer_readFileCounts(mp3); //read all file counts in SD card
  Serial_print("MP3:readCFN:");
  myDFPlayer_select(mp3); //read current play file number
}

void setup(mp3_t *mp3, wifi_t *wifi) {
  Serial_begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(BUSY_PIN, INPUT);

  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(RED_PIN, LOW);

  SetupMP3(mp3);
  setupWifi(wifi);

  digitalWrite(GREEN_PIN, HIGH);

  Serial_println("==== ALL GOOD TO GO!");
}

int main(int argc, char **argv){
  struct  timeval t1,t2;
  TIME_S(t1)
  mp3_t mp3;
  wifi_t wifi;
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);

  setup(&mp3, &wifi);

  Serial_println("Button Pressed");

  t = time(NULL);
  printf("now: %d-%02d-%02d %02d:%02d:%02d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  myDFPlayer_play(&mp3);
  delay(1000);
  

  delay(10);

  TIME_F(t1,t2);
  return 0;
}


