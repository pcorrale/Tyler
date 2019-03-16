/*

  MSE 2202 MSEBot base code for Labs 3 and 4
  Language: Arduino
  Authors: Michael Naish and Eugen Porter
  Date: 16/01/17

  Rev 1 - Initial version
  Rev 2 - Update for MSEduino v. 2

*/

//dynamic variables
unsigned long tempTimer = 0;
unsigned long chargeTimer = 0;
unsigned long angleKeeper = 0;
unsigned long distanceKeeper = 0;
bool turnFlag = false; //assuming the first u-turn is left, false will be a left turn and vice versa
bool chargeFlag = false; //to reverse when dropping off parts
bool foundBeacon = false;
bool secondRound = false;
unsigned int phaseA = 1; //sweeping
unsigned int phaseB = 1; //dumping
unsigned int phaseC = 1; //resuming
unsigned int phaseD = 1; //avoiding
unsigned int dumbCount = 0;
unsigned echo; //ultrasonic sensor
const int forwardSpeed = 1800;
const int reverseSpeed = 1300;
const int forwardSlow = 1600;
const int reverseSlow = 1400;
const int brake = 1500;
const int uTurnTime = 1900;
const int turnStart = 1000;
int infra;

#include <Servo.h>
#include <EEPROM.h>
#include <uSTimer2.h>
#include <CharliePlexM.h>
#include <Wire.h>
#include <I2CEncoder.h>

Servo servo_RightMotor;
Servo servo_LeftMotor;
Servo servo_ArmMotor;
Servo servo_GripMotor;

I2CEncoder encoder_RightMotor;
I2CEncoder encoder_LeftMotor;

// Uncomment keywords to enable debugging output

//#define DEBUG_MODE_DISPLAY
//#define DEBUG_MOTORS
//#define DEBUG_LINE_TRACKERS
//#define DEBUG_ENCODERS
//#define DEBUG_ULTRASONIC
//#define DEBUG_LINE_TRACKER_CALIBRATION
//#define DEBUG_MOTOR_CALIBRATION

boolean bt_Motors_Enabled = true;

//port pin constants
const int frontLPing = 2;   //input plug
const int frontLData = 3;   //output plug
const int frontRPing = 4;
const int frontRData = 5;
const int topPing = 6;
const int topData = 7;
const int leftPing = 10;
const int leftData = 11;
const int rightPing = 12;
const int rightData = 13;

const int ci_Charlieplex_LED1 = 4;
const int ci_Charlieplex_LED2 = 5;
const int ci_Charlieplex_LED3 = 6;
const int ci_Charlieplex_LED4 = 7;
const int ci_Mode_Button = 7;
const int ci_Right_Motor = 8;
const int ci_Left_Motor = 9;
const int ci_Motor_Enable_Switch = 12;
const int ci_Light_Sensor = A3;
const int ci_I2C_SDA = A4;         // I2C data = white
const int ci_I2C_SCL = A5;         // I2C clock = yellow

// Charlieplexing LED assignments
const int ci_Heartbeat_LED = 1;
const int ci_Indicator_LED = 4;

//constants
const int ci_Display_Time = 500;
const int ci_Motor_Calibration_Cycles = 3;
const int ci_Motor_Calibration_Time = 5000;
const int ci_Left_Motor_Offset_Address_L = 12;
const int ci_Left_Motor_Offset_Address_H = 13;
const int ci_Right_Motor_Offset_Address_L = 14;
const int ci_Right_Motor_Offset_Address_H = 15;

//variables
byte b_LowByte;
byte b_HighByte;
unsigned ui_Left_Motor_Speed;
unsigned ui_Right_Motor_Speed;
unsigned long frontLEcho;
unsigned long frontREcho;
unsigned long leftEcho;
unsigned long rightLEcho;
unsigned long topEcho;
long l_Left_Motor_Position;
long l_Right_Motor_Position;

unsigned long ul_3_Second_timer = 0;
unsigned long ul_Display_Time;
unsigned long ul_Calibration_Time;
unsigned long ui_Left_Motor_Offset;
unsigned long ui_Right_Motor_Offset;

unsigned int ui_Cal_Count;
unsigned int ui_Cal_Cycle;

unsigned int  ui_Robot_State_Index = 0;
//0123456789ABCDEF
unsigned int  ui_Mode_Indicator[6] = {
  0x00,    //B0000000000000000,  //Stop
  0x00FF,  //B0000000011111111,  //Run
  0x0F0F,  //B0000111100001111,  //Calibrate line tracker light level
  0x3333,  //B0011001100110011,  //Calibrate line tracker dark level
  0xAAAA,  //B1010101010101010,  //Calibrate motors
  0xFFFF   //B1111111111111111   //Unused
};

unsigned int  ui_Mode_Indicator_Index = 0;

//display Bits 0,1,2,3, 4, 5, 6,  7,  8,  9,  10,  11,  12,  13,   14,   15
int  iArray[16] = {
  1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384, 65536
};
int  iArrayIndex = 0;

boolean bt_Heartbeat = true;
boolean bt_3_S_Time_Up = false;
boolean bt_Do_Once = false;
boolean bt_Cal_Initialized = false;

void setup() {
  Wire.begin();        // Wire library required for I2CEncoder library
  Serial.begin(9600);

  CharliePlexM::setBtn(ci_Charlieplex_LED1, ci_Charlieplex_LED2,
                       ci_Charlieplex_LED3, ci_Charlieplex_LED4, ci_Mode_Button);

  // set up ultrasonic
  pinMode(frontLPing, OUTPUT);
  pinMode(frontLData, INPUT);
  pinMode(frontRPing, OUTPUT);
  pinMode(frontRData, INPUT);
  pinMode(leftPing, OUTPUT);
  pinMode(leftData, INPUT);
  pinMode(rightPing, OUTPUT);
  pinMode(rightData, INPUT);
  pinMode(topPing, OUTPUT);
  pinMode(topData, INPUT);

  // set up drive motors
  pinMode(ci_Right_Motor, OUTPUT);
  servo_RightMotor.attach(ci_Right_Motor);
  pinMode(ci_Left_Motor, OUTPUT);
  servo_LeftMotor.attach(ci_Left_Motor);

  // set up motor enable switch
  pinMode(ci_Motor_Enable_Switch, INPUT);

  // set up encoders. Must be initialized in order that they are chained together,
  // starting with the encoder directly connected to the Arduino. See I2CEncoder docs
  // for more information
  encoder_LeftMotor.init(1.0 / 3.0 * MOTOR_393_SPEED_ROTATIONS, MOTOR_393_TIME_DELTA);
  encoder_LeftMotor.setReversed(false);  // adjust for positive count when moving forward
  encoder_RightMotor.init(1.0 / 3.0 * MOTOR_393_SPEED_ROTATIONS, MOTOR_393_TIME_DELTA);
  encoder_RightMotor.setReversed(true);  // adjust for positive count when moving forward

  //read saved values from EEPROM
  b_LowByte = EEPROM.read(ci_Left_Motor_Offset_Address_L);
  b_HighByte = EEPROM.read(ci_Left_Motor_Offset_Address_H);
  ui_Left_Motor_Offset = word(b_HighByte, b_LowByte);
  b_LowByte = EEPROM.read(ci_Right_Motor_Offset_Address_L);
  b_HighByte = EEPROM.read(ci_Right_Motor_Offset_Address_H);
  ui_Right_Motor_Offset = word(b_HighByte, b_LowByte);
}

void loop()
{
  if ((millis() - ul_3_Second_timer) > 3000)
  {
    bt_3_S_Time_Up = true;
  }

  // button-based mode selection
  if (CharliePlexM::ui_Btn)
  {
    if (bt_Do_Once == false)
    {
      bt_Do_Once = true;
      ui_Robot_State_Index++;
      ui_Robot_State_Index = ui_Robot_State_Index & 7;
      ul_3_Second_timer = millis();
      bt_3_S_Time_Up = false;
      bt_Cal_Initialized = false;
      ui_Cal_Cycle = 0;
    }
  }
  else
  {
    bt_Do_Once = LOW;
  }

  // check if drive motors should be powered
  bt_Motors_Enabled = digitalRead(ci_Motor_Enable_Switch);

  // modes
  // 0 = default after power up/reset
  // 1 = Press mode button once to enter. Run robot.
  // 2 = Press mode button twice to enter. Calibrate line tracker light level.
  // 3 = Press mode button three times to enter. Calibrate line tracker dark level.
  // 4 = Press mode button four times to enter. Calibrate motor speeds to drive straight.
  switch (ui_Robot_State_Index)
  {
    case 0:    //Robot stopped
      {
        servo_LeftMotor.writeMicroseconds(brake);
        servo_RightMotor.writeMicroseconds(brake);
        encoder_LeftMotor.zero();
        encoder_RightMotor.zero();
        ui_Mode_Indicator_Index = 0;
        break;
      }

    case 1:    //Robot Run after 3 seconds
      {
        if (bt_3_S_Time_Up)
        {
#ifdef DEBUG_ENCODERS
          l_Left_Motor_Position = encoder_LeftMotor.getRawPosition();
          l_Right_Motor_Position = encoder_RightMotor.getRawPosition();

          Serial.print("Encoders L: ");
          Serial.print(l_Left_Motor_Position);
          Serial.print(", R: ");
          Serial.println(l_Right_Motor_Position);
#endif

          // set motor speeds
          ui_Left_Motor_Speed = constrain(forwardSpeed + ui_Left_Motor_Offset, 1600, 2100);
          ui_Right_Motor_Speed = constrain(forwardSpeed + ui_Right_Motor_Offset, 1600, 2100);


          /*if (frontLEcho <= 700 && millis() - chargeTimer < 7500)
            {
            phaseA = 2;
            } //test to find perfect distance from wall*/

          infra = analogRead(ci_Light_Sensor);
          Serial.println(infra);

          if (millis() - chargeTimer >= 7500) //test to find how long it takes to reach the charging station
          {
            phaseA = 3;
          }
          switch (phaseA)
          {
            case 1:
              if (dumbCount > 200)
              {
                tempTimer = millis();
                phaseA = 2;
                dumbCount = 0;
              }
              if (frontLEcho <= turnStart && frontREcho <= turnStart)
              {
                dumbCount++;
                bt_Motors_Enabled = false;
              }
              if ((frontLEcho <= turnStart && frontREcho >= turnStart * 1.5) || (frontREcho <= turnStart && frontLEcho >= turnStart * 1.5))
              {
                phaseA = 5;
                bt_Motors_Enabled = false;
              }
              else
              {
                driveStraight(chargeFlag);
              }
              break;
            case 2:
              uTurn(turnFlag);
              if (millis() - tempTimer >= uTurnTime)
              {
                phaseA = 1;
                if (turnFlag == false)
                {
                  turnFlag = true;
                }
                else
                {
                  turnFlag = false;
                }
              } //set phaseA back to 1 after turning
              break;
            case 3:
              chargeFlag = true;
              findBeacon();
              break;
            case 4:
              resumeSweep();
              break;
            case 5:
              obstacleAvoidance();
              break;
          }


          if (bt_Motors_Enabled)
          {
            servo_LeftMotor.writeMicroseconds(ui_Left_Motor_Speed);
            servo_RightMotor.writeMicroseconds(ui_Right_Motor_Speed);
          }
          else
          {
            servo_LeftMotor.writeMicroseconds(brake);
            servo_RightMotor.writeMicroseconds(brake);
          }
#ifdef DEBUG_MOTORS
          Serial.print("Motors enabled: ");
          Serial.print(bt_Motors_Enabled);
          Serial.print(", Default: ");
          Serial.print(ui_Motors_Speed);
          Serial.print(", Left = ");
          Serial.print(ui_Left_Motor_Speed);
          Serial.print(", Right = ");
          Serial.println(ui_Right_Motor_Speed);
#endif
          ui_Mode_Indicator_Index = 1;
        }
        break;
      }

    case 2:    //Calibrate motor straightness after 3 seconds.
      {
        if (bt_3_S_Time_Up)
        {
          if (!bt_Cal_Initialized)
          {
            bt_Cal_Initialized = true;
            encoder_LeftMotor.zero();
            encoder_RightMotor.zero();
            ul_Calibration_Time = millis();
            servo_LeftMotor.writeMicroseconds(forwardSpeed);
            servo_RightMotor.writeMicroseconds(forwardSpeed);
          }
          else if ((millis() - ul_Calibration_Time) > ci_Motor_Calibration_Time)
          {
            servo_LeftMotor.writeMicroseconds(brake);
            servo_RightMotor.writeMicroseconds(brake);
            l_Left_Motor_Position = encoder_LeftMotor.getRawPosition();
            l_Right_Motor_Position = encoder_RightMotor.getRawPosition();
            if (l_Left_Motor_Position > l_Right_Motor_Position)
            {
              // May have to update this if different calibration time is used
              ui_Right_Motor_Offset = 0;
              ui_Left_Motor_Offset = (l_Left_Motor_Position - l_Right_Motor_Position) / 4;
            }
            else
            {
              // May have to update this if different calibration time is used
              ui_Right_Motor_Offset = (l_Right_Motor_Position - l_Left_Motor_Position) / 4;
              ui_Left_Motor_Offset = 0;
            }

#ifdef DEBUG_MOTOR_CALIBRATION
            Serial.print("Motor Offsets: Left = ");
            Serial.print(ui_Left_Motor_Offset);
            Serial.print(", Right = ");
            Serial.println(ui_Right_Motor_Offset);
#endif
            EEPROM.write(ci_Right_Motor_Offset_Address_L, lowByte(ui_Right_Motor_Offset));
            EEPROM.write(ci_Right_Motor_Offset_Address_H, highByte(ui_Right_Motor_Offset));
            EEPROM.write(ci_Left_Motor_Offset_Address_L, lowByte(ui_Left_Motor_Offset));
            EEPROM.write(ci_Left_Motor_Offset_Address_H, highByte(ui_Left_Motor_Offset));

            ui_Robot_State_Index = 0;    // go back to Mode 0
          }
#ifdef DEBUG_MOTOR_CALIBRATION
          Serial.print("Encoders L: ");
          Serial.print(encoder_LeftMotor.getRawPosition());
          Serial.print(", R: ");
          Serial.println(encoder_RightMotor.getRawPosition());
#endif
          ui_Mode_Indicator_Index = 4;
        }
        break;
      }
  }

  if ((millis() - ul_Display_Time) > ci_Display_Time)
  {
    ul_Display_Time = millis();

#ifdef DEBUG_MODE_DISPLAY
    Serial.print("Mode: ");
    Serial.println(ui_Mode_Indicator[ui_Mode_Indicator_Index], DEC);
#endif
    bt_Heartbeat = !bt_Heartbeat;
    CharliePlexM::Write(ci_Heartbeat_LED, bt_Heartbeat);
    digitalWrite(13, bt_Heartbeat);
    Indicator();
  }
}

// set mode indicator LED state
void Indicator()
{
  //display routine, if true turn on led
  CharliePlexM::Write(ci_Indicator_LED, !(ui_Mode_Indicator[ui_Mode_Indicator_Index] &
                                          (iArray[iArrayIndex])));
  iArrayIndex++;
  iArrayIndex = iArrayIndex & 15;
}

// measure distance to target using ultrasonic sensor
unsigned long ping(const int output, const int input)
{
  //ping Ultrasonic
  //Send the Ultrasonic Range Finder a 10 microsecond pulse per tech spec
  digitalWrite(output, HIGH);
  delayMicroseconds(10);  //The 10 microsecond pause where the pulse in "high"
  digitalWrite(output, LOW);
  //use command pulseIn to listen to Ultrasonic_Data pin to record the
  //time that it takes from when the Pin goes HIGH until it goes LOW
  return pulseIn(input, HIGH, 10000);

  // Print Sensor Readings
#ifdef DEBUG_ULTRASONIC
  Serial.print("Time (microseconds): ");
  Serial.print(frontLEcho, DEC);
  Serial.print(", Inches: ");
  Serial.print(frontLEcho / 148); //divide time by 148 to get distance in inches
  Serial.print(", cm: ");
  Serial.println(frontLEcho / 58); //divide time by 58 to get distance in cm
#endif
}


void obstacleAvoidance()
{
  switch (phaseD)
  {
    case 1:
      if (turnFlag == false) //turn left
      {

      }
  }
}

void driveStraight(bool direction)
{
  frontLEcho = ping(frontLPing, frontLData);
  frontREcho = ping(frontRPing, frontRData);
  if (direction == false)
  {
    ui_Left_Motor_Speed = forwardSpeed;
    ui_Right_Motor_Speed = forwardSpeed;
    bt_Motors_Enabled = true;
  }
  else
  {
    ui_Left_Motor_Speed = reverseSpeed;
    ui_Right_Motor_Speed = reverseSpeed;
    bt_Motors_Enabled = true;
  }

}

void uTurn(bool direction)
{
  if (direction == false)
  {
    ui_Left_Motor_Speed = brake;
    ui_Right_Motor_Speed = forwardSpeed;
    bt_Motors_Enabled = true;
  }
  else
  {
    ui_Left_Motor_Speed = forwardSpeed;
    ui_Right_Motor_Speed = brake;
    bt_Motors_Enabled = true;
  }
}

void findBeacon() // keep rotating on the spot until the beacon is found
{

}

void resumeSweep()
{
  secondRound = true;
  switch (phaseC)
  {
    case 1:
      driveStraight(chargeFlag);
      if (millis() - tempTimer >= distanceKeeper)
      {
        tempTimer = millis();
        phaseC = 2;
      }
      break;
    case 2: // must rotate opposite to findBeacon: case 2
      ui_Left_Motor_Speed = reverseSlow;
      ui_Right_Motor_Speed = forwardSlow;
      bt_Motors_Enabled = true;
      if (millis() - tempTimer >= angleKeeper)
      {
        phaseA = 1;
      }
      break;
  }
}
