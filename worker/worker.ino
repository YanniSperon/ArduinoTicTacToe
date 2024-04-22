/*
Names: Yanni Speron, Ele Berri, Maaz Iqbal
NetIDs: yspero2@uic.edu, eberr6@uic.edu, miqba4@uic.edu
Project Name: Tic-Tac-Toe-Two
Abstract: Two players play a classic game of tic-tac-toe without any setup or clean up required. Players press buttons to move their cursor to the spot they want, then press the select button. The central game Arduino accepts the button selection and determines the game state. Upon the game ending, the Winner (or Draw) is printed onto the LCD.
*/

// Include Wire.h for I2C communication
#include <Wire.h>
 
// Define I2C addresses to setup communication
#define SLAVE_START_ADDR 8
// When uploading to Arduino, set this to either 1 or 2
#define SLAVE_OFFSET 1
#define SLAVE_FINAL_ADDR (SLAVE_START_ADDR + SLAVE_OFFSET)

// Define where the LEDs start
#define LED_START_PIN 5
#define CONTROL_PIN 2
 
// Define Slave answer size
#define LED_PINS 9
#define DATA_SIZE (LED_PINS + 1)
 
// Initialization code
void setup() {
 
  // Initialize I2C communications as Slave
  Wire.begin(SLAVE_FINAL_ADDR);
  
  // Function to run when data requested from master
  Wire.onRequest(requestEvent); 
  
  // Function to run when data received from master
  Wire.onReceive(receiveEvent);
  
  // Setup Serial Monitor 
  Serial.begin(9600);
  Serial.println("I2C Slave Demonstration");

  // Setup all the outpout pins
  pinMode(CONTROL_PIN, OUTPUT);
  for (int i = 0; i < LED_PINS; ++i) {
    pinMode(LED_START_PIN + i, OUTPUT);
  }
}

// Triggered when the controller transmits data to this worker
void receiveEvent() {
  // Read the transmission
  byte receivedTransmission[DATA_SIZE + 1];
  for (int i = 0; i < DATA_SIZE + 1; ++i)
  {
    receivedTransmission[i] = 0;
  }
  int index = 0;
  // Read while data received
  while (0 < Wire.available()) {
    if (index < 10) {
      // Any bytes over the first 10 just ignore
      byte in = Wire.read();
      receivedTransmission[index] = in;
      index++;
    }
    else {
      byte x = Wire.read();
    }
  }
  
  // Output high to an LED if the byte at that location contains its SLAVE_OFFSET bit (1 or 2)
  for (int i = 0; i < LED_PINS; ++i) {
    if (receivedTransmission[i] & SLAVE_OFFSET) {
      digitalWrite(LED_START_PIN + i, HIGH);
    }
    else {
      digitalWrite(LED_START_PIN + i, LOW);
    }
  }

  // Unused, originally supposed to be for deciding to use controller 1 or 2
  digitalWrite(CONTROL_PIN, HIGH);

  // Output the state to the Serial monitor for debugging
  Serial.println("Receive event on slave " + SLAVE_FINAL_ADDR);
  Serial.print("    Read: [");
  for (int i = 0; i < DATA_SIZE; ++i) {
    if (receivedTransmission[i] == SLAVE_OFFSET) {
      Serial.print("ON, ");
    }
    else {
      Serial.print("OFF, ");
    }
  }
  Serial.println("]");
}

// Triggered when the worker is replying to a request, not currently used anywhere in our code
void requestEvent() {
  // Setup byte variable in the correct size
  byte response[DATA_SIZE];
  
  // Format answer as array
  for (byte i=0;i<DATA_SIZE;i++) {
    response[i] = 0;
  }
  
  // Send response back to Master
  Wire.write(response,sizeof(response));
  
  // Print to Serial Monitor
  Serial.println("Request event from slave " + SLAVE_FINAL_ADDR);
}

// Looping code, we do not need to do anything here
void loop() {
}