/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h> 
#include <unistd.h>

//#include "hardware/uart.h"

#define UART_ID "uart0"
#define BAUD_RATE 115200

#define UART_TX_PIN 0
#define UART_RX_PIN 1

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

typedef struct uart{
    char name[5];
    int pin_tx;
    int pin_rx;
    int boud_rate;
}uart_t;

void uart_init(uart_t *uart, char *name, int rate){
    strcpy(uart->name, name);
    uart->boud_rate = rate;
    printf("Init %s\n", name);
}

void gpio_set_tx(uart_t *uart, int tx){
    uart->pin_tx = tx;
}

void gpio_set_rx(uart_t *uart, int rx){
    uart->pin_rx = rx;
}

void uart_putc_raw(uart_t *uart, char c){
    printf("%c", c);
}

void uart_putc(uart_t *uart, char c){
    printf("%c\n", c);
}

void uart_puts(uart_t *uart, char *str){
    puts(str);
    usleep(10000);
}

void uart_input(uart_t *uart, char *buf){
    scanf("%s",buf);
    usleep(1000000);
}

int main(int argc, char **argv) {

    uart_t uart;
    char buf[40];

    struct  timeval t1,t2;
    TIME_S(t1)

    // Set up our UART with the required speed.
    uart_init(&uart, UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_tx(&uart, UART_TX_PIN);
    gpio_set_rx(&uart, UART_RX_PIN);

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART

    // Send out a character without any conversions
    uart_putc_raw(&uart, 'A');

    // Send out a character but do CR/LF conversions
    uart_putc(&uart, 'B');

    // Send out a string, with CR/LF conversions
    uart_puts(&uart, " Hello, UART!\n");

    uart_input(&uart, buf);

    uart_puts(&uart, buf);

    gettimeofday(&t2, NULL);

    TIME_F(t1,t2)

    return 0;
}
