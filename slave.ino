#include <Wire.h>
 
#define SLAVE_START_ADDR 8
// Define Slave I2C Address
// Change the SLAVE_OFFSET when uploading code to each slave Arduino to either be 1 or 2
#define SLAVE_OFFSET 2
#define SLAVE_FINAL_ADDR (SLAVE_START_ADDR + SLAVE_OFFSET)

#define LED_START_PIN 5
#define CONTROL_PIN 2
 
// Define Slave answer size
#define LED_PINS 9
#define DATA_SIZE (LED_PINS + 1)
 
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

  pinMode(CONTROL_PIN, OUTPUT);
  for (int i = 0; i < LED_PINS; ++i) {
    pinMode(LED_START_PIN + i, OUTPUT);
  }
}
 
void receiveEvent() {
  byte receivedTransmission[DATA_SIZE + 1];
  for (int i = 0; i < DATA_SIZE + 1; ++i)
  {
    receivedTransmission[i] = 0;
  }
  int index = 0;
  // Read while data received
  while (0 < Wire.available()) {
    if (index < 10) {
      byte in = Wire.read();
      receivedTransmission[index] = in;
      index++;
    }
    else {
      byte x = Wire.read();
    }
  }
  
  for (int i = 0; i < LED_PINS; ++i) {
    if (receivedTransmission[i] & SLAVE_OFFSET) {
      digitalWrite(LED_START_PIN + i, HIGH);
    }
    else {
      digitalWrite(LED_START_PIN + i, LOW);
    }
  }

  //if (receivedTransmission[LED_PINS] & SLAVE_OFFSET) {
    digitalWrite(CONTROL_PIN, HIGH);
  //}
  //else
  //{
  //  digitalWrite(CONTROL_PIN, LOW);
  //}

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
 
void loop() {
}
