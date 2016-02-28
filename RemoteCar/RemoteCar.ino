#include <nRF24L01.h>
#include <RF24.h>
#include <RF24_config.h>

#include <SPI.h>

#include <SoftPWM.h>
#include <SoftPWM_timer.h>

// Arduino pins for the shift register
#define MOTORLATCH 12
#define MOTORCLK 4
#define MOTORENABLE 7
#define MOTORDATA 8

// 8-bit bus after the 74HC595 shift register 
// (not Arduino pins)
// These are used to set the direction of the bridge driver.
#define MOTOR1_A 2
#define MOTOR1_B 3
#define MOTOR2_A 1
#define MOTOR2_B 4
#define MOTOR3_A 5
#define MOTOR3_B 7
#define MOTOR4_A 0
#define MOTOR4_B 6

// Arduino pins for the PWM signals.
#define MOTOR1_PWM 11
#define MOTOR2_PWM 3
#define MOTOR3_PWM 6
#define MOTOR4_PWM 5
#define SERVO1_PWM 10
#define SERVO2_PWM 9

// Codes for the motor function.
#define FORWARD 1
#define BACKWARD 2
#define BRAKE 3
#define RELEASE 4
#define LEFT 5
#define RIGHT 6

// NRF Transciever Pins
#define nrf_GND 53
#define nrf_VCC 52
#define nrf_CE 51
#define nrf_CSN 50
#define nrf_SCK 49
#define nrf_MOSI 48
#define nrf_MISO 47
#define nrf_IRQ 48

int msg[2];
RF24 radio(9,10);
const uint64_t pipe = 0xE8E8F0F0E1LL;

void setup() {
  // put your setup code here, to run once:

  SoftPWMBegin(SOFTPWM_NORMAL);

  pinMode(nrf_GND, OUTPUT);
  pinMode(nrf_VCC, OUTPUT);
  pinMode(nrf_CE, OUTPUT);
  pinMode(nrf_CSN, OUTPUT);
  pinMode(nrf_SCK, OUTPUT);
  pinMode(nrf_MOSI, OUTPUT);
  pinMode(nrf_MISO, OUTPUT);
  pinMode(nrf_IRQ, OUTPUT);

  digitalWrite(nrf_GND, LOW); // Set to GND
  SoftPWMSet(nrf_VCC, 168); //Approx 3.3V

  radio.begin();
  radio.openReadingPipe(1,pipe);
  radio.startListening();

}

void loop() {
  
  if (radio.available()){
   bool done = false;    
   while (!done){
     done = radio.read(msg, 1);      
     if (msg[0] == 101){
        motor(FORWARD,msg[1]);
      }
     if (msg[0] == 202){
        motor(BACKWARD,msg[1]);
      }
     if (msg[0] == 201){
        motor(LEFT,msg[1]);
      }
     if (msg[0] == 102){
        motor(RIGHT,msg[1]);
      }
     else {
      motor(RELEASE,0);
     }
     delay(10);
     }
  }
  
}

void motor(int command, int speed)
{

    switch (command)
    {
    case FORWARD:
      motor_output (MOTOR1_A, HIGH, speed);
      motor_output (MOTOR1_B, LOW, -1);     // -1: no PWM set
      motor_output (MOTOR4_A, HIGH, speed);
      motor_output (MOTOR4_B, LOW, -1);     // -1: no PWM set
      delay(100);
      break;
    case BACKWARD:
      motor_output (MOTOR1_A, LOW, speed);
      motor_output (MOTOR1_B, HIGH, -1);    // -1: no PWM set
      motor_output (MOTOR4_A, LOW, speed);
      motor_output (MOTOR4_B, HIGH, -1);    // -1: no PWM set
      delay(100);
      break;
    case BRAKE:
      // The brake will probably be unused becasue its not that good
      motor_output (MOTOR1_A, LOW, 255); // 255: fully on.
      motor_output (MOTOR1_B, LOW, -1);  // -1: no PWM set
      motor_output (MOTOR4_A, LOW, 255); // 255: fully on.
      motor_output (MOTOR4_B, LOW, -1);  // -1: no PWM set
      break;
    case RELEASE:
      motor_output (MOTOR1_A, LOW, 0);
      motor_output (MOTOR1_B, LOW, -1);  // -1: no PWM set
      motor_output (MOTOR4_A, LOW, 0);
      motor_output (MOTOR4_B, LOW, -1);  // -1: no PWM set
      break;
    case LEFT:
      motor_output (MOTOR1_A, HIGH, speed);
      motor_output (MOTOR1_B, LOW, -1);     // -1: no PWM set
      motor_output (MOTOR4_A, LOW, 0);
      motor_output (MOTOR4_B, LOW, -1);     // -1: no PWM set
      delay(100);
      break;
    case RIGHT:
      motor_output (MOTOR1_A, LOW, 0);
      motor_output (MOTOR1_B, LOW, -1);     // -1: no PWM set
      motor_output (MOTOR4_A, HIGH, speed);
      motor_output (MOTOR4_B, LOW, -1);     // -1: no PWM set
      delay(100);
      break;
    default:
      break;
    }
}

void motor_output (int output, int high_low, int speed)
{
  int motorPWM;

  switch (output)
  {
  case MOTOR1_A:
  case MOTOR1_B:
    motorPWM = MOTOR1_PWM;
    break;
  case MOTOR2_A:
  case MOTOR2_B:
    motorPWM = MOTOR2_PWM;
    break;
  case MOTOR3_A:
  case MOTOR3_B:
    motorPWM = MOTOR3_PWM;
    break;
  case MOTOR4_A:
  case MOTOR4_B:
    motorPWM = MOTOR4_PWM;
    break;
  default:
    // Use speed as error flag, -3333 = invalid output.
    speed = -3333;
    break;
  }

  if (speed != -3333)
  {
    // Set the direction with the shift register 
    // on the MotorShield, even if the speed = -1.
    // In that case the direction will be set, but
    // not the PWM.
    shiftWrite(output, high_low);

    // set PWM only if it is valid
    if (speed >= 0 && speed <= 255)    
    {
      analogWrite(motorPWM, speed);
    }
  }
}

void shiftWrite(int output, int high_low)
{
  static int latch_copy;
  static int shift_register_initialized = false;

  // Do the initialization on the fly, 
  // at the first time it is used.
  if (!shift_register_initialized)
  {
    // Set pins for shift register to output
    pinMode(MOTORLATCH, OUTPUT);
    pinMode(MOTORENABLE, OUTPUT);
    pinMode(MOTORDATA, OUTPUT);
    pinMode(MOTORCLK, OUTPUT);

    // Set pins for shift register to default value (low);
    digitalWrite(MOTORDATA, LOW);
    digitalWrite(MOTORLATCH, LOW);
    digitalWrite(MOTORCLK, LOW);
    // Enable the shift register, set Enable pin Low.
    digitalWrite(MOTORENABLE, LOW);

    // start with all outputs (of the shift register) low
    latch_copy = 0;

    shift_register_initialized = true;
  }

  // The defines HIGH and LOW are 1 and 0.
  // So this is valid.
  bitWrite(latch_copy, output, high_low);

  // Use the default Arduino 'shiftOut()' function to
  // shift the bits with the MOTORCLK as clock pulse.
  // The 74HC595 shiftregister wants the MSB first.
  // After that, generate a latch pulse with MOTORLATCH.
  shiftOut(MOTORDATA, MOTORCLK, MSBFIRST, latch_copy);
  delayMicroseconds(5);    // For safety, not really needed.
  digitalWrite(MOTORLATCH, HIGH);
  delayMicroseconds(5);    // For safety, not really needed.
  digitalWrite(MOTORLATCH, LOW);
}
