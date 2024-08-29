#include<stdio.h>
#include<unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#define AM2315_I2CADDR       0x5C
#define AM2315_READREG       0x03

// I2C i2c(I2C1_SDA, I2C1_SCL);
 
// DigitalOut myled(LED1);
// Serial pc(STDIO_UART_TX, STDIO_UART_RX);

float target_temp = 22.0;
int myled = 0;

void die() {
	while (1) {
    	myled = !myled;
        sleep(1);
    }
} 

int pc_read_buf_vuln(char* buf) {
    int i = 0;
    char c;
    while (1) {
        c = fgetc(stdin);
        buf[i] = c;
        i++;
        if (c == 0) {
            return i;
        }        
    }
}

void read_data(uint8_t* data_read) {
    data_read[0] = 0;
    data_read[1] = 0;
    data_read[2] = 2;
    data_read[3] = 38;
    data_read[4] = 2;
    data_read[5] = 38;
}

float get_humidity() {
	float humidity;
	uint8_t reply[10];
	read_data(reply);
	humidity = reply[2];
	humidity *= 256;
	humidity += reply[3];
	humidity /= 10;
	return humidity;
}

float get_temp() {
	uint8_t reply[10];
	float temp;
	read_data(reply);
	temp = reply[4] & 0x7F;
	temp *= 256;
	temp += reply[5];
	temp /= 10;
	return temp;
}

float get_new_temp() {
	float new_temp = 0.0;

	char buf[16];
	int i = 0;
	char c;
	while (1) {	
        c = fgetc(stdin);
        if (c == 0)
            continue;
        else if (c == 0xd) {
            buf[i] = 0;
            new_temp = atof(buf);
            printf("Temp set to %2f\r\n", new_temp);
            break;
        }
        else {
            buf[i] = c;
            i++;
        }		
	}
	return new_temp;
}

void check_temp(float temp) {
	if (temp > target_temp + 0.5) {
		if (!myled) {
			printf("AC ON!\r\n");
			myled = 1;
		}
	}
	else if (temp < target_temp - 0.5) {
		if (!myled) {
			printf("HEATER ON!\r\n");
			myled = 1;
		}
	}
	else {
		if (myled) {
			printf("HEAT/AC OFF\r\n");
			myled = 0;
		}
	}
}

int main()
{
    char cmd;
    printf("Booting firmware...\r\n");
	sleep(5);
    printf("Booted!\r\n");
    while (1) {
        //pc.printf("Reading sensor...\r\n");
		// Calculate temperature value in Celcius
		float temp;
		float humidity;
        cmd = fgetc(stdin);
		
		switch (cmd) {
            case 't':
                temp = get_temp();
                check_temp(temp);
                printf("%2f\r\n", temp);
                break;
            case 'h':
                humidity = get_humidity();
                printf("%2f\r\n", humidity);
                break;
			case 's':
				target_temp = get_new_temp();
				break;
        }
    }
}

