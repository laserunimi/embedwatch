#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define D3 7
#define PIR_PIN 8
#define LOW 0
#define PIR_PIN 1
#define HIGH 1

#define WL_CONNECTED 0xFFFFFFFF

typedef int bool;
#define TRUE  1
#define FALSE 0

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

typedef struct wifi{
  char ssid[100];
  char password[100];
}wifi_t;

unsigned long lastMovement = 0;
unsigned long lastTransmission = 0;
wifi_t wifi;

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

void Serial_print(char *msg){
  printf("%s", msg);
}

void Serial_println(char *msg){
  printf("%s\n", msg);
}

void Serial_readString(char *buffer){
  scanf("%s", buffer);
  usleep(1000000);
}

int millis(){
  return 12;
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

void WiFi_begin(wifi_t *wifi, char *ssid, char *password){
  strcpy(wifi->ssid, ssid);
  strcpy(wifi->password, password);
}

int WiFi_status(wifi_t *wifi){
  return WL_CONNECTED;
}

void http_send(wifi_t *wifi, char *myip, char *endpoint, char *msg){
  char *header = "Content-Type\ntext/plain;charset=UTF-8";
  printf("From: %s\nTo: %s\nHeader: %s\nSending %s\n", myip, endpoint, header, msg);
}

void http_recv(wifi_t *wifi, char *buf){
  FILE *f = fopen("svr", "rb");
  if (f == NULL)
    exit(0);

  int c = 100;
  int i = 0;
  while(c > 0x20){
      c = fgetc(f);
      buf[i] = c;
      i++;
  }
  usleep(10000);
  fclose(f);
}

void sendHttpRequest(wifi_t *wifi, char *endpoint) {
  char buffer[100];
  char *myip = "192.168.1.1";

  lastTransmission = millis();

  if (WiFi_status(wifi) == WL_CONNECTED) {
    http_send(wifi, myip, endpoint, "{\"on\":true}");
    http_recv(wifi, buffer);
    Serial_println(buffer);
  } else {
    Serial_println("Request not sent: Not connected");
  }
}

void setup(char *ssid, char *password) {
  pinMode(PIR_PIN, 1);

  Serial_begin(9600);

  WiFi_begin(&wifi, ssid, password);
  while (WiFi_status(&wifi) != WL_CONNECTED) {
    delay(500);
    Serial_print(".");
  }

  Serial_println("");
  Serial_println("Connected");

  lastMovement = millis();
  lastTransmission = millis();
}


int main(int argc, char **argv){

  struct  timeval t1,t2;
  TIME_S(t1)

  unsigned long idlePeriod = 1000 * 60 * 5;
  int last = LOW;

  const char* ssid = "YOUR WIFI SSID";
  const char* password = "YOUR WIFI PASSWORD";
  const char* endpoint = "http://<BRIDGE IP>/api/<YOUR USER ID>/lights/<LIGHT NUMBER>/state";

  setup(ssid, password);

  for(int i = 0; i < 10; i++){

    if (digitalRead(PIR_PIN) == HIGH) {
      Serial_println("Movement detected");
      Serial_println("Sending ON");
      sendHttpRequest(&wifi, endpoint);
    }
    
    delay(2000);
    usleep(10000);
  }

  TIME_F(t1,t2);
  hook_exit();
  
  return 0;
}
