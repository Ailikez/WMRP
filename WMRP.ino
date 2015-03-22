// some preprocessor defines
//DIGITAL INPUT PINS
// Values after the // are the one for my pcb. Other for debugging on Arduino Uno!
#define PIN_SW_1         12
#define PIN_SW_2         3//5
#define PIN_SW_3         9
#define PIN_ROT_A        8
#define PIN_ROT_B        7//6
#define PIN_ROT_SW       4

#define DEBUG_LED        2//6

//ADC INPUT PINS
#define PIN_ADC_T_GRIP   A1
#define PIN_ADC_T_TIP    A0
#define PIN_ADC_U_IN     A4
#define PIN_ADC_I_HEATER A3
#define PIN_ADC_STAND    A2

//OUTPUT PINS
#define PIN_HEATER       10
#define PIN_STAND        13
//#define PIN_I2C_SDA    2
//#define PIN_I2C_SCL    3
//#define PIN_MOSI       16
//#define PIN_MISO       17
//#define PIN_SCK        15
//#define PIN_RXI        0
//#define PIN_TXO        1

//TIMES
#define TIMER1_PERIOD_US        500
#define DELAY_BEFORE_MEASURE_MS 50
#define TIME_SW_POLL_MS         10
#define TIME_TUI_MEAS_MS        100
#define TIME_SERIAL_MS          500
#define TIME_LCD_MS             500
#define TIME_CYCLECOUNT_MS      1000
#define TIME_EEPROM_WRITE_MS    30000
#define TIME_STAND_MS           1000
#define DELAY_BEFORE_STAND_MS   5

//THRESHOLDS
#define THRESHOLD_STAND 300

//PID CONTROL
#define CNTRL_P_GAIN 0
#define CNTRL_I_GAIN 1
#define CNTRL_D_GAIN 0

//TEMPERATURES
#define STDBY_TEMP_DEG      150
#define MIN_TARGET_TEMP_DEG 150
#define MAX_TARGET_TEMP_DEG 450

//PWM MAX
#define PWM_MAX_VALUE       1024

//ADC TO TEMP CONVVERSION
#define ADC_TO_T_TIP_A0  -1176200 //inverse function: t[*C] = - 1176.2 + 3.34 * SQRT(14978 * U[mV] + 130981)
#define ADC_TO_T_TIP_A1  3340     //                  u[mV] = adc_value * 4mV * 1/OPAMP_T_TIP_GAIN
#define ADC_TO_T_TIP_A2  139      //           1000 * t[*C] = -1176200 + 3340 * SQRT(139 * adc_value + 130981)
#define ADC_TO_T_TIP_A3  130981
#define ADC_TO_T_GRIP_A0 289      //inverse function: t[*C] = (u[V] - 1.1073) * 1000/6.91
#define ADC_TO_T_GRIP_A1 -160246  //                   u[V] = adc_value * 4mV/1000 * 1/OPAMP_T_GRIP_GAIN
//                                             1000 * t[*C] = adc_value * 289.435 - 160246
#define ADC_TO_U_IN_A0 14213      //               U_adc[V] = U_IN[V] * 4700/(4700 + 12000); U_adc[V] = adc_value * 4 mV/1000
//                                                  U_IN[V] = adc_value * 4 mV/1000 *(4700 + 1200)/4700
//                                           10^6 * U_IN[V] = adc_value * 14213
#define ADC_TO_I_HEATER_A0 18182  //              U_adc[mV] = UVCC/2[mV] + I_HEATER[A] * 220 mV/A; U_adc[mV] = adc_value * 4 mV
//                                              I_HEATER[A] = (adc_value * 4 mV - UVCC/2)/(220 mV/A); UVCC/2[mV] = adc_value(@i=0A) * 4 mV
//                                       10^6 * I_HEATER[A] = (adc_value - adc_value(@i=0A)) * 18182
//#define OPAMP_T_TIP_GAIN    431
//#define OPAMP_T_GRIP_GAIN   2
//#define OPAMP_I_HEATER_GAIN 2.2

//EEPROM ADDRESS
#define EEPROM_ADDRESS_TEMP_START 0
#define EEPROM_ADDRESS_TEMP_END   1
#define EEPROM_ADDRESS_VAL1_START 2
#define EEPROM_ADDRESS_VAL1_END   3
#define EEPROM_ADDRESS_VAL2_START 4
#define EEPROM_ADDRESS_VAL2_END   5
#define EEPROM_ADDRESS_VAL3_START 6
#define EEPROM_ADDRESS_VAL3_END   7


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "TimerOne.h"
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address
#include <Encoder.h>
Encoder enc(PIN_ROT_A, PIN_ROT_B);

#define SW_VERSION      "Version 1.0"
#define DEBUG true

//GLOBAL VARIABLES
long    temperature_tip_absolute;
int adc_temperature_tip_relative;
long    temperature_tip_relative;
int adc_temperature_grip;
long    temperature_grip;
int adc_voltage_input;
long    voltage_input;
int adc_current_heater;
long    current_heater;

int adc_current_heater_offset;
int pwm_value;
byte pwm_percent;
int temp_setpoint;
int temp_setpoint_old;
int encoder_value;

boolean state_switch[5];
boolean state_switch_old[5];// = {false, true, true, true};
byte button_count[5];

boolean meas_flag;

boolean stand_flag;
boolean in_stand;

//variables for cycle count
int cycle;
int last_cycle;

//chars for bargraph
byte p1[8] = {
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000,
  0b10000
};

byte p2[8] = {
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000,
  0b11000
};

byte p3[8] = {
  0b11100,
  0b11100,
  0b11100,
  0b11100,
  0b11100,
  0b11100,
  0b11100,
  0b11100
};

byte p4[8] = {
  0b11110,
  0b11110,
  0b11110,
  0b11110,
  0b11110,
  0b11110,
  0b11110,
  0b11110
};

byte p5[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111
};
byte temperatur_char[8] = {
  0b10010,
  0b11010,
  0b10010,
  0b11010,
  0b10010,
  0b11110,
  0b11110,
  0b11110
};
byte gradcelsius_char[8] = {
  0b01000,
  0b10100,
  0b01000,
  0b00011,
  0B00100,
  0b00100,
  0b00100,
  0b00011
};
byte arrow_char[8] = {
  0b11111,
  0b00100,
  0b01110,
  0b10101,
  0b00100,
  0b00100,
  0b00100,
  0b00100
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CLASSES

//timer_over class
class timer_over {
  private:
    unsigned long _last_time;
  public:
    timer_over() {
      _last_time = 0;
    }
    boolean over(int sample_time) {
      unsigned long now = millis();
      unsigned long time_change = (now - this->_last_time);

      if (time_change >= sample_time) {
        this->_last_time = now;
        return true;
      }
      else {
        return false;
      }
    }
    boolean set_timer() {
      this->_last_time = millis();
    }
};
//timer_over class members
timer_over timer_meas;
timer_over timer_serial;
timer_over timer_lcd;
timer_over timer_switch;
timer_over timer_cyclecount;
timer_over timer_eeprom;
timer_over timer_delay_before_measure;
timer_over timer_stand;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//FUNCTIONS

void draw_bar(int value, byte numCols, int maxValue) {
  static int prevValue;
  static byte lastFullChars;

  byte fullChars = (long)value * numCols / maxValue;
  byte mod = ((long)value * numCols * 5 / maxValue) % 5;

  int normalizedValue = (int)fullChars * 5 + mod;

  if (prevValue != normalizedValue) {

    for (byte i = 0; i < fullChars; i++) {
      lcd.write(4);
    }
    if (mod > 0) {
      switch (mod) {
        case 0:
          break;
        case 1:
          lcd.print((char)0);
          break;
        case 2:
          lcd.write(1);
          break;
        case 3:
          lcd.write(2);
          break;
        case 4:
          lcd.write(3);
          break;
          ++fullChars;
      }
    }
    for (byte i = fullChars; i < lastFullChars; i++) {
      lcd.write(' ');
    }

    // -- save cache
    lastFullChars = fullChars;
    prevValue = normalizedValue;
  }
}


void update_measurments() {
  //empty
}


int calculate_pid(int setpoint, int input, float kp, float ki, float kd, int sample_time, int out_max) {
  //float pid(float setpoint, float input, float kp, float ki, float kd, unsigned int sample_time, float out_max, float &i_value, float &p_value) {
  static unsigned long last_time;
  static float last_input;
  static float iterm;
  float error;
  float dterm;
  float pterm;
  float output;
  float out_min = 0;

  float sample_time_in_sec = ((float)sample_time) / 1000;

  kp = kp;
  ki = ki * sample_time_in_sec;
  kd = kd / sample_time_in_sec;

  // compute all the working error variables
  error = setpoint - input;

  // (i)ntegral
  iterm += (ki * error);

  if (iterm > out_max) iterm = out_max;
  else if (iterm < out_min) iterm = out_min;

  // (d)erivative
  dterm = kd * (input - last_input);

  // (p)roportional
  pterm = kp * error;

  // compute PID output
  output = pterm + iterm - dterm;

  if (output > out_max) output = out_max;
  else if (output < out_min) output = out_min;

  // Remember some variables for next time
  last_input  = input;
  return (int)output;
}


long adc_to_t_tip_calc(int adc_value) {
  long t_tip = ADC_TO_T_TIP_A0 + (ADC_TO_T_TIP_A1 * sqrt((ADC_TO_T_TIP_A2 * (long)adc_value) + ADC_TO_T_TIP_A3));
  //Serial.println((float)t_tip / 1000, 3);
  return t_tip;
}


long adc_to_t_grip_calc(int adc_value) {
  long t_grip = (ADC_TO_T_GRIP_A0 * (long)adc_value) + ADC_TO_T_GRIP_A1;
  //Serial.println((float)t_grip / 1000, 3);
  return t_grip;
}


long adc_to_u_in_calc(int adc_value) {
  long u_in = (ADC_TO_U_IN_A0 * (long)adc_value);
  //Serial.println((float)u_in / 1000000, 3);
  return u_in;
}


long adc_to_i_heater_calc(int adc_value, int adc_value_offset) {
  long i_heater = ADC_TO_I_HEATER_A0 * ((long)adc_value - (long)adc_value_offset);
  //Serial.println((float)i_heater / 1000000, 3);
  return i_heater;
}


void  serial_draw_line() {
  Serial.println("--------------------------------------");
}

int eeprom_read_int(int addr) {
  int value = EEPROM.read(addr) | (((int) EEPROM.read(addr + 1)) << 8);
  value = constrain(value, MIN_TARGET_TEMP_DEG, MAX_TARGET_TEMP_DEG);
  if (DEBUG) {
    Serial.print("Read from EEPROM Address: ");
    Serial.print(addr);
    Serial.print("/");
    Serial.println(addr + 1);
    Serial.print("Value: ");
    Serial.println(value);
    serial_draw_line();
  }
  return value;
}

void eeprom_write_int(int addr, int value) {
  EEPROM.write(addr, value & 0xff);
  EEPROM.write(addr + 1, (value & 0xff00) >> 8);

  if (DEBUG) {
    Serial.print("Write to EEPROM Address: ");
    Serial.print(addr);
    Serial.print("/");
    Serial.println(addr + 1);
    Serial.print("Value: ");
    Serial.println(value);
    serial_draw_line();
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//SETUP ROUTINE

void setup()
{
  //OUTPUT PINS
  pinMode(PIN_HEATER, OUTPUT);
  pinMode(PIN_STAND, OUTPUT);

  pinMode(DEBUG_LED, OUTPUT);


  //DIGITAL INPUT PINS
  pinMode(PIN_SW_1, INPUT);
  digitalWrite(PIN_SW_1, HIGH);
  pinMode(PIN_SW_2, INPUT);
  digitalWrite(PIN_SW_2, HIGH);
  pinMode(PIN_SW_3, INPUT);
  digitalWrite(PIN_SW_3, HIGH);
  pinMode(PIN_ROT_A, INPUT);
  digitalWrite(PIN_ROT_A, HIGH);
  pinMode(PIN_ROT_B, INPUT);
  digitalWrite(PIN_ROT_B, HIGH);
  pinMode(PIN_ROT_SW, INPUT);
  digitalWrite(PIN_ROT_SW, HIGH);

  //ADC INPUT PINS
  pinMode(PIN_ADC_T_GRIP, INPUT);
  pinMode(PIN_ADC_U_IN, INPUT);
  pinMode(PIN_ADC_I_HEATER, INPUT);
  pinMode(PIN_ADC_STAND, INPUT);


  Timer1.initialize(TIMER1_PERIOD_US); //initialize timer1, and set a 500 second period == 2 kHz
  Timer1.attachInterrupt(isr_routine); //attaches callback() as a timer overflow interrupt
  Timer1.pwm(PIN_HEATER, 0);           //setup pwm pin, 0 % duty cycle

  delay(100);
  adc_current_heater_offset = analogRead(PIN_ADC_I_HEATER); //read adc offset for current measurement @ I_HEATER = 0 A

  temp_setpoint = eeprom_read_int(EEPROM_ADDRESS_TEMP_START); //read eeprom
  temp_setpoint_old = temp_setpoint;

  enc.write(temp_setpoint);   //set encoder value to temp_setpoint

  Serial.begin(115200);       //start serial communication
  Serial.println(SW_VERSION); //print software version

  delay(100);
  lcd.begin(16, 2);      //initialize the lcd for 16 chars 2 lines
  lcd.createChar(0, p1); //create chars for bargraph
  lcd.createChar(1, p2);
  lcd.createChar(2, p3);
  lcd.createChar(3, p4);
  lcd.createChar(4, p5);
  lcd.createChar(5, temperatur_char);
  lcd.createChar(6, gradcelsius_char);
  lcd.createChar(7, arrow_char);
  lcd.clear();
  delay(100);
}

void isr_routine()
{
  //isr routine - not used now
}

void loop()
{
  digitalWrite(DEBUG_LED, !digitalRead(DEBUG_LED));


  if (timer_cyclecount.over(TIME_CYCLECOUNT_MS)) {
    //here comes the cycles per second count
    last_cycle = cycle;
    cycle = 0;
  }
  else {
    cycle++;
  }


  if (timer_eeprom.over(TIME_EEPROM_WRITE_MS)) {
    //here comes the eeprom write
    if (temp_setpoint != temp_setpoint_old) {
      eeprom_write_int(EEPROM_ADDRESS_TEMP_START, temp_setpoint);
      temp_setpoint_old = temp_setpoint;
    }
  }


  if (timer_meas.over(TIME_TUI_MEAS_MS)) {
    //here comes the start of the measurement
    Timer1.setPwmDuty(PIN_HEATER, 0);         //stop heating
    meas_flag = true;                         //set meas_flag
    timer_delay_before_measure.set_timer();   //set delay timer to actual time
    //if (DEBUG) {Serial.print("S: "); Serial.println(millis()); }
  }


  if (timer_delay_before_measure.over(DELAY_BEFORE_MEASURE_MS) && meas_flag) {
    meas_flag = false;
    //delay(DELAY_BEFORE_MEASURE_MS);   //wait for steady state of all filters and opmap
    adc_temperature_tip_relative = analogRead(PIN_ADC_T_TIP);
    adc_temperature_grip         = analogRead(PIN_ADC_T_GRIP);
    adc_voltage_input            = analogRead(PIN_ADC_U_IN);
    adc_current_heater           = analogRead(PIN_ADC_I_HEATER);

    //code for stand recognition....
    if (timer_stand.over(TIME_STAND_MS)) {
      digitalWrite(PIN_STAND, 1);
      delay(DELAY_BEFORE_STAND_MS);
      if (analogRead(PIN_ADC_STAND) < THRESHOLD_STAND) {
        in_stand = 1;
      }
      else {
        in_stand = 0;
      }
      digitalWrite(PIN_STAND, 0);
    }

    Timer1.setPwmDuty(PIN_HEATER, pwm_value); //set pwm back to old value

    //calculate SI values form the adc value
    temperature_tip_relative = adc_to_t_tip_calc(adc_temperature_tip_relative);                     //temperature in grad celsius divide with 1000
    temperature_grip         = adc_to_t_grip_calc(adc_temperature_grip);                            //temperature in grad celsius divide with 1000
    voltage_input            = adc_to_u_in_calc(adc_voltage_input);                                 //voltage in volt divide with 10^6
    current_heater           = adc_to_i_heater_calc(adc_current_heater, adc_current_heater_offset); //current in volt divide with 10^6

    //calculate real tip temperature as sum of thermo couple temperature and cold junction temperature
    temperature_tip_absolute = temperature_tip_relative + temperature_grip;

    //temp_setpoint = constrain(temp_setpoint, MIN_TARGET_TEMP_DEG, MAX_TARGET_TEMP_DEG);
    pwm_value = calculate_pid(temp_setpoint, (int)(temperature_tip_absolute / 1000), CNTRL_P_GAIN, CNTRL_I_GAIN, CNTRL_D_GAIN, TIME_TUI_MEAS_MS, PWM_MAX_VALUE);
    Timer1.setPwmDuty(PIN_HEATER, pwm_value);
    //Serial.println("Timer over...");
  }


  if (timer_serial.over(TIME_SERIAL_MS)) {
    //here comes the serial output
    Serial.print("Input Voltage:       ");
    Serial.println((float)voltage_input / 1000000, 3);
    Serial.print("Heater Current:      ");
    Serial.println((float)current_heater / 1000000, 3);
    Serial.print("Temp Grip:           ");
    Serial.println((float)temperature_grip / 1000, 3);
    Serial.print("Temp Tip (relative): ");
    Serial.println((float)temperature_tip_relative / 1000, 3);
    Serial.print("Temp Tip (absolute): ");
    Serial.println((float)temperature_tip_absolute / 1000, 3);
    Serial.print("Setpoint Temp:       ");
    Serial.println(temp_setpoint);
    Serial.print("PWM:                 ");
    Serial.println(pwm_value);
    Serial.print("Cycles per sec:      ");
    Serial.println(last_cycle);
    serial_draw_line();
  }


  if (timer_lcd.over(TIME_LCD_MS)) {
    //here comes the lcd output
    //first line
    //"temperature symbol (0), temp_tip_abs (1,2,3), grad celsius symbol (4), blank (5), setpoint symbol (6), temp_set (7,8,9), grad celsius symbol (10)"
    lcd.setCursor(0, 0); //Start at character 0 on line 0
    lcd.write(byte(5));
    if (int(temperature_tip_absolute / 1000) < 10)   lcd.print(" ");
    if (int(temperature_tip_absolute / 1000) < 100)  lcd.print(" ");
    lcd.print(int(temperature_tip_absolute / 1000));
    lcd.write(byte(6));

    lcd.setCursor(6, 0); //Start at character 6 on line 0
    lcd.write(byte(7));
    lcd.print(temp_setpoint);
    lcd.write(byte(6));

    lcd.setCursor(12, 0); //Start at character 6 on line 0
    if (in_stand == 1) {
      lcd.print("IDLE");
    }
    else {
      if (pwm_value == 0) {
        lcd.print("COOL");
      }
      else {
        lcd.print("HEAT");
      }
    }

    //second line
    //pwm in percent (0,1,2), % (3), blank (4), bargraph (5-15)
    pwm_percent = map(pwm_value, 0, PWM_MAX_VALUE, 0, 100);
    lcd.setCursor(0, 1); //Start at character 0 on line 1
    if (pwm_percent < 10)  lcd.print(" ");
    if (pwm_percent < 100) lcd.print(" ");
    lcd.print(pwm_percent);
    lcd.print("%");

    lcd.setCursor(5, 1); //Start at character 5 on line 1
    draw_bar(pwm_percent, 11, 100);
  }


  if (timer_switch.over(TIME_SW_POLL_MS)) {
    //here comes the switch reading
    state_switch[0] = !digitalRead(PIN_SW_1);
    state_switch[1] = !digitalRead(PIN_SW_2);
    state_switch[2] = !digitalRead(PIN_SW_3);
    state_switch[3] = !digitalRead(PIN_ROT_SW);

    if (state_switch[0] && state_switch_old[0]) {
      if (button_count[0] == 100) {
        eeprom_write_int(EEPROM_ADDRESS_VAL1_START, temp_setpoint);  //safe temperature value to eeprom if buttonCount is equal 100
      }
      button_count[0]++;          //count up if button is and was pushed
    }
    else {
      if (!state_switch[0] && state_switch_old[0]) {                 //detect falling edge, releasing the button
        temp_setpoint = eeprom_read_int(EEPROM_ADDRESS_VAL1_START);  //read temp from eeeprom
        enc.write(temp_setpoint);
      }
      button_count[0] = 0;                                              //resett counter
    }


    if (state_switch[1] && state_switch_old[1]) {
      if (button_count[1] == 100) {
        eeprom_write_int(EEPROM_ADDRESS_VAL2_START, temp_setpoint);  //safe temperature value to eeprom if buttonCount is equal 100
        enc.write(temp_setpoint);
      }
      button_count[1]++;          //count up if button is and was pushed
    }
    else {
      if (!state_switch[1] && state_switch_old[1]) {                 //detect falling edge, releasing the button
        temp_setpoint = eeprom_read_int(EEPROM_ADDRESS_VAL2_START);  //read temp from eeeprom
        enc.write(temp_setpoint);
      }
      button_count[1] = 0;                                              //resett counter
    }


    if (state_switch[2] && state_switch_old[2]) {
      if (button_count[2] == 100) {
        eeprom_write_int(EEPROM_ADDRESS_VAL3_START, temp_setpoint);  //safe temperature value to eeprom if buttonCount is equal 100
      }
      button_count[2]++;          //count up if button is and was pushed
    }
    else {
      if (!state_switch[2] && state_switch_old[2]) {                 //detect falling edge, releasing the button
        temp_setpoint = eeprom_read_int(EEPROM_ADDRESS_VAL3_START);  //read temp from eeeprom
        enc.write(temp_setpoint);
      }
      button_count[2] = 0;                                              //resett counter
    }


    if (!state_switch[3] && state_switch_old[3]) {
      if (DEBUG) {
        Serial.println("Switch Rotary rising edge...");
        serial_draw_line();
      }
    }

    memcpy(&state_switch_old, &state_switch, sizeof(state_switch)); // save values
  }


  if (1) {
    //here comes the rotary encoder reading (every loop)
    encoder_value = enc.read();
    if (abs(temp_setpoint - encoder_value) >= 4) {
      temp_setpoint = temp_setpoint + (encoder_value - temp_setpoint) / 4; //read rotary encoder
      temp_setpoint = constrain(temp_setpoint, MIN_TARGET_TEMP_DEG, MAX_TARGET_TEMP_DEG);
      enc.write(temp_setpoint);
    }
  }
}

