#include <stdio.h>
#include <stdlib.h>    
#include <err.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

//Timing Stuff
#include <time.h>
#include <sys/time.h>

#define TIME_S(tv_s) {\
                gettimeofday(&tv_s,NULL);\
            }
    
#define TIME_NOW(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Speedstatus: %.6f; %lu\n", elapsed);\
                }


int main(int argc, char *argv[]){
    struct timeval t1,t2;
	char buf[20];
    int c = 100;
    int i = 0;
    FILE *fd = fopen("/dev/zero", "r"); 

    TIME_S(t1);
    while(1){
        for(int j=0; j < 1000; j++){
            c = fgetc(fd);
            buf[i%5] = c;
            i++;
        }
        TIME_NOW(t1,t2);
    }
  
    return 0;
}
