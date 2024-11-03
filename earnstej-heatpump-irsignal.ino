// earnstej-heatpump-irsignal
// Copyright (c) 2020-2024 Lasse Nielsen
// Controlling a heat pump via IR (e.g. TSAL6400 LED). 

#include <avr/wdt.h>
#include <PanasonicHeatpumpIR.h> // HeatPumpIR library by Toni Arte (https://github.com/ToniA/arduino-heatpumpir) - last tested with version 1.0.15

#define IR_SENDER_PIN            3 // PWM pin
#define EXT_LED_OUTPUT_PIN       4
#define FORCE_UPDATE_INPUT_PIN   5
#define VERTICAL_DIR_INPUT_PIN   6
#define HEAT_TOGGLE_IN1_PIN      7
#define HEAT_TOGGLE_IN2_PIN      8
#define HEAT_TOGGLE_IN3_PIN      9
#define TEMP_SELECT_OUT_PIN     10
#define TEMP_SELECT_IN1_PIN     11
#define TEMP_SELECT_IN2_PIN     12
#define INT_LED_OUTPUT_PIN      13

#define TEMPERATURE_SETPOINT_0     22 // Pins not wired
#define TEMPERATURE_SETPOINT_1     23 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN1_PIN
#define TEMPERATURE_SETPOINT_2     24 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN2_PIN
#define TEMPERATURE_SETPOINT_3     25 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN1_PIN+TEMP_SELECT_IN2_PIN
#define UPDATE_INTERVAL_MSEC      200
#define IR_SIGNAL_PAUSE_BLINKS      3 // x 2 x UPDATE_INTERVAL_MSEC = pause duration
#define STEADY_INPUT_STATE_MSEC  100UL // Must be short enough to allow a user briefly pressing a button, but long enough to handle debounching
#define BOOT_DELAY_SECONDS         10 // Must be at least 2 seconds because of watchdog disabling during startup
//#define REBOOT_INTERVAL_MSEC     700000000UL // A little more than a week

#define HEAT_TIME_MILLIS_1  (12 * 60 * 60 * 1000) // 12 hours - enough for a meeting
#define HEAT_TIME_MILLIS_2  (24 * 60 * 60 * 1000) // 24 hours - enough for a one-day event
#define HEAT_TIME_MILLIS_3  (72 * 60 * 60 * 1000) // 72 hours - enough for a weekend

#define HEAT_MILLIS_MAX  HEAT_TIME_MILLIS_3 // The highest of the heat times

HeatpumpIR *pHeatpumpIR = new PanasonicNKEHeatpumpIR(); // NKE model has 8/10 degrees maintenance with max. fan speed //PanasonicDKEHeatpumpIR();

IRSenderPWM irSender(IR_SENDER_PIN);  // IR led on Arduino, using Arduino PWM

unsigned long lastStateChangeTick = 1;

bool togglingHeatMode = false;

int heatInputState = -1;
int verticalDirInputState = -1;
int updateInputState = -1;

unsigned long heatOffMillis = 0; // Heat off

void setup()
{
  wdt_disable();  /* Disable the watchdog and wait for more than 2 seconds so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */

  Serial.begin(9600);
  Serial.println(F("setup()"));

  pinMode(HEAT_TOGGLE_IN1_PIN,      INPUT_PULLUP);
  pinMode(HEAT_TOGGLE_IN2_PIN,      INPUT_PULLUP);
  pinMode(HEAT_TOGGLE_IN3_PIN,      INPUT_PULLUP);
  pinMode(VERTICAL_DIR_INPUT_PIN,   INPUT_PULLUP);
  pinMode(FORCE_UPDATE_INPUT_PIN,   INPUT_PULLUP);
  pinMode(TEMP_SELECT_OUT_PIN,      OUTPUT);
  pinMode(TEMP_SELECT_IN1_PIN,      INPUT_PULLUP);
  pinMode(TEMP_SELECT_IN2_PIN,      INPUT_PULLUP);
  pinMode(INT_LED_OUTPUT_PIN,       OUTPUT);        
  pinMode(EXT_LED_OUTPUT_PIN,       OUTPUT);        
  digitalWrite(INT_LED_OUTPUT_PIN,  LOW);
  digitalWrite(EXT_LED_OUTPUT_PIN,  LOW);
  digitalWrite(TEMP_SELECT_OUT_PIN, LOW);

  // Because the heat pump may be booting up at the same time,
  // delay the inital IR signalling so allow it to get ready.
  for (int i = 0; i < BOOT_DELAY_SECONDS; ++i)
  {
    digitalWrite(INT_LED_OUTPUT_PIN, HIGH);
    digitalWrite(EXT_LED_OUTPUT_PIN, HIGH);
    delay(500);
    digitalWrite(INT_LED_OUTPUT_PIN, LOW);
    digitalWrite(EXT_LED_OUTPUT_PIN, LOW);
    delay(500);
  }

  wdt_enable(WDTO_8S);  /* Enable the watchdog with a timeout of 8 seconds */
}

void loop()
{
  wdt_reset(); /* Reset the watchdog */

  delay(UPDATE_INTERVAL_MSEC);

//  // Time to reboot at reqular interval?
//  if (millis() > REBOOT_INTERVAL_MSEC)
//  {
//    // Let the watchdog do the rebooting
//    while (true);
//  }
  
  if (checkUpdatedInputs())
  {
    toggleHeatModeIfRelevant();
    updateIR();
  }
}

bool checkUpdatedInputs()
{
  // Heat input considered activated (low) if at least one of the heat inputs is low
  int heatInputVal = ((digitalRead(HEAT_TOGGLE_IN1_PIN) == LOW) || (digitalRead(HEAT_TOGGLE_IN2_PIN) == LOW) || (digitalRead(HEAT_TOGGLE_IN3_PIN) == LOW)) ? LOW : HIGH;

  int verticalDirInputVal = digitalRead(VERTICAL_DIR_INPUT_PIN);
  int updateInputVal = digitalRead(FORCE_UPDATE_INPUT_PIN);

  // Just changing input state?
  if ((heatInputVal != heatInputState) || (verticalDirInputVal != verticalDirInputState) || (updateInputVal != updateInputState))
  {
    lastStateChangeTick = millis();

    digitalWrite(INT_LED_OUTPUT_PIN, HIGH);
    digitalWrite(EXT_LED_OUTPUT_PIN, HIGH);

    // Don't use the tick value 0 - reserved
    if (lastStateChangeTick == 0)
      lastStateChangeTick = 1;

    // Seeing a change on the heat input?
    if (heatInputVal != heatInputState)
    {
      // High-to-low, i.e. user pressing the button?
      // If low-to-high we either see end of button press or bounching button
      togglingHeatMode = (heatInputVal == LOW);
    }

    heatInputState = heatInputVal;
    verticalDirInputState = verticalDirInputVal;
    updateInputState = updateInputVal;

    // Not steady yet
  }
  else // Unchanged input
  {
    // Still waiting for steadiness
    if (lastStateChangeTick != 0)
    {
      // Blink external LED by toggling state at each update
      digitalWrite(EXT_LED_OUTPUT_PIN, (digitalRead(EXT_LED_OUTPUT_PIN) == LOW) ? HIGH : LOW);
      
      if (millis() - lastStateChangeTick >= STEADY_INPUT_STATE_MSEC)
      {
        lastStateChangeTick = 0;
        return true;
      }
    }
  }

  return false;
}

void toggleHeatModeIfRelevant()
{
    // Not toggling heat mode?
    if (!togglingHeatMode || (heatInputState == HIGH))
      return;

    if (digitalRead(HEAT_TOGGLE_IN1_PIN) == LOW)
    {
      // Toggle heating interval 1
      heatOffMillis = isHeating() ? 0 : millis() + HEAT_TIME_MILLIS_1;
    }
    else if (digitalRead(HEAT_TOGGLE_IN2_PIN) == LOW)
    {
      // Toggle heating interval 2
      heatOffMillis = isHeating() ? 0 : millis() + HEAT_TIME_MILLIS_2;
    }
    else if (digitalRead(HEAT_TOGGLE_IN3_PIN) == LOW)
    {
      // Toggle heating interval 3
      heatOffMillis = isHeating() ? 0 : millis() + HEAT_TIME_MILLIS_3;
    }

    togglingHeatMode = false;
}

void updateIR()
{
  Serial.println(F("updateIR()"));

  byte power = isHeating() ? POWER_ON : POWER_OFF;
  byte temp  = isHeating() ? getTemperatureSetpoint() : 0;
  byte vertDir = getVerticalDirection();

  Serial.print(F("Power: ")); Serial.println(power);
  Serial.print(F("Temp: ")); Serial.println(temp);
  Serial.print(F("VertDir: ")); Serial.println(vertDir);

  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, vertDir, HDIR_AUTO);
  pauseIRWhileBlinkingExtLED();
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, vertDir, HDIR_AUTO);
  pauseIRWhileBlinkingExtLED();
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, vertDir, HDIR_AUTO);

  digitalWrite(INT_LED_OUTPUT_PIN, LOW);

  // Keep external LED on while heating
  digitalWrite(EXT_LED_OUTPUT_PIN, isHeating() ? HIGH : LOW);
}

void pauseIRWhileBlinkingExtLED()
{
  for (byte i = 0; i < IR_SIGNAL_PAUSE_BLINKS * 2 ; ++i)
  {
    // Blink external LED by toggling state at each update
    digitalWrite(EXT_LED_OUTPUT_PIN, (digitalRead(EXT_LED_OUTPUT_PIN) == LOW) ? HIGH : LOW);

    delay(UPDATE_INTERVAL_MSEC);
  }  
}

bool isHeating()
{
  // Heating?
  if (heatOffMillis != 0)
  {
    unsigned long heatMillisLeft = heatOffMillis - millis();

    // Overflow, i.e. no heating time left
    if (heatMillisLeft > HEAT_MILLIS_MAX)
      heatOffMillis = 0; // Turn off heating
  }

  // Heating?
  return (heatOffMillis != 0);
}

bool getVerticalDirection()
{
  return (verticalDirInputState == LOW) ? VDIR_UP : VDIR_DOWN;
}

byte getTemperatureSetpoint()
{
  switch (((digitalRead(TEMP_SELECT_IN2_PIN) == LOW) ? 2 : 0) + ((digitalRead(TEMP_SELECT_IN1_PIN) == LOW) ? 1 : 0))
  {
    case 3:
      return TEMPERATURE_SETPOINT_3;
    case 2:
      return TEMPERATURE_SETPOINT_2;
    case 1:
      return TEMPERATURE_SETPOINT_1;
    case 0:
    default:
      return TEMPERATURE_SETPOINT_0;
  }
}
