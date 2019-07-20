#include <Arduino.h>
#include <PinChangeInt.h>

#define PPM_PIN 12
#define MAX_CHANNELS 10

//////////////////////CONFIGURATION///////////////////////////////
#define CHANNEL_NUMBER 10  //set the number of chanels
#define CHANNEL_DEFAULT_VALUE 1491  //set the default servo value
#define FRAME_LENGTH 20000  //set the PPM frame length in microseconds (1ms = 1000µs)
#define PULSE_LENGTH 300  //set the pulse length
#define onState 1  //set polarity of the pulses: 1 is positive, 0 is negative
#define sigPin 11  //set PPM signal output pin on the arduino
//////////////////////////////////////////////////////////////////
#define SWITCH_PIN 16
#define CHANNEL_TO_MODIFY 11
#define SWITCH_STEP 100

byte previousSwitchValue;

/*this array holds the servo values for the ppm signal
  change theese values in your code (usually servo values move between 1000 and 2000)*/
int ppm[CHANNEL_NUMBER];
/*
   |ppm[i] | CHANNEL   |
   |-------|-----------|
   | 0     | Roll      |
   | 1     | Pitch     |
   | 2     | Throttle  |
   | 3     | Yaw       |
   | 4     | AUX1      |
   | 5     | AUX2      |
   | 6     | AUX3      |
   | 7     | AUX4      |
   | 8     | AUX5      |
   | 9     | AUX6      |
   |-------|-----------|
*/

int currentChannelStep;

volatile int pwm_value[MAX_CHANNELS] = { 0 };
volatile int prev_time = 0;
volatile int curr_channel = 0;

volatile bool overflow = false;

void rising()
{
  int tstamp = micros();

  /* overflow should never acutally happen, but who knows... */
  if (curr_channel < MAX_CHANNELS) {
    pwm_value[curr_channel] = tstamp - prev_time;
    if (pwm_value[curr_channel] > 2100) { /* it's actually a sync */
      pwm_value[curr_channel] = 0;
      curr_channel = 0;
    } else
      curr_channel++;
  } else
    overflow = true;

  prev_time = tstamp;
}

void setup() {
  pinMode(PPM_PIN, INPUT_PULLUP);
  PCintPort::attachInterrupt(PPM_PIN, &rising, RISING);
  Serial.begin(9600);

  previousSwitchValue = HIGH;

  //initiallize default ppm values
  for (int i = 0; i < CHANNEL_NUMBER; i++) {
    if (i > 3 || i == 2 || i == CHANNEL_TO_MODIFY) {
      ppm[i] = 992;
    } else {
      ppm[i] = CHANNEL_DEFAULT_VALUE;
    }
  }

  /*
    Roll, Pitch, Yaw = 1500
    Trottle, AUX1-AUX6 = 1000
  */

  pinMode(sigPin, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  digitalWrite(sigPin, !onState);  //set the PPM signal pin to the default state (off)

  cli();
  TCCR1A = 0; // set entire TCCR1 register to 0
  TCCR1B = 0;

  OCR1A = 100;  // compare match register, change this
  TCCR1B |= (1 << WGM12);  // turn on CTC mode
  TCCR1B |= (1 << CS11);  // 8 prescaler: 0,5 microseconds at 16mhz
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt
  sei();

  currentChannelStep = SWITCH_STEP;
}

ISR(TIMER1_COMPA_vect) { //leave this alone
  static boolean state = true;

  TCNT1 = 0;

  if (state) {  //start pulse
    digitalWrite(sigPin, onState);
    OCR1A = PULSE_LENGTH * 2;
    state = false;
  } else { //end pulse and calculate when to start the next pulse
    static byte cur_chan_numb;
    static unsigned int calc_rest;

    digitalWrite(sigPin, !onState);
    state = true;

    if (cur_chan_numb >= CHANNEL_NUMBER) {
      cur_chan_numb = 0;
      calc_rest = calc_rest + PULSE_LENGTH;//
      OCR1A = (FRAME_LENGTH - calc_rest) * 2;
      calc_rest = 0;
    }
    else {
      OCR1A = (ppm[cur_chan_numb] - PULSE_LENGTH) * 2;
      calc_rest = calc_rest + ppm[cur_chan_numb];
      cur_chan_numb++;
    }
  }
}

void loop() {

  for (int i = 0; i < MAX_CHANNELS; i++) {
    ppm[i] = pwm_value[i];
  }

  int switchState;

  switchState = digitalRead(SWITCH_PIN);

  if (switchState == LOW && previousSwitchValue == HIGH) {

    static int val = SWITCH_STEP;

    ppm[CHANNEL_TO_MODIFY] = ppm[CHANNEL_TO_MODIFY] + currentChannelStep;

    if (ppm[CHANNEL_TO_MODIFY] > 2000 || ppm[CHANNEL_TO_MODIFY] < 1000) {
      currentChannelStep = currentChannelStep * -1;
      ppm[CHANNEL_TO_MODIFY] = ppm[CHANNEL_TO_MODIFY] + currentChannelStep;
    }
  }
  previousSwitchValue = switchState;
}

//void loop() {
//  for (uint8_t i = 0; i < MAX_CHANNELS; i++)
//    Serial.println("ch" + String(i + 1) + " = " + String(pwm_value[i]));
//  if (overflow)
//    Serial.println("OVERFLOW!");
//  Serial.println("----------------");
//  delay(1000);
//}
