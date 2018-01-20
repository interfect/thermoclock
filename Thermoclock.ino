
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

// Use a PNP transistor as a switch.
// Low is on, high is off.
const int switchControl = 9;

// Use a 210k/whatever thermistor I found voltage divider
const int thermIn = A0;
// Higher heat = less thermistor resistance = more positive
// We want the temp to stay in this range
const int thermMin = 600;
const int thermMax = 755;

// We have calibration data that can be changed later
// Corresponding entries are in raw thermistor readings
// and in degrees F.
int calPointsRaw[] = {672, 724};
int calPointsF[] = {65, 99};

// We have a configurable temperature to maintain
int targetTempF = 65;
// And a range for it.
const int targetTempMin = 60;
const int targetTempMax = 80;

// And we have a fixed schedule during which to operate
const int startHour = 8;
const int endHour = 23;

// Set up the LCD screen
const int lcdReset = 12;
const int lcdEnable = 11;
const int lcdData[] = {5, 4, 3, 2};
LiquidCrystal lcd(lcdReset, lcdEnable,
  lcdData[0], lcdData[1], lcdData[2], lcdData[3]);

// We also have two buttons
const int buttons[] = {6, 7};

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

// Function to set switch to state. 1 = on, 0 = off
void setSwitch(int state) {
  digitalWrite(switchControl, state ? LOW : HIGH);
}

// Get the current temperature in degrees Arduino.
// Probably between 600 and 755, higher is hotter
int getRawTemp() {
    return analogRead(thermIn);
}

// Convert a raw temp to degrees F according to the conversion data
int rawToF(int raw) {
  return map(raw, calPointsRaw[0], calPointsRaw[1],
    calPointsF[0], calPointsF[1]);
}

// We have some schedule functions

// This returns if we should be trying to maintain
// target temp at the given time
int shouldBeArmed(time_t t) {
  return hour(t) >= startHour && hour(t) < endHour;
}

// This returns if it is too cold.
// It takes a raw sensor reading, not a temp in f.
int isTooCold(int rawTemp) {
  return rawToF(rawTemp) < targetTempF;
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

// Print a 6-character string, flashing
void printString6Flash(const char* string, int shouldFlash) {
  if (shouldFlash &&
    millis() % (2 * flashPeriodMs) < flashPeriodMs) {
      
    // Display the other thing
    lcd.print("      ");
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
  int cold = isTooCold(rawTemp);
  
  if (armed) {
    if (cold) {
      printString6Flash("HEAT  ", currentState == STATE_START);
    } else {
      lcd.print("OK    ");
    }
  } else {
    lcd.print("DISARM");
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
  // Hysteresis is for wimps.
  if (currentState == STATE_START && armed && cold) {
    // Turn on the heat!
    setSwitch(1);
  } else {
    // Turn it off
    setSwitch(0);
  }
  
}
