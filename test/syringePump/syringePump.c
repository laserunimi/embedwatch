//https://github.com/naroom/OpenSyringePump/blob/master/syringePump/syringePump.ino
//https://hackaday.io/project/1838-open-syringe-pump

// Controls a stepper motor via an LCD keypad shield.
// Accepts triggers and serial commands.
// To run, you will need the LCDKeypad library installed - see libraries dir.

// Serial commands:
// Set serial baud rate to 57600 and terminate commands with newlines.
// Send a number, e.g. "100", to set bolus size.
// Send a "+" to push that size bolus.
// Send a "-" to pull that size bolus.

#include <stdio.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>


/* -- Constants -- */
#define SYRINGE_VOLUME_ML 30.0
#define SYRINGE_BARREL_LENGTH_MM 80.0

#define THREADED_ROD_PITCH 1.25
#define STEPS_PER_REVOLUTION 200.0
#define MICROSTEPS_PER_STEP 16.0

#define SPEED_MICROSECONDS_DELAY 2 //longer delay = lower speed

#define	false	0
#define	true	1

#define	boolean	_Bool
#define three_dec_places( x ) ( (int)( (x*1e3)+0.5 - (((int)x)*1e3) ) )

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

long ustepsPerMM = MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION / THREADED_ROD_PITCH;
long ustepsPerML = (MICROSTEPS_PER_STEP * STEPS_PER_REVOLUTION * SYRINGE_BARREL_LENGTH_MM) / (SYRINGE_VOLUME_ML * THREADED_ROD_PITCH );

/* -- Pin definitions -- */
int motorDirPin = 2;
int motorStepPin = 3;

int triggerPin = 0;    //TODO check RPi pin-out before implementing
int bigTriggerPin = 0; //TODO check RPi pin-out before implementing

/* -- Keypad states -- */
int  adc_key_val[5] ={30, 150, 360, 535, 760 };

enum{ KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_SELECT, KEY_NONE};
int NUM_KEYS = 5;
int adc_key_in;
int key = KEY_NONE;

/* -- Enums and constants -- */
enum{PUSH,PULL}; //syringe movement direction
enum{MAIN, BOLUS_MENU}; //UI states

enum{INPUT, OUTPUT}; //GPIO directions
enum{HIGH, LOW}; //GPIO states

const int mLBolusStepsLength = 9;
float mLBolusSteps[9] = {0.001, 0.005, 0.010, 0.050, 0.100, 0.500, 1.000, 5.000, 10.000};

/* -- Default Parameters -- */
float mLBolus = 0.500; //default bolus size
float mLBigBolus = 1.000; //default large bolus size
float mLUsed = 0.0;
int mLBolusStepIdx = 3; //0.05 mL increments at first
//float mLBolusStep = mLBolusSteps[mLBolusStepIdx];
float mLBolusStep = 0.050;

long stepperPos = 0; //in microsteps
char charBuf[16];

//debounce params
long lastKeyRepeatAt = 0;
long keyRepeatDelay = 400;
long keyDebounce = 125;
int prevKey = KEY_NONE;
        
//menu stuff
int uiState = MAIN;

//triggering
int prevBigTrigger = HIGH;
int prevTrigger = HIGH;

//serial
char serialStr[8000] = "";
boolean serialStrReady = false;
int serialStrLen = 0;

// ----------- led.c -----------------
void led_on() {
	printf("%s\n", __func__);
	return;
}

void led_off() {
	printf("%s\n", __func__);
	return;
}
// -----------------------------------

// ----------------- liquidCrystal.c -------------------
typedef struct LiquidCrystalStruct{
	unsigned int id;
} LiquidCrystal;

void lcd_begin(LiquidCrystal* lcd, unsigned int cols, unsigned int rows) {
	printf("%s (LiquidCrystal * lcd = 0x%x, unsigned int cols = %u, unsigned int rows = %u)\n",__func__,lcd, cols, rows);
	return;
}

void lcd_clear(LiquidCrystal* lcd) {
	printf("%s (LiquidCrystal * lcd = 0x%x\n", __func__);
	return; //http://stackoverflow.com/questions/10105666/clearing-the-terminal-screen
}

void lcd_print(LiquidCrystal* lcd, char* output, int len) {
	printf("%s (LiquidCrystal * lcd = 0x%x, char* output = %s, int len = %d)\n", __func__, lcd, output, len);
	return;
}

void lcd_setCursor(LiquidCrystal* lcd, int x, int y) { //http://stackoverflow.com/questions/10105666/clearing-the-terminal-screen
	printf("%s (LiquidCrystal * lcd = 0x%x, int x = %d, int y = %d\n", __func__, x, y);
	return;
}
// -------------------------------------------------------


// ---------------------- util.c -------------------------
void pinMode(int pin, int mode) {
	printf("%s (int pin = %d, int mode = %d)\n", __func__, pin, mode);
	return;
}

int digitalRead(int pin) {
	int val;
	printf("%s (int pin = %d)\n", __func__, pin);
	scanf("%d", (char *)&val);
	return val;
}

void digitalWrite(int pin, int value) {
//	printf("%s (int pin = %d, int value = %d)\n", __func__, pin, value);
	return;
}

void Serial_begin(int baud) {
	printf("%s (int baud = %d)\n", __func__, baud);
	return;
}

int Serial_available() {
	// char c;

	// c = getchar();

	// printf("%s() c:%c\n", __func__, c);

	// if (c == 'y')
	// 	return 1;
	// else
	// 	return 0;
	return 1;
}

int Serial_read() {
	char c;

	c = fgetc(stdin);

	return (int)c;
}

int Serial_write(char* output, int len) {
	printf("%s (char *output = %s, int len = %d)\n", __func__, output, len);
	return 0;
}

int analogRead(int pin) {
	int val;
	printf("read from pin %d\n", pin);
	//scanf("%d", &val);
	val = 5;
	return val;
}

unsigned long millis() {
	struct timeval start;

	gettimeofday(&start, NULL);

	return start.tv_sec * 1000 + start.tv_usec/1000;

}

unsigned long usecs() {
	struct timeval start;

	gettimeofday(&start, NULL);

	return start.tv_sec * 1000 * 1000 + start.tv_usec;

}

void delayMicroseconds(float usecs) {
	usleep((long)usecs);
	//printf("fake sleep of %f\n", usecs);
}

int toUInt(char* input, int len) {
	int val;
	val = atoi(input);
	return val;
}

/* -- Initialize libraries -- */
LiquidCrystal lcd;

void setup(){
	/* LCD setup */
	lcd_begin(&lcd, 16, 2);
	lcd_clear(&lcd);

	lcd_print(&lcd, "SyringePump v2.0", 16);

	/* Triggering setup */
	pinMode(triggerPin, INPUT);
	pinMode(bigTriggerPin, INPUT);
	digitalWrite(triggerPin, HIGH); //enable pullup resistor
	digitalWrite(bigTriggerPin, HIGH); //enable pullup resistor
  
	/* Motor Setup */
	pinMode(motorDirPin, OUTPUT);
	pinMode(motorStepPin, OUTPUT);
  
	/* Serial setup */
	//Note that serial commands must be terminated with a newline
	//to be processed. Check this setting in your serial monitor if
	//serial commands aren't doing anything.
	Serial_begin(57600); //Note that your serial connection must be set to 57600 to work!
}
// ------------------------------------------

void checkTriggers();
void readSerial();
void processSerial();
void bolus(int direction);
void readKey();
void doKeyAction(unsigned int key);
void updateScreen();
int get_key(unsigned int input);
unsigned int tim;

void loop(int count){
	//check for LCD updates
	//readKey();

	//look for triggers on trigger lines
	//checkTriggers();

	//check serial port for new commands
	readSerial();

	// EVAL1: get the time spent on processSerial() for 1 uL, with/without rewritting.
	// 	here we hard code mLBolus
	mLBolus = 0.001 * count;
	serialStr[0] = '+';
	serialStrReady = true;
	unsigned long start, end;

	start = usecs();

	if(serialStrReady){
		processSerial();
	}

	end = usecs();

	printf("mLBolus = %f, time usecs: %lu\n", mLBolus, end - start);
}

void checkTriggers(){
	//check low-reward trigger line
	int pushTriggerValue = digitalRead(triggerPin);
	if(pushTriggerValue == HIGH && prevTrigger == LOW){
		bolus(PUSH);
		updateScreen();
	}
	prevTrigger = pushTriggerValue;
    
	//check high-reward trigger line
	int bigTriggerValue = digitalRead(bigTriggerPin);
	if(bigTriggerValue == HIGH && prevBigTrigger == LOW){
		//push big reward amount
		float mLBolusTemp = mLBolus;
		mLBolus = mLBigBolus;
		bolus(PUSH);
		mLBolus = mLBolusTemp;

		updateScreen();
	}
	prevBigTrigger = bigTriggerValue;
}

void readSerial(){
	char buffer[8000] = {0};
	//pulls in characters from serial port as they arrive
	//builds serialStr and sets ready flag when newline is found
	while (Serial_available()) {
		char inChar = (char)Serial_read();
		if (inChar < 0x20) {
			serialStrReady = true;
			break;
		}
		else{
			buffer[serialStrLen] = inChar;
			serialStrLen++; 
		}
	}
	strcpy(serialStr, buffer); 
}

void processSerial(){
	//process serial commands as they are read in
	if(serialStr[0] == '+'){
		bolus(PUSH);
		updateScreen();
	}
	else if(serialStr[0] == '-'){
		bolus(PULL);
		updateScreen();
	}
	else if(toUInt(serialStr, serialStrLen) != 0){
		int uLbolus = toUInt(serialStr, serialStrLen);
		mLBolus = (float)uLbolus / 1000.0;
		updateScreen();
	}
	else{
		Serial_write("Invalid command: [", 18);
		Serial_write(serialStr, serialStrLen);
		Serial_write("]\n", 2);
	}
	serialStrReady = false;
	serialStrLen = 0;
}

void bolus(int direction){
	//Move stepper. Will not return until stepper is done moving.
  
	//change units to steps
	long steps = (mLBolus * ustepsPerML);
	if(direction == PUSH){
		led_on();
		digitalWrite(motorDirPin, HIGH);
		steps = mLBolus * ustepsPerML;
		mLUsed += mLBolus;
	}
	else if(direction == PULL){
		led_off();
		digitalWrite(motorDirPin, LOW);
		if((mLUsed-mLBolus) > 0){
			mLUsed -= mLBolus;
		}
		else{
			mLUsed = 0;
		}
	}	

	float usDelay = SPEED_MICROSECONDS_DELAY; //can go down to 20 or 30
    
	for(long i=0; i < steps; i++){
		digitalWrite(motorStepPin, HIGH);
		delayMicroseconds(usDelay);
    
		digitalWrite(motorStepPin, LOW);
		delayMicroseconds(usDelay);
	}

}

void readKey(){
	//Some UI niceness here. 
	//When user holds down a key, it will repeat every so often (keyRepeatDelay).
	//But when user presses and releases a key,
	//the key becomes responsive again after the shorter debounce period (keyDebounce).

	adc_key_in = analogRead(0);
	key = get_key(adc_key_in); // convert into key press

	long currentTime = millis();
	long timeSinceLastPress = (currentTime-lastKeyRepeatAt);
        
	boolean processThisKey = false;
	if (prevKey == key && timeSinceLastPress > keyRepeatDelay){
		processThisKey = true;
	}
	if(prevKey == KEY_NONE && timeSinceLastPress > keyDebounce){
		processThisKey = true;
	}
	if(key == KEY_NONE){
		processThisKey = false;
	}
        
	prevKey = key;
        
	if(processThisKey){
		doKeyAction(key);
		lastKeyRepeatAt = currentTime;
	}
}

void doKeyAction(unsigned int key){
	if(key == KEY_NONE){
		return;
	}

	if(key == KEY_SELECT){
		if(uiState == MAIN){
			uiState = BOLUS_MENU;
		}
		else if(BOLUS_MENU){
			uiState = MAIN;
		}
	}

	if(uiState == MAIN){
		if(key == KEY_LEFT){
			bolus(PULL);
		}
		if(key == KEY_RIGHT){
			bolus(PUSH);
		}
		if(key == KEY_UP){
			mLBolus += mLBolusStep;
		}
		if(key == KEY_DOWN){
			if((mLBolus - mLBolusStep) > 0){
				mLBolus -= mLBolusStep;
			}
			else{
				mLBolus = 0;
			}
		}
	}
	else if(uiState == BOLUS_MENU){
		if(key == KEY_LEFT){
			//nothin'
		}
		if(key == KEY_RIGHT){
			//nothin'
		}
		if(key == KEY_UP){
			if(mLBolusStepIdx < mLBolusStepsLength-1){
				mLBolusStepIdx++;
				mLBolusStep = mLBolusSteps[mLBolusStepIdx];
			}
		}
		if(key == KEY_DOWN){
			if(mLBolusStepIdx > 0){
				mLBolusStepIdx -= 1;
				mLBolusStep = mLBolusSteps[mLBolusStepIdx];
			}
		}
	}

	updateScreen();
}

void updateScreen(){
	//build strings for upper and lower lines of screen
	char s1[80]; //upper line
	char s2[80]; //lower line
	int s1Len = 0;
	int s2Len = 0;
	
	if(uiState == MAIN){
		s1Len = sprintf(s1, "Used %d.%d mL", (int)mLUsed, three_dec_places(mLUsed));
		s2Len = sprintf(s2, "Bolus %d.%d mL", (int)mLBolus, three_dec_places(mLBolus));
	}
	else if(uiState == BOLUS_MENU){
		s1Len = sprintf(s1, "Menu> BolusStep");
		s2Len = sprintf(s2, "%d.%d", (int)mLBolusStep, three_dec_places(mLBolusStep));
	}

	//do actual screen update
	lcd_clear(&lcd);

	lcd_setCursor(&lcd, 0, 0);  //line=1, x=0
	lcd_print(&lcd, s1, s1Len);

	lcd_setCursor(&lcd, 0, 1);  //line=2, x=0
	lcd_print(&lcd, s2, s2Len);
}


// Convert ADC value to key number
int get_key(unsigned int input){
	int k;
	for (k = 0; k < NUM_KEYS; k++){
		if (input < (unsigned int)adc_key_val[k]){
    		return k;
		}
	}
	if (k >= NUM_KEYS){
	  k = KEY_NONE;     // No valid key pressed
	}
	return k;
}

void main(int argc, char **args, char **atags) {
	(void)argc;
	(void)args;
	(void)atags;

    struct  timeval t1,t2;
    TIME_S(t1)

	printf("Starting syringe pump\n");
	setup();
	int count = 1;
	while(count < 1000) {
		count += 10;
		loop(count);
	}

	TIME_F(t1,t2);
	hook_exit();

}
