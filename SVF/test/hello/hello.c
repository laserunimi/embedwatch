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
    
#define TIME_F(tv_s,tv_f) {\
                gettimeofday(&tv_f,NULL);\
                long seconds = tv_f.tv_sec - tv_s.tv_sec;\
                long microseconds = tv_f.tv_usec - tv_s.tv_usec;\
                double elapsed = seconds + microseconds*1e-6;\
                printf("Elapsed: %.6f\n", elapsed);\
                }

void func1(int num){  //num=1000 
    for(int i = 0; i < num; i++){
        printf("%d\n", i);
    }
}

void myfunc(){
    char buf[20];

    int c = 100;
    int i = 0;
    while(c > 0x20){
        c = fgetc(stdin);
        buf[i] = c;
        i++;
    }
}

int main(int argc, char *argv[]){
    struct  timeval t1,t2;
    TIME_S(t1)

	func1(1000);
    myfunc();

    TIME_F(t1,t2)
  
    return 0;
}
