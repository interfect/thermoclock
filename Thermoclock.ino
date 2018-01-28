
// Thermostat control

#include <LiquidCrystal.h>

// Use Time library from https://github.com/PaulStoffregen/Time
#include <Time.h>
#include <TimeLib.h>

// We control the thermostat and clock with a
// big 2-button state machine.
// A lot of our states have slow and fast variants for
// push vs. hold on the advance button.
typedef enum State {
  // Start state, or just operating.
  STATE_START,
  // Set thermostat
  STATE_SET_TARGET,
  // Set hour
  STATE_SET_HOUR,
  //STATE_SET_HOUR_FAST,
  // Set minute
  STATE_SET_MINUTE,
  //STATE_SET_MINUTE_FAST,
  // Set second
  STATE_SET_SECOND
  //STATE_SET_SECOND_FAST
 };

State currentState = STATE_START;

// Use a relay as a switch.
// Low is off, high is on.
const int switchControl = 13;

// Use a 210k/whatever thermistor I found voltage divider
const int thermIn = A0;
// Higher heat = less thermistor resistance = more positive
// Reasonable values are ~600-755

// We have a configurable temperature to maintain
int targetTempF = 65;
// And a range for it.
const int targetTempMin = 60;
const int targetTempMax = 80;
// The temperature sensor is noisy and may flick up or down
// before the temperature really gets there.
// So we have some hysteresis; we turn on this far below the target
// or lower.
const int tempToleranceDown = 2;
// And we turn off this far above it or higher.
const int tempToleranceUp = 2;

// And we have a fixed schedule during which to operate
const int startHour = 8;
const int endHour = 23;

// Set up the LCD screen
const int lcdReset = 9;
const int lcdEnable = 8;
const int lcdData[] = {7, 6, 5, 4};
LiquidCrystal lcd(lcdReset, lcdEnable,
  lcdData[0], lcdData[1], lcdData[2], lcdData[3]);

// We also have two buttons
const int buttons[] = {2, 3};

// Function to read button state. Pressed = 1, not pressed = 0
int readButton(int buttonPin) {
  return !digitalRead(buttonPin);
}

// And we watch the buttons with a debouncer thing.
// We track the button states 1 tick ago and on this tick.
// We track ticks with millis.
int buttonLastState[] = {0, 0};
int buttonNewState[] = {0, 0};

// This should be called every loop to watch the buttons
void updateButtonStates() {
  buttonLastState[0] = buttonNewState[0];
  buttonLastState[1] = buttonNewState[1];
  
  buttonNewState[0] = readButton(buttons[0]);
  buttonNewState[1] = readButton(buttons[1]);
}

// Now we have functions to detect events.
// This is true on the loop that the button was pressed on
int buttonDown(int buttonNum) {
  return buttonNewState[buttonNum] && !buttonLastState[buttonNum];
}

// We track the heater switch state for hysteresis.
int switchState = 0;

// Function to set switch to state. 1 = on, 0 = off
void setSwitch(int state) {
  digitalWrite(switchControl, state ? HIGH : LOW);
  switchState = state;
}

// Get whether the switch was on or off.
int getSwitchState() {
  return switchState;
}

// Get the current temperature in degrees Arduino.
// Probably between 600 and 755, higher is hotter
int getRawTemp() {
    return analogRead(thermIn);
}

// Convert a raw temp to degrees F according to the conversion
// formula based on linear regression. Then fudge based on the
// calibrating thermometer being very wrong.
int rawToF(int raw) {
  return round(0.1674 * raw - 45.19 - 3);
}

// We have some schedule functions

// This returns if we should be trying to maintain
// target temp at the given time
int shouldBeArmed(time_t t) {
  return hour(t) >= startHour && hour(t) < endHour;
}

// This returns if it is too cold and we need to turn on
// the heat.
// It takes a raw sensor reading, not a temp in f.
int isTooCold(int rawTemp) {
  return rawToF(rawTemp) <= targetTempF - tempTolerance;
}

// This returns if it is too hot and we need to turn off
// the heat.
// It takes a raw sensor reading, not a temp in f.
int isTooHot(int rawTemp) {
  return rawToF(rawTemp) >= targetTempF + tempTolerance;
}

// Now we have some lcd printing functions

// Print padded with 0 to 2 digits
void printPad2(int val) {
  if (val < 10) {
    lcd.print("0");
  }
  lcd.print(val);
}

// When things flash, how long should they flash for?
const unsigned long flashPeriodMs = 200;

// Print padded with 0 to 2 digits, optionally flashing
void printPad2Flash(int val, int shouldFlash) {
  if (shouldFlash &&
    millis() % (2 * flashPeriodMs) < flashPeriodMs) {
      
    // Display the other thing
    lcd.print("  ");
  } else {
    // Display the actual value
    printPad2(val);
  }
}

// Print a 7-character string, flashing
void printString7Flash(const char* string, int shouldFlash) {
  if (shouldFlash &&
    millis() % (2 * flashPeriodMs) < flashPeriodMs) {
      
    // Display the other thing
    lcd.print("       ");
  } else {
    // Display the actual value
    lcd.print(string);
  }
}

void printTime(time_t t) {
  printPad2Flash(hour(t), currentState == STATE_SET_HOUR);
  lcd.print(":");
  printPad2Flash(minute(t), currentState == STATE_SET_MINUTE);
  lcd.print(":");
  printPad2Flash(second(t), currentState == STATE_SET_SECOND);
}



void setup() {
  // Start serial
  Serial.begin(9600);
  
  // Set up switch
  pinMode(switchControl, OUTPUT);
  // Set up pull-up for buttons
  pinMode(buttons[0], INPUT_PULLUP);
  pinMode(buttons[1], INPUT_PULLUP);
  // Start out with the switch off
  setSwitch(0);

  // Set up LCD
  lcd.begin(16, 2);
}

void loop() {
  // Check buttons
  updateButtonStates();
  
  // Check temperature
  int rawTemp = getRawTemp();
  
  // Show temperature
  lcd.setCursor(0, 0);
  lcd.print(rawTemp);
  lcd.print(" = ");
  lcd.print(rawToF(rawTemp));
  lcd.print((char)223);
  lcd.print("F");
  lcd.print("/");
  printPad2Flash(targetTempF, currentState == STATE_SET_TARGET);
  lcd.print((char)223);
  lcd.print("F");
  
  // Show time
  lcd.setCursor(0, 1);
  time_t t = now();
  printTime(t);
  lcd.print(" ");
  
  // Decide whether to be armed or not
  int armed = shouldBeArmed(t);
  // And whether the temp is out of bounds
  int cold = isTooCold(rawTemp);
  int hot = isTooHot(rawTemp);
  // And whether we are heating
  int heating = getSwitchState();
 
  
  if (heating) {
    printString7Flash("HEATING", heating);
  } else if(cold && armed) {
    // Probably in the menu
    printString7Flash("HEAT   ", 0);
  } else if (hot) {
    printString7Flash("TOO HOT", 0);
  } else if (armed) {
    // We know it's not cold
    printString7Flash("TEMP OK", 0);
  } else if (cold) {
    // We know we're not armed
    printString7Flash("COLD   ", 0);
  } else {
    printString7Flash("STANDBY", 0);
  }
  
  // Handle input
  if (buttonDown(0)) {
    // Left button was pressed.
    // Switch major states.
    switch (currentState) {
      case STATE_START:
        currentState = STATE_SET_TARGET;
        break;
      case STATE_SET_TARGET:
        currentState = STATE_SET_HOUR;
        break;
      case STATE_SET_HOUR:
        currentState = STATE_SET_MINUTE;
        break;
      case STATE_SET_MINUTE:
        currentState = STATE_SET_SECOND;
        break;
      case STATE_SET_SECOND:
        currentState = STATE_START;
        break;
    }
  }
  
  if (buttonDown(1)) {
    // Right button was pressed.
    // Do actual changes
    switch (currentState) {
      case STATE_START:
        // TODO: do something cool?
        break;
      case STATE_SET_TARGET:
        // Adjust set point
        targetTempF++;
        if (targetTempF > targetTempMax) {
          targetTempF = targetTempMin;
        }
        break;
      case STATE_SET_HOUR:
        // Add an hour
        adjustTime(60 * 60);
        break;
      case STATE_SET_MINUTE:
        // Add a minute, but roll over
        if (minute(t) == 59) {
          adjustTime(-59 * 60);
        } else {
          adjustTime(60);
        }
        break;
      case STATE_SET_SECOND:
        // Add a second
        if (second(t) == 59) {
          adjustTime(-59);
        } else {
          adjustTime(1);
        }
        break;
    }
  }
  
  // Now do the actual heater code
  // Hysteresis is important
  
  if (hot || !armed || currentState != STATE_START) {
    // We are not supposed to be running! Turn off right now!
    // Note that we don't want to run when someone is adjusting us.
    setSwitch(0);
  } else if (currentState == STATE_START && armed && cold) {
    // Turn on the heat!
    setSwitch(1);
  } else if(currentState == STATE_START && armed && !hot) {
    // Keep the same state
    setSwitch(heating);
  }
  
  // We don't actually need to look at the heating flag
  // The heater will stay on until we turn it off.
  
}
