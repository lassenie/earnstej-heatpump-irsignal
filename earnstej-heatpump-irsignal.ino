// earnstej-heatpump-irsignal
// Copyright (c) 2020-2024 Lasse Nielsen
// Controlling a heat pump via IR (e.g. TSAL6400 LED). 

#include <avr/wdt.h>
#include <PanasonicHeatpumpIR.h> // HeatPumpIR library by Toni Arte (https://github.com/ToniA/arduino-heatpumpir) - last tested with version 1.0.15

#define IR_SENDER_PIN            3 // PWM pin
#define EXT_LED_OUTPUT_PIN       4
#define VERTICAL_DIR_INPUT_PIN   6
#define HEAT_TOGGLE_IN_PIN       7
#define INT_LED_OUTPUT_PIN      13

#define TEMPERATURE_SETPOINT              23
#define UPDATE_INTERVAL_MSEC            20UL
#define SERIAL_UPDATE_INTERVAL_MSEC  10000UL
#define IR_SIGNAL_PAUSE_BLINKS             3
#define STEADY_INPUT_STATE_MSEC        100UL // Must be long enough to handle debounching
#define BOOT_DELAY_SECONDS                10 // Must be at least 2 seconds because of watchdog disabling during startup

#define HEAT_TIME_MILLIS   (24UL * 60UL * 60UL * 1000UL) // 24 hours - enough for a one-day event

HeatpumpIR *pHeatpumpIR = new PanasonicNKEHeatpumpIR(); // NKE model has 8/10 degrees maintenance with max. fan speed //PanasonicDKEHeatpumpIR();

IRSenderPWM irSender(IR_SENDER_PIN);  // IR led on Arduino, using Arduino PWM

unsigned long lastStateChangeMillis = 0;
unsigned long lastSerialUpdateMillis = 0;

bool togglingHeatMode = false;

int heatInputState        = -1; // Will update input state at startup
int verticalDirInputState = -1; // Will update input state at startup

unsigned long heatOffMillis = 0; // Heat off

void setup()
{
  wdt_disable();  /* Disable the watchdog and wait for more than 2 seconds so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */

  Serial.begin(9600);
  Serial.println();
  Serial.println(F("-------------------------------"));
  Serial.println(F("setup()"));

  pinMode(HEAT_TOGGLE_IN_PIN,      INPUT_PULLUP);
  pinMode(VERTICAL_DIR_INPUT_PIN,   INPUT_PULLUP);
  pinMode(INT_LED_OUTPUT_PIN,       OUTPUT);        
  pinMode(EXT_LED_OUTPUT_PIN,       OUTPUT);        
  digitalWrite(INT_LED_OUTPUT_PIN,  LOW);
  digitalWrite(EXT_LED_OUTPUT_PIN,  LOW);

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

  if (checkUpdatedInputs())
  {
    toggleHeatModeIfRelevant();
    updateIR();
  }
  else if (isHeatTurnOffTime())
  {
    updateIR();
  }
  else
  {
    updateSerial();
  }
}

void updateSerial()
{
  if (!isHeating())
    return;

  unsigned long nowMillis = millis();

  if (nowMillis - lastSerialUpdateMillis > SERIAL_UPDATE_INTERVAL_MSEC)
  {
    lastSerialUpdateMillis = nowMillis;

    unsigned long heatSecondsLeft = getHeatMillisLeft() / 1000UL;
   
    Serial.print(F("Heat time left: "));

    if (heatSecondsLeft > 60UL * 60UL) // More than an hour left?
    {
      Serial.print((int)(heatSecondsLeft / (60UL * 60UL)));
      Serial.println(F(" hour(s)"));
    }
    else if (heatSecondsLeft > 60UL) // More than a minute left?
    {
      Serial.print((int)(heatSecondsLeft / 60UL));
      Serial.println(F(" minute(s)"));
    }
    else
    {
      Serial.print((int)(heatSecondsLeft));
      Serial.println(F(" second(s)"));
    }
  }
}

bool checkUpdatedInputs()
{
  // Heat input considered activated (low) if at least one of the heat inputs is low
  int heatInputVal = digitalRead(HEAT_TOGGLE_IN_PIN);

  int verticalDirInputVal = digitalRead(VERTICAL_DIR_INPUT_PIN);

  // Just changing input state?
  if ((heatInputVal != heatInputState) || (verticalDirInputVal != verticalDirInputState))
  {
    Serial.println(F("checkUpdatedInputs(): Changing state"));

    lastStateChangeMillis = millis();

    digitalWrite(INT_LED_OUTPUT_PIN, HIGH);

    // Don't use the tick value 0 - reserved
    if (lastStateChangeMillis == 0)
      lastStateChangeMillis = 1;

    // Seeing a change on the heat input?
    if (heatInputVal != heatInputState)
    {
      // High-to-low, i.e. user pressing the button?
      // If low-to-high we either see end of button press or bounching button
      if (heatInputVal == LOW)
        togglingHeatMode = true;
    }

    heatInputState = heatInputVal;
    verticalDirInputState = verticalDirInputVal;

    // Not steady yet
  }
  else // Unchanged input
  {
    // Still waiting for steadiness
    if (lastStateChangeMillis != 0)
    {
      // Only consider steady if heat button released
      if ((heatInputVal != LOW) && millis() - lastStateChangeMillis >= STEADY_INPUT_STATE_MSEC)
      {
        Serial.println(F("checkUpdatedInputs(): Steady"));

        lastStateChangeMillis = 0;
        return true;
      }
    }
  }

  return false;
}

void toggleHeatModeIfRelevant()
{
    // Not toggling heat mode?
    if (!togglingHeatMode)
      return;

    Serial.println(F("toggleHeatModeIfRelevant(): Heat mode toggle"));

    // Toggle heating interval 1
    heatOffMillis = isHeating() ? 0 : millis() + HEAT_TIME_MILLIS;

    togglingHeatMode = false;
}

void updateIR()
{
  Serial.println(F("updateIR()"));

  byte power = isHeating() ? POWER_ON : POWER_OFF;
  byte temp  = isHeating() ? TEMPERATURE_SETPOINT : 0;
  byte vertDir = (verticalDirInputState == LOW) ? VDIR_DOWN : VDIR_UP;

  Serial.print(F("Power: ")); Serial.println(power);
  Serial.print(F("Temp: ")); Serial.println(temp);
  Serial.print(F("VertDir: ")); Serial.print(vertDir); Serial.println(getVertDirText(vertDir));

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

    wdt_reset(); /* Reset the watchdog */
    delay(120);
  }  
}

bool isHeatTurnOffTime()
{
  if (isHeating())
  {
    if (getHeatMillisLeft() == 0)
    {
      heatOffMillis = 0; // Turn off heating
      return true;
    }
  }

  return false;
}

unsigned long getHeatMillisLeft()
{
  unsigned long heatMillisLeft = heatOffMillis - millis();

  // Overflow, i.e. no heating time left
  if (heatMillisLeft > HEAT_TIME_MILLIS)
  {
    heatMillisLeft = 0;
  }

  return heatMillisLeft;
}

bool isHeating()
{
  // Heating?
  return (heatOffMillis != 0);
}

String getVertDirText(byte vertDir)
{
  switch (vertDir)
  {
    case VDIR_UP: return " (up)";
    case VDIR_DOWN: return " (down)";
    default: return " (?)";
  }
}