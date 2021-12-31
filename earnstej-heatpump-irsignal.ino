// HeatPumpIRSignal
// Controlling a heat pump via IR (e.g. TSAL6400 LED). 

#include <avr/wdt.h>
#include <PanasonicHeatpumpIR.h>

#define IR_SENDER_PIN           3 // PWM pin
#define FORCE_UPDATE_INPUT_PIN  5
#define FROST_INPUT_PIN         6
#define HEAT_INPUT_PIN1         7
#define HEAT_INPUT_PIN2         8
#define HEAT_INPUT_PIN3         9
#define LED_OUTPUT_PIN          13

#define TEMPERATURE_SETPOINT       23
#define UPDATE_INTERVAL_MSEC      200
#define STEADY_INPUT_STATE_MSEC  1000UL
#define BOOT_DELAY_SECONDS         10

HeatpumpIR *pHeatpumpIR = new PanasonicNKEHeatpumpIR(); // NKE model has 8/10 degrees maintenance with max. fan speed //PanasonicDKEHeatpumpIR();

IRSenderPWM irSender(IR_SENDER_PIN);  // IR led on Arduino, using Arduino PWM

unsigned long lastStateChangeTick = 1;

int heatInputState = -1;
int frostInputState = -1;
int updateInputState = -1;

void setup()
{
  Serial.begin(9600);
  Serial.println(F("setup()"));

  pinMode(HEAT_INPUT_PIN1,        INPUT_PULLUP);
  pinMode(HEAT_INPUT_PIN2,        INPUT_PULLUP);
  pinMode(HEAT_INPUT_PIN3,        INPUT_PULLUP);
  pinMode(FROST_INPUT_PIN,        INPUT_PULLUP);
  pinMode(FORCE_UPDATE_INPUT_PIN, INPUT_PULLUP);
  pinMode(LED_OUTPUT_PIN,         OUTPUT);        
  digitalWrite(LED_OUTPUT_PIN,    LOW);

  wdt_disable();  /* Disable the watchdog and wait for more than 2 seconds so that the Arduino doesn't keep resetting infinitely in case of wrong configuration */

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
  byte temp  = (heatInputState == LOW) ? TEMPERATURE_SETPOINT : ((frostInputState == LOW) ? 8 : 0);

  Serial.print(F("Power: ")); Serial.println(power);
  Serial.print(F("Temp: ")); Serial.println(temp);

  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);
  delay(1000);
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);
  delay(1000);
  pHeatpumpIR->send(irSender, power, MODE_HEAT, FAN_AUTO, temp, VDIR_UP, HDIR_AUTO);

  digitalWrite(LED_OUTPUT_PIN, LOW);
}
