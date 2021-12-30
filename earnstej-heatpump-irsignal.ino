// earnstej-heatpump-irsignal
// Copyright (c) 2020 Lasse Nielsen
// Controlling a heat pump via IR (e.g. TSAL6400 LED). 

#include <avr/wdt.h>
#include <PanasonicHeatpumpIR.h> // HeatPumpIR library by Toni Arte (https://github.com/ToniA/arduino-heatpumpir) - last tested with version 1.0.15

#define IR_SENDER_PIN           3 // PWM pin
#define FORCE_UPDATE_INPUT_PIN  5
#define FROST_INPUT_PIN         6
#define HEAT_INPUT_PIN1         7
#define HEAT_INPUT_PIN2         8
#define HEAT_INPUT_PIN3         9
#define TEMP_SELECT_OUT_PIN     10
#define TEMP_SELECT_IN1_PIN     11
#define TEMP_SELECT_IN2_PIN     12
#define LED_OUTPUT_PIN          13

#define TEMPERATURE_SETPOINT_0     22 // Pins not wired
#define TEMPERATURE_SETPOINT_1     23 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN1_PIN
#define TEMPERATURE_SETPOINT_2     24 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN2_PIN
#define TEMPERATURE_SETPOINT_3     25 // TEMP_SELECT_OUT_PIN wired to TEMP_SELECT_IN1_PIN+TEMP_SELECT_IN2_PIN
#define UPDATE_INTERVAL_MSEC      200
#define STEADY_INPUT_STATE_MSEC  1000UL
#define BOOT_DELAY_SECONDS         10 // Must be at least 2 seconds because of watchdog disabling during startup
#define REBOOT_INTERVAL_MSEC     700000000UL // A little more than a week

HeatpumpIR *pHeatpumpIR = new PanasonicNKEHeatpumpIR(); // NKE model has 8/10 degrees maintenance with max. fan speed //PanasonicDKEHeatpumpIR();

IRSenderPWM irSender(IR_SENDER_PIN);  // IR led on Arduino, using Arduino PWM

unsigned long lastStateChangeTick = 1;

int heatInputState = -1;
int frostInputState = -1;
int updateInputState = -1;

void setup()
{
  wdt_disable();  /* Disable the watchdog and wait for more than 2 seconds so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */

  Serial.begin(9600);
  Serial.println(F("setup()"));

  pinMode(HEAT_INPUT_PIN1,          INPUT_PULLUP);
  pinMode(HEAT_INPUT_PIN2,          INPUT_PULLUP);
  pinMode(HEAT_INPUT_PIN3,          INPUT_PULLUP);
  pinMode(FROST_INPUT_PIN,          INPUT_PULLUP);
  pinMode(FORCE_UPDATE_INPUT_PIN,   INPUT_PULLUP);
  pinMode(TEMP_SELECT_OUT_PIN,      OUTPUT);
  pinMode(TEMP_SELECT_IN1_PIN,      INPUT_PULLUP);
  pinMode(TEMP_SELECT_IN2_PIN,      INPUT_PULLUP);
  pinMode(LED_OUTPUT_PIN,           OUTPUT);        
  digitalWrite(LED_OUTPUT_PIN,      LOW);
  digitalWrite(TEMP_SELECT_OUT_PIN, LOW);

  // Because the heat pump may be booting up at the same time,
  // delay the inital IR signalling so allow it to get ready.
  for (int i = 0; i < BOOT_DELAY_SECONDS; ++i)
  {
    digitalWrite(LED_OUTPUT_PIN, HIGH);
    delay(500);
    digitalWrite(LED_OUTPUT_PIN, LOW);
    delay(500);
  }

  wdt_enable(WDTO_8S);  /* Enable the watchdog with a timeout of 8 seconds */
}

void loop()
{
  wdt_reset(); /* Reset the watchdog */

  delay(UPDATE_INTERVAL_MSEC);

  // Time to reboot at reqular interval?
  if (millis() > REBOOT_INTERVAL_MSEC)
  {
    // Let the watchdog do the rebooting
    while (true);
  }
  
  if (checkUpdatedInputs())
    updateIR();
}

bool checkUpdatedInputs()
{
  // Heat input considered activated (low) if at least one of the heat inputs is low
  int heatInputVal = ((digitalRead(HEAT_INPUT_PIN1) == LOW) || (digitalRead(HEAT_INPUT_PIN2) == LOW) || (digitalRead(HEAT_INPUT_PIN3) == LOW)) ? LOW : HIGH;

  int frostInputVal = digitalRead(FROST_INPUT_PIN);
  int updateInputVal = digitalRead(FORCE_UPDATE_INPUT_PIN);

  // Just changing input state?
  if ((heatInputVal != heatInputState) || (frostInputVal != frostInputState) || (updateInputVal != updateInputState))
  {
    lastStateChangeTick = millis();

    digitalWrite(LED_OUTPUT_PIN, HIGH);

    // Don't use the tick value 0 - reserved
    if (lastStateChangeTick == 0)
      lastStateChangeTick = 1;

    heatInputState = heatInputVal;
    frostInputState = frostInputVal;
    updateInputState = updateInputVal;

    // Not steady yet
  }
  else // Unchanged input
  {
    // Still waiting for steadiness
    if (lastStateChangeTick != 0)
    {
      if (millis() - lastStateChangeTick >= STEADY_INPUT_STATE_MSEC)
      {
        lastStateChangeTick = 0;
        return true;
      }
    }
  }

  return false;
}

void updateIR()
{
  Serial.println(F("updateIR()"));

  byte power = (heatInputState == LOW) || (frostInputState == LOW) ? POWER_ON : POWER_OFF;
  byte temp  = (heatInputState == LOW) ? getTemperatureSetpoint() : ((frostInputState == LOW) ? 8 : 0);

  Serial.print(F("Power: ")); Serial.println(power);
  Serial.print(F("Temp: ")); Serial.println(temp);

  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);
  delay(1000);
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);
  delay(1000);
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);

  digitalWrite(LED_OUTPUT_PIN, LOW);
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
