#include <Wire.h>
// include the LCD library code for the score keeper
#include <LiquidCrystal.h>
 
// Define Slave I2C Address
#define SLAVE_START_ADDR 8

// Define the I2C address offset of the two controller Arduinos
#define SLAVE_1_OFFSET 1
#define SLAVE_2_OFFSET 2

// Define the final I2C address of the two controller Arduinos
#define SLAVE_1_FINAL_ADDR SLAVE_START_ADDR + SLAVE_1_OFFSET
#define SLAVE_2_FINAL_ADDR SLAVE_START_ADDR + SLAVE_2_OFFSET
 
// Define the data packet being sent between Arduinos
#define NUM_LEDS 9
#define DATA_SIZE NUM_LEDS + 1

// Defines the number of controller buttons
#define NUM_BUTTONS 5
// Button pins start here and count down in the order specified with the
// button debounce definitions
#define BUTTON_START_PIN 13

// The current board state
byte boardState[DATA_SIZE];

// The cursor location
int cursorHorizontalPosition;
int cursorVerticalPosition;

// Game stats
int p1Wins;
int p2Wins;
int ties;

// indices: 0 is right button, 1 is left button, 2 is select button, 3 is down button, 4 is up button
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

// Timers for debouncing
unsigned long lastDebounceTime[NUM_BUTTONS];
const unsigned long debounceDelay = 50;

// Game over timer
bool shouldReset = false;
unsigned long lastResetTimerStart;
unsigned long lastResetTimerCurr;
const unsigned long resetTimerDuration = 1000;

// Store the stats LCD output
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

// Returns whether a player holds a position or not
bool doesPlayerHoldPosition(byte player, int verticalPos, int horizontalPos)
{
  return getPositionState(verticalPos, horizontalPos) == player;
}

// Returns true if a player has won through the horizontal rules
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

// Returns true if a player has won through the vertical rules
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

// Returns true if a player has won through the diagonal rules
bool checkDiagonalWin(byte player)
{
  if ((doesPlayerHoldPosition(player, 0, 0) && doesPlayerHoldPosition(player, 1, 1) && doesPlayerHoldPosition(player, 2, 2)) || 
    (doesPlayerHoldPosition(player, 0, 2) && doesPlayerHoldPosition(player, 1, 1) && doesPlayerHoldPosition(player, 2, 0))) {
      Serial.println("Player wins diagonally");
      return true;
  }
  return false;
}

// Returns true if the board still has any open spots
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

// Resets a board to a starting state
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

// Attempts to make a turn given the current cursor location
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
      // Wrap around ties to prevent too large of a number
      ties++;
      if (ties >= 100)
      {
        ties = 0;
      }
    }
    else if (winner == 1)
    {
      // Wrap around wins to prevent too large of a number
      p1Wins++;
      if (p1Wins >= 100)
      {
        p1Wins = 0;
      }
    }
    else if (winner == 2)
    {
      // Wrap around wins to prevent too large of a number
      p2Wins++;
      if (p2Wins >= 100)
      {
        p2Wins = 0;
      }
    }

    if (winner != 0)
    {
      // If there is a winner
      shouldReset = true;
      lastResetTimerStart = millis();
    }
  }
}

// Returns true if the board state should blink differently (we are over one of our own already selected locations)
bool shouldBlinkFast()
{
  return boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition] == boardState[NUM_LEDS];
}

// Attempts to move the cursor left
void moveCursorLeft()
{
  cursorHorizontalPosition--;
  if (cursorHorizontalPosition < 0)
  {
    cursorHorizontalPosition = 2;
  }
}

// Attempts to move the cursor right
void moveCursorRight()
{
  cursorHorizontalPosition++;
  if (cursorHorizontalPosition > 2)
  {
    cursorHorizontalPosition = 0;
  }
}

// Attempts to move the cursor up
void moveCursorUp()
{
  cursorVerticalPosition--;
  if (cursorVerticalPosition < 0)
  {
    cursorVerticalPosition = 2;
  }
}

// Attempts to move the cursor down
void moveCursorDown()
{
  cursorVerticalPosition++;
  if (cursorVerticalPosition > 2)
  {
    cursorVerticalPosition = 0;
  }
}

// Attempts to press the button with index i
void buttonPressed(int i)
{
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

// Sends data over I2C to the workers
void dispatchDataToSlaves()
{
  byte prevCursorState = boardState[(cursorVerticalPosition * 3) + cursorHorizontalPosition];
  if (isCursorOffFromBlink && !shouldReset)
  {
    // Only blink if the game is not over, and the blink timer indicates we should
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

// Initialization code
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

  // Button default states
  for (int i = 0; i < NUM_BUTTONS; ++i)
  {
    buttonState[i] = LOW;
    lastButtonState[i] = LOW;
    lastDebounceTime[i] = 0;
  }
  
  // LCD default state
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing");
}

// Looping code
void loop() {
  // Debounce buttons
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
          // Trigger a button press
          Serial.println("Button pressed");
          buttonPressed(i);
        }
      }
    }
    lastButtonState[i] = reading;
  }

  // Transmit the data to the workers if the timer has ended
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

  // Blink the cursor if we should at either the fast rate or the slow rate depending on the situation
  blinkingTimerCurr = millis();
  bool shouldBeBlinkingFast = shouldBlinkFast();
  if ((((blinkingTimerCurr - blinkingTimerStart) >= blinkingTimerDuration) && !shouldBeBlinkingFast) || (((blinkingTimerCurr - blinkingTimerStart) >= blinkingTimerDurationAlreadySelected) && shouldBeBlinkingFast))
  {
    isCursorOffFromBlink = !isCursorOffFromBlink;
    blinkingTimerStart = blinkingTimerCurr;
  }

  // If we should reset the game, start the reset timer before ultimately resetting
  if (shouldReset)
  {
    lastResetTimerCurr = millis();
  
    if ((lastResetTimerCurr - lastResetTimerStart) >= resetTimerDuration)
    {
      resetGame();
    }
  }
}