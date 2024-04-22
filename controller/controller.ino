#include <Wire.h>
// include the LCD library code
#include <LiquidCrystal.h>
 
// Define Slave I2C Address
#define SLAVE_START_ADDR 8

#define SLAVE_1_OFFSET 1
#define SLAVE_2_OFFSET 2

#define SLAVE_1_FINAL_ADDR SLAVE_START_ADDR + SLAVE_1_OFFSET
#define SLAVE_2_FINAL_ADDR SLAVE_START_ADDR + SLAVE_2_OFFSET
 
#define NUM_LEDS 9
#define DATA_SIZE NUM_LEDS + 1

#define NUM_BUTTONS 5
// Button pins start here and count down in the order specified with the
// button debounce definitions
#define BUTTON_START_PIN 13

byte boardState[DATA_SIZE];

int cursorHorizontalPosition;
int cursorVerticalPosition;

int p1Wins;
int p2Wins;
int ties;

// 0 is select, 1 is up, 2 is down, 3 is left, 4 is right
int buttonState[NUM_BUTTONS];
int lastButtonState[NUM_BUTTONS];

// Timer for transmitting IC2
unsigned long transmitTimerStart;
unsigned long transmitTimerCurr;
const unsigned long transmitTimerDuration = 25;

// Timer for blinking cursor
unsigned long blinkingTimerStart;
unsigned long blinkingTimerCurr;
bool isCursorOffFromBlink;
const unsigned long blinkingTimerDuration = 250;
const unsigned long blinkingTimerDurationAlreadySelected = 500;

// Timer for debouncing
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long debounceDelay = 50;

bool shouldReset = false;
unsigned long lastResetTimerStart;
unsigned long lastResetTimerCurr;
const unsigned long resetTimerDuration = 1000;

char LCDLine1[16];
char LCDLine2[16];

// Create LCD
const int rs = 8, en = 7, d4 = 6, d5 = 5, d6 = 4, d7 = 3;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// 0 is unmarked, 1 is player 1 marked, 2 is player 2 marked
byte getPositionState(int verticalPos, int horizontalPos)
{
  return boardState[horizontalPos + (verticalPos * 3)];
}

bool doesPlayerHoldPosition(byte player, int verticalPos, int horizontalPos)
{
  return getPositionState(verticalPos, horizontalPos) == player;
}

bool checkHorizontalWin(byte player)
{
  for (int i = 0; i < 3; ++i)
  {
    if (doesPlayerHoldPosition(player, i, 0) && doesPlayerHoldPosition(player, i, 1) && doesPlayerHoldPosition(player, i, 2))
    {
      Serial.println("Player wins horizontally");
      return true;
    }
  }
  return false;
}

bool checkVerticalWin(byte player)
{
  for (int i = 0; i < 3; ++i)
  {
    if (doesPlayerHoldPosition(player, 0, i) && doesPlayerHoldPosition(player, 1, i) && doesPlayerHoldPosition(player, 2, i))
    {
      Serial.println("Player wins vertically");
      return true;
    }
  }
  return false;
}

bool checkDiagonalWin(byte player)
{
  if ((doesPlayerHoldPosition(player, 0, 0) && doesPlayerHoldPosition(player, 1, 1) && doesPlayerHoldPosition(player, 2, 2)) || 
    (doesPlayerHoldPosition(player, 0, 2) && doesPlayerHoldPosition(player, 1, 1) && doesPlayerHoldPosition(player, 2, 0))) {
      Serial.println("Player wins diagonally");
      return true;
  }
  return false;
}

bool isAnyOpenSpot()
{
  for (int i = 0; i < 9; ++i)
  {
    if (boardState[i] == 0)
    {
      return true;
    }
  }
  return false;
}

// Returns -1 if tie, 0 if game should continue, 1 if player 1 won, 2 if player 2 won
int findWinner()
{
  for (byte i = 1; i <= 2; ++i)
  {
    if (checkHorizontalWin(i) || checkVerticalWin(i) || checkDiagonalWin(i))
    {
      return i;
    }
  }

  if (isAnyOpenSpot())
  {
    return 0;
  }
  else
  {
    Serial.println("Player drew");
    return -1;
  }
}

void resetGame()
{
  for (int i = 0; i < NUM_LEDS; ++i)
  {
    boardState[i] = 0;
  }
  boardState[NUM_LEDS] = SLAVE_1_OFFSET;
  shouldReset = false;
  Serial.print("New score p1: ");
  Serial.print(p1Wins);
  Serial.print(" p2: ");
  Serial.print(p2Wins);
  Serial.print(" ties: ");
  Serial.println(ties);
  cursorVerticalPosition = 1;
  cursorHorizontalPosition = 1;
  snprintf(LCDLine1, 16, "Red: %d Ties: %d", p2Wins, ties);
  snprintf(LCDLine2, 16, "Blue: %d", p1Wins);
}

// Return true if successful place, false otherwise
bool attemptPlace(byte player, int verticalPos, int horizontalPos)
{
  if (getPositionState(verticalPos, horizontalPos) == 0)
  {
    boardState[horizontalPos + (verticalPos * 3)] = player;
    return true;
  }
  else
  {
    return false;
  }
}

void attemptMakeTurn()
{
  if (attemptPlace(boardState[NUM_LEDS], cursorVerticalPosition, cursorHorizontalPosition))
  {
    if (boardState[NUM_LEDS] == SLAVE_1_OFFSET)
    {
      boardState[NUM_LEDS] = SLAVE_2_OFFSET;
    }
    else
    {
      boardState[NUM_LEDS] = SLAVE_1_OFFSET;
    }

    int winner = findWinner();
    if (winner == -1)
    {
      ties++;
      if (ties >= 100)
      {
        ties = 0;
      }
    }
    else if (winner == 1)
    {
      p1Wins++;
      if (p1Wins >= 100)
      {
        p1Wins = 0;
      }
    }
    else if (winner == 2)
    {
      p2Wins++;
      if (p2Wins >= 100)
      {
        p2Wins = 0;
      }
    }

    if (winner != 0)
    {
      shouldReset = true;
      lastResetTimerStart = millis();
    }
  }
}

bool shouldBlinkFast()
{
  return boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] == boardState[NUM_LEDS];
}

void moveCursorLeft()
{
  cursorHorizontalPosition--;
  if (cursorHorizontalPosition < 0)
  {
    cursorHorizontalPosition = 2;
  }
}

void moveCursorRight()
{
  cursorHorizontalPosition++;
  if (cursorHorizontalPosition > 2)
  {
    cursorHorizontalPosition = 0;
  }
}

void moveCursorUp()
{
  cursorVerticalPosition--;
  if (cursorVerticalPosition < 0)
  {
    cursorVerticalPosition = 2;
  }
}

void moveCursorDown()
{
  cursorVerticalPosition++;
  if (cursorVerticalPosition > 2)
  {
    cursorVerticalPosition = 0;
  }
}

// 0 is select, 1 is up, 2 is down, 3 is left, 4 is right
void buttonPressed(int i)
{
  //Serial.println("Button pressed");
  //switch (i) {
  //  case 0:
  //    attemptMakeTurn();
  //    break;
  //  case 1:
  //    moveCursorUp();
  //    break;
  //  case 2:
  //    moveCursorDown();
  //    break;
  //  case 3:
  //    moveCursorLeft();
  //    break;
  //  case 4:
  //    moveCursorRight();
  //    break;
  //  default:
  //    break;
  //}

  // TEMP
  switch (i) {
    case 0:
      moveCursorRight();
      break;
    case 1:
      moveCursorLeft();
      break;
    case 2:
      attemptMakeTurn();
      break;
    case 3:
      moveCursorDown();
      break;
    case 4:
      moveCursorUp();
      break;
    default:
      break;
  }
}

void dispatchDataToSlaves()
{
  byte prevCursorState = boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition];
  if (isCursorOffFromBlink && !shouldReset)
  {
    if (boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] == boardState[NUM_LEDS])
    {
      boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] = 0;
    }
  }
  else if (!shouldReset)
  {
    boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] |= boardState[NUM_LEDS];
  }
  Wire.beginTransmission(SLAVE_1_FINAL_ADDR);
  Wire.write(boardState, sizeof(boardState));
  Wire.endTransmission();
  Wire.beginTransmission(SLAVE_2_FINAL_ADDR);
  Wire.write(boardState, sizeof(boardState));
  Wire.endTransmission();

  boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] = prevCursorState;
}

void setup() {
  // Initialize I2C communications as Slave
  Wire.begin(SLAVE_1_FINAL_ADDR);
  Wire.begin(SLAVE_2_FINAL_ADDR);
  
  // Setup Serial Monitor 
  Serial.begin(9600);
  Serial.println("Master connected");

  // Setup the LCD's number of columns and rows
  lcd.begin(16, 2);
  LCDLine1[0] = '\0';
  LCDLine2[0] = '\0';

  // Setup tic-tac-toe
  cursorHorizontalPosition = 1;
  cursorVerticalPosition = 1;
  
  // Setup stats
  p1Wins = 0;
  p2Wins = 0;
  ties = 0;

  // Setup timers
  transmitTimerStart = millis();
  transmitTimerCurr = millis();

  // Setup cursor blink
  blinkingTimerStart = millis();
  blinkingTimerCurr = millis();
  isCursorOffFromBlink = false;

  // Setup initial LED values
  resetGame();
  lastResetTimerStart = millis();
  shouldReset = false;

  for (int i = 0; i < NUM_BUTTONS; ++i)
  {
    buttonState[i] = LOW;
    lastButtonState[i] = LOW;
    lastDebounceTime[i] = 0;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing");
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; ++i)
  {
    int reading = digitalRead(BUTTON_START_PIN - i);
    if (reading == HIGH)
    {
      Serial.print("Button reading HIGH on player ");
      Serial.print(boardState[NUM_LEDS]);
      Serial.print(" on button ");
      Serial.println(i);
    }

    if (reading != lastButtonState[i]) {
      // reset the debouncing timer
      Serial.println("Resetting debounce timer");
      lastDebounceTime[i] = millis();
    }

    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (reading != buttonState[i]) {
        buttonState[i] = reading;
        Serial.println("Reading has changed");
        if (buttonState[i] == HIGH) {
          Serial.println("Button pressed");
          buttonPressed(i);
        }
      }
    }
    lastButtonState[i] = reading;
  }

  transmitTimerCurr = millis();
  if ((transmitTimerCurr - transmitTimerStart) >= transmitTimerDuration)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(LCDLine1);
    lcd.setCursor(0, 1);
    lcd.print(LCDLine2);
    dispatchDataToSlaves();
    transmitTimerStart = transmitTimerCurr;
  }

  blinkingTimerCurr = millis();
  bool shouldBeBlinkingFast = shouldBlinkFast();
  if ((((blinkingTimerCurr - blinkingTimerStart) >= blinkingTimerDuration) && !shouldBeBlinkingFast) || (((blinkingTimerCurr - blinkingTimerStart) >= blinkingTimerDurationAlreadySelected) && shouldBeBlinkingFast))
  {
    isCursorOffFromBlink = !isCursorOffFromBlink;
    blinkingTimerStart = blinkingTimerCurr;
  }

  if (shouldReset)
  {
    lastResetTimerCurr = millis();
  
    if ((lastResetTimerCurr - lastResetTimerStart) >= resetTimerDuration)
    {
      resetGame();
    }
  }
}