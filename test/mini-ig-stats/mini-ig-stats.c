#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> 
#include <time.h>

//uncomment this line if using a Common Anode LED
#define COMMON_ANODE

#define D1 1
#define D2 2
#define D3 3

#define OUTPUT 1

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

unsigned int nextTime = 0;

typedef struct http_request{
    char hostname[100];
    int port;
    char path[100];
}http_request_t;

typedef struct http_response{
    char body[200];
}http_response_t;

char* headers[] = {"Accept" , "*/*"};

// ---- Memory ----
int lastFollowerCount = 0;
int lastLikeCount = 0;
int lastCommentCount = 0;

char publishString[40]; // Used for debugging to the Particle console

void delay(int msec){
  usleep(msec*100);
}

void pinMode(int pin, int status){
  printf("Now pin %d is %d\n", pin, status);
}

void analogWrite(int pin, int color){
    printf("PIN: %d, color: %d\n", pin, color);
} 

void setColor(int red, int green, int blue) {
  #ifdef COMMON_ANODE
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  #endif
  analogWrite(D1, red);
  analogWrite(D3, green);
  analogWrite(D2, blue);  
}

void handleResult(int followerDelta, int likeDelta, int commentDelta) {
    setColor(0, 0, 0); // Turn off LED
    
    // Favor "Followers" over "Comments"
    if (followerDelta > 0) {
        setColor(0, 0, 255);
    } else if (commentDelta > 0) { // Favor "Comments" over "Likes"
        setColor(0, 255, 0);
    } else if (likeDelta > 0) {
        setColor(255, 0, 0);
    }
}

void getHttpPage(http_request_t *request, http_response_t *response, char *headers){

    char resource[200];

    sprintf(resource, "%s%s", request->hostname, request->path);
    printf("Asking for %s\n", resource);

    FILE *f = fopen(resource, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET); 

    printf("The web resource is %d bytes\n", fsize);

     if (f == NULL)
      exit(0);

    int c = 100;
    int i = 0;
    while(c > 0x20){
        c = fgetc(f);
        response->body[i] = c;
        i++;
    }

    fclose(f);
    delay(200);
}

int fetchInteger(http_request_t *request, http_response_t *response, char *headers) {
    char* count[10];

    getHttpPage(request, response, headers);
    sprintf(count, "%d", strlen(response->body));
    return atoi(count);
}

int getFollowers(http_request_t *request, http_response_t *response, char *headers) {
    strcpy(request->path, "followers");
    return fetchInteger(request, response, headers);
}

int getLikes(http_request_t *request, http_response_t *response, char *headers) {
    strcpy(request->path, "likes");
    return fetchInteger(request, response, headers);
}

int getComments(http_request_t *request, http_response_t *response, char *headers) {
    strcpy(request->path, "comments");
    return fetchInteger(request, response, headers);
}

void publish(char *str, char *buffer){
    printf("str: %s, buffer: %s", str, buffer);
}

void setup(http_request_t *request) {
    // Setup the endpoint for the Node.js server
    strcpy(request->hostname, "");
    request->port = 80;
    
    // (PWM) Pins for RGB Led
    pinMode(D1, OUTPUT); // Red
    pinMode(D2, OUTPUT); // Blue
    pinMode(D3, OUTPUT); // Green
    
    // Test LEDs
    setColor(255, 0, 0);
    delay(2000);
    setColor(0, 255, 0);
    delay(2000);
    setColor(0, 0, 255);
    delay(2000);
    setColor(0, 0, 0);
}

int main(int argc, char **argv){

    struct  timeval t1,t2;
    TIME_S(t1)

    http_response_t response;
    http_request_t request;

    setup(&request);

    int followerCount = getFollowers(&request, &response, headers);
    int followerDelta = followerCount - lastFollowerCount;
    
    int likeCount = getLikes(&request, &response, headers);
    int likeDelta = likeCount - lastLikeCount;
    
    int commentCount = getComments(&request, &response, headers);
    int commentDelta = commentCount - lastCommentCount;
    
    // Skip the first run
    if (lastFollowerCount > 0 && lastLikeCount > 0 && lastFollowerCount > 0) {
        handleResult(followerDelta, likeDelta, commentDelta);
    }
    
    sprintf(publishString, "F: %d, L: = %d, C: %d", followerCount, likeCount, commentCount);
    publish("InstagramStats", publishString);

    lastLikeCount = likeCount;
    lastFollowerCount = followerCount;
    lastCommentCount = commentCount;

    TIME_F(t1,t2);
    hook_exit();
    
    return 0;
}

