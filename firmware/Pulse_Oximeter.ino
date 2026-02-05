
//Downloading necessary libraries
#include <Arduino.h>
#include <Wire.h>              //I2C communication
#include "MAX30105.h"          //Pulse oximeter sensor
#include "spo2_algorithm.h"    //SpO2 calculations
#include "heartRate.h"         //heart rate calculations
#include <Adafruit_SSD1306.h>  //OLED display
#include <Adafruit_GFX.h>      //OLED graphics
#include <NimBLEDevice.h>      //BLE 
#include <NimBLEDevice.h>      //BLE 

//Defining pulse oximeter sensor as a class
MAX30105 particleSensor;

// these used to be locals in setup() — pull them out to globals
const byte ledBrightness = 50;
const byte sampleAverage = 1;
const byte ledMode      = 2;
const byte sampleRate   = 100;
const int  pulseWidth   = 69;
const int  adcRange     = 4096;


//Defining OLED screen parameters
#define MAX_BRIGHTNESS 255
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  //Handle reset in software for rebooting and initializng
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Buffers to hold sensor data for processing
uint32_t irBuffer[100];   //IR LED sensor data
uint32_t redBuffer[100];  //red LED sensor data

//Initializing variables
int32_t bufferLength;   //Data length
int32_t spo2;           //SPO2 value
int8_t validSPO2;       //Indicator to show if the SPO2 calculation is valid
int32_t heartRate;      //Heart rate value calcualated as per Maxim's algorithm
int8_t validHeartRate;  //Indicator to show if the heart rate calculation is valid
long lastBeat = 0;      //Time at which the last beat occurs
float beatsPerMinute;   //Stores the BPM for custom algorithm
int beatAvg = 0;        //Stores average beats per minute
int spo2Avg = 0;        //Stores average SpO2
int lastValidSpO2 = 0;  //Stores last valid SpO2 value
int lastValidBPM = 0;   //Stores last valid BPM value
unsigned long lastDisplayUpdate = 0;
unsigned long displayInterval = 5000;  //Time between oled updates 7s for now but can change.
bool pulseVisible = false;             //Heart animation symbol presence
unsigned long pulseStartTime = 0;      //Time the pulse starts for heart animation

//Defines BLE Characteristic pointers
NimBLECharacteristic* hrChar; 
NimBLECharacteristic* spo2Char; 
NimBLECharacteristic* irChar;
NimBLEServer* pServer;

//Connection flag
bool deviceConnected=false; 

//Server callback for connection management
class MyServerCallbacks : public NimBLEServerCallbacks
{
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override
  {
    deviceConnected=true;
    Serial.printf("Client address: %s\n", connInfo.getAddress().toString().c_str());
   pServer->updateConnParams(connInfo.getConnHandle(), 24, 48, 0, 180);
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override
  {
    deviceConnected=false;
    Serial.printf("Client disconnected.\n");
    NimBLEDevice::startAdvertising();
  }

  void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override
  {
    Serial.printf("MTU updated: %u for connection ID: %u\n", MTU, connInfo.getConnHandle());
  }
} serverCallbacks; 

//Handler class for characteristic actions
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks
{
  void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override
  {
    Serial.printf("%s : onRead(), value: %s\n",
      pCharacteristic->getUUID().toString().c_str(),
      pCharacteristic->getValue().c_str());
  }

  void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override
  {
    Serial.printf("%s : onWrite(), value: %s\n",
      pCharacteristic->getUUID(). toString().c_str(),
      pCharacteristic->getValue().c_str());
  }

  void onStatus(NimBLECharacteristic* pCharacteristic, int code) override
  {
    Serial.printf("Notification/Indication return code: %d, %s\n",
      code,
      NimBLEUtils::returnCodeToString(code));
  }

  void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
    std::string str = "Client ID: ";
    str            += std::to_string(connInfo.getConnHandle());
    str            += " Address: ";
    str            +=connInfo.getAddress().toString();
    if (subValue ==0)
    {
      str += "Unsubscribed to ";
    }
    else if (subValue ==1)
    {
      str +=" Subscribed to notifications for";
    }
    else if (subValue==2)
    {
      str += " Subscribed to indications for ";
    }
    else if (subValue==3)
    {
      str += " Subscribed to notifications and indications for ";
    }
    str += std::string(pCharacteristic->getUUID());

    Serial.printf("%s\n", str.c_str());
  }
}chrCallbacks;

//Handler class for descriptor actions 
class DescriptorCallbacks : public NimBLEDescriptorCallbacks
{
  void onWrite(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo) override
  {
    std::string dscVal = pDescriptor->getValue();
    Serial.printf("Descriptor written value: %s\n", dscVal.c_str());
  }

  void onRead(NimBLEDescriptor* pDescriptor, NimBLEConnInfo& connInfo)override
  {
    Serial.printf("%s Descriptor read\n", pDescriptor->getUUID().toString().c_str());
  }
} dscCallbacks;

//Defining LED parameters
#define LED_PIN 2 //GIOP pin 
const unsigned long inhaleTime = 4000; 
const unsigned long holdTime = 7000; 
const unsigned long exhaleTime = 8000; 
const unsigned long totalBreathCycle = inhaleTime + holdTime + exhaleTime; 
unsigned long breathStartTime = 0; 


void setup() {
  //Beginning I2C communication 
  Wire.begin(21, 22);

  //Initializing serial monitor communication
  Serial.begin(115200);                           //Open serial monitor at 115200 bauds
  Serial.print("Initializng Pulse Oximeter...");  //Prints to screen

  //Initializing sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST))  //If fails reads as true, initializes using I2C protocol at 400 kHz ("if sensor not found basically")
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));  //Prints to screen
    while (1)
      ;  //While true loop does not continue forward
  }

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);  //Configures sensor

  //Initializing OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))  //If OLED was not foudn
  {
    Serial.println(F("OLED not found, please check wiring/power"));  //Prints to scrreen
    while (1)
      ;  //While true loop does not continue
  }

  //OLED initial parameters
  display.clearDisplay();               //Clears display
  display.setTextSize(1);               //Text size
  display.setTextColor(SSD1306_WHITE);  //Text color

  //Initial message placement
  String message = "Initializing...";                 //Message string
  int x = (SCREEN_WIDTH - message.length() * 6) / 2;  //Width of message on screen centered
  int y = (SCREEN_HEIGHT - 8) / 2;                    //Height of message on screen centered
  display.setCursor(x, y);                            //Placement
  display.print(message);                             //Dispaly on Oled
  display.display();                                  //Display on screen
  delay(1000);                                        //Delay for system stability

  //Initializing BLE
  Serial.printf("Starting NimBLE Server\n");
  NimBLEDevice::init("Pulse_Oximeter");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(&serverCallbacks);
  NimBLEService* pulseService= pServer->createService("180D"); //Heart rate service 

  //Heart Rate Characteristic 
  hrChar = pulseService->createCharacteristic("2A37", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  hrChar->setValue("HR: ---");
  hrChar->setCallbacks(&chrCallbacks);

  //SpO2 Characteristic 
  spo2Char = pulseService->createCharacteristic("2A5F", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ);
  spo2Char->setValue("SpO2: ---");
  spo2Char->setCallbacks(&chrCallbacks);

  //IR Characteristic
  irChar = pulseService->createCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26a8", NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::READ); 
  irChar->setValue("IR: ---");
  irChar->setCallbacks(&chrCallbacks);

  //Start the service
  pulseService->start();

  //Start advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->setName("Pulse-Oximeter");
  pAdvertising->addServiceUUID(pulseService->getUUID());
  pAdvertising->enableScanResponse(true); 
  pAdvertising->start();
  
  Serial.printf("Advertising Started\n");

  //LED setup 
  // LEDC timer config
ledc_timer_config_t ledc_timer = {
  .speed_mode       = LEDC_HIGH_SPEED_MODE,
  .duty_resolution  = LEDC_TIMER_8_BIT,
  .timer_num        = LEDC_TIMER_0,
  .freq_hz          = 5000,
  .clk_cfg          = LEDC_AUTO_CLK
};
ledc_timer_config(&ledc_timer);

// LEDC channel config
ledc_channel_config_t ledc_channel = {
  .gpio_num   = LED_PIN,
  .speed_mode = LEDC_HIGH_SPEED_MODE,
  .channel    = LEDC_CHANNEL_0,
  .intr_type  = LEDC_INTR_DISABLE,
  .timer_sel  = LEDC_TIMER_0,
  .duty       = 0,
  .hpoint     = 0
};
ledc_channel_config(&ledc_channel);

}

void loop() {
  bufferLength = 100;  //buffer length of 100 stores 4 seconds of samples running at 25sps

  //read the first 100 samples and save it to buffers
  for (byte i = 0; i < bufferLength; i++)  //go through the loop for 0-100 samples and increment i at each iteration
  {
    while (particleSensor.available() == false)  //If there is no new data available
      particleSensor.check();                    //Checks the sensor for new data

    irBuffer[i] = particleSensor.getIR();    //save IR sensor data to IR Buffer
    redBuffer[i] = particleSensor.getRed();  // save red LED sensor data to red buffer

    particleSensor.nextSample();  //Move on to the next sample

    Serial.print(F("red: "));         //Print IR and red LED sensor data to serial monitor
    Serial.print(redBuffer[i], DEC);  //Dec means decimal
    Serial.print(F("\t ir: "));
    Serial.println(irBuffer[i], DEC);
  }

  //Calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  //Continuously taking samples and changing buffer values. Heart rate and SpO2 calculated every 1 second.
  while (1)  //While true
  {
    for (byte i = 25; i < 100; i++)  //for 25-100 increment i at each loop
    {
      redBuffer[i - 25] = redBuffer[i];  //Shifts last 75 sensors to the beginning of the buffer keeping 75 recent data and discarding the remaining 25
      irBuffer[i - 25] = irBuffer[i];
    }

    //Takes new sensor data before calculating heart rate
    for (byte i = 75; i < 100; i++)  //increment i 25 times
    {
      while (particleSensor.available() == false)  //If there is no new data waiting
        particleSensor.check();                    //Check sensor for new data

      redBuffer[i] = particleSensor.getRed();  //Gets new red LED data and saves to buffer
      irBuffer[i] = particleSensor.getIR();    //Gets new IR data and saves to buffer
      particleSensor.nextSample();             //move on to next sample

      Serial.print(F("red: "));  //Prints red, IR, HR and SpO2 values from maxim function to serial monitor.
      Serial.print(redBuffer[i], DEC);
      Serial.print(F("\t ir: "));
      Serial.print(irBuffer[i], DEC);
      Serial.print(F("\t HR="));
      Serial.print(heartRate, DEC);
      Serial.print(F("\t"));
      Serial.print(beatAvg, DEC);

      Serial.print(F("\t HRvalid="));
      Serial.print(validHeartRate, DEC);

      Serial.print(F("\t SPO2="));
      Serial.print(spo2, DEC);
      Serial.print(F("\t SPO2Valid="));
      Serial.println(validSPO2, DEC);

      //Save IR buffer value to IR value for real time heart calculation
      long irValue = irBuffer[i];

      //IR value threshold for finger detection
      if (irValue < 5000)  //5000 indicates no finger is present
      {
        display.clearDisplay();                             //Clears display
        String message = "Place finger";                    //Message saved in variable
        int x = (SCREEN_WIDTH - message.length() * 6) / 2;  //X positioning
        int y = (SCREEN_HEIGHT - 8) / 2;                    //Y positioning
        display.setTextSize(1);                             //Text size
        display.setCursor(x, y);                            //Positioning
        display.println(message);                           //Prints to OLED
        display.display();
        continue;  //Continue code
      }

      //Custom BPM calculation for real time data
      if (checkForBeat(irValue) == true)  //If a beat has been detected
      {
        long delta = millis() - lastBeat;        //Time between beats
        lastBeat = millis();                     //Time since program started marks last beat
        beatsPerMinute = 60 / (delta / 1000.0);  //Instantaneous BPM
        pulseVisible = true;
        pulseStartTime = millis();
        if (beatAvg == 0)  //If not enough values have been sensed it will reconfigure average beats
        {
          beatAvg = beatsPerMinute;
        } else {
          beatAvg = (beatAvg + beatsPerMinute) / 2;
        }
      }
      //Get sensor data for OLED display values
      if (beatAvg >= 40 && beatAvg <= 170)  //Will only show BPM if within this range
        lastValidBPM = beatAvg;             //Stores valid BPM in OLED variable
      if (spo2 >= 80 && spo2 <= 100)
        lastValidSpO2 = spo2;  //Stores valid SpO2 in OLED variable

      //BLE notifications
      if(deviceConnected)
      {
        hrChar->setValue((uint8_t*)&lastValidBPM, sizeof(lastValidBPM));
        hrChar->notify();

        spo2Char->setValue((uint8_t*)&lastValidSpO2, sizeof(lastValidSpO2));
        spo2Char->notify();

        int32_t irVal=irBuffer[i];
        irChar->setValue((uint8_t*)&irVal, sizeof(irVal));
        irChar->notify();
      }
      delay(1); //Let BLE breathe
    }

    //OLED updates
    if (millis() - lastDisplayUpdate > displayInterval)  //If programmed refresh time has past
    {
      //Initialize screen
      lastDisplayUpdate = millis();         //Updates display update value relative to run time
      display.clearDisplay();               //Clears display
      display.setTextSize(1);               //Font size
      display.setTextColor(SSD1306_WHITE);  //Text color
      //BPM OLED text
      if (lastValidBPM == 0)  //Creates an OLED animation that allows for data processing
      {
        display.setCursor(0, 0);
        display.println("HR: ---");
      } else {
        String hrText = "HR: " + String(lastValidBPM) + " BPM";  //Valid value sensing
        display.setCursor(0, 0);
        display.println(hrText);
      }
      // SpO2 OLED text
      if (lastValidSpO2 == 0)  //Data processing OLED printing
      {
        display.setCursor(0, SCREEN_HEIGHT - 16);
        display.println("SpO2: ---");
      } else {
        String spo2Text = "SpO2: " + String(lastValidSpO2) + "%";  //Valid value sensing
        display.setCursor(0, SCREEN_HEIGHT - 16);
        display.println(spo2Text);

        // Battery animation for SpO2
        int batteryWidth = 20;                             //Rectangle width
        int batteryHeight = 8;                             //Rectangle Height
        int batteryX = SCREEN_WIDTH - batteryWidth - 2;    // X positining on OLED with some space from side
        int batteryY = SCREEN_HEIGHT - batteryHeight - 2;  // Y positioning on OLED with some space from bottom

        // Draw battery body
        display.drawRect(batteryX, batteryY, batteryWidth, batteryHeight, SSD1306_WHITE);  //Draws the rectangle to resemble a battery on screen

        // Draw battery tip
        display.drawRect(batteryX + batteryWidth, batteryY + 2, 2, batteryHeight - 4, SSD1306_WHITE);

        // Calculate fill width from SpO2
        int fillWidth = map(lastValidSpO2, 80, 100, 0, batteryWidth - 2);  //Fill based on spo2 percentage
        fillWidth = constrain(fillWidth, 0, batteryWidth - 2);             //Keeps from overfilling

        // Fill battery level
        display.fillRect(batteryX + 1, batteryY + 1, fillWidth, batteryHeight - 2, SSD1306_WHITE);  //Displays on OLED
      }

      display.display();
    }
    // Clear heart area
    int heartX = SCREEN_WIDTH - 14;
    int heartY = 0;
    display.fillRect(heartX, heartY, 14, 12, SSD1306_BLACK);

    // Show heart only for 300ms after beat
    if (pulseVisible && millis() - pulseStartTime < 300) {
      display.fillCircle(heartX + 4, heartY + 3, 3, SSD1306_WHITE);
      display.fillCircle(heartX + 8, heartY + 3, 3, SSD1306_WHITE);
      display.fillTriangle(heartX + 2, heartY + 5, heartX + 6, heartY + 11, heartX + 10, heartY + 5, SSD1306_WHITE);
    } else {
      pulseVisible = false;  // Hide the heart after blink duration
    }

    // Always push this small area update
    display.display();

    //After gathering 25 new samples recalculate HR and SP02
    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    Serial.print(beatAvg, DEC);  //Prints to screen

    Serial.print(F("\t HRvalid="));
    Serial.print(validHeartRate, DEC);

    Serial.print(F("\t SPO2="));
    Serial.print(spo2Avg, DEC);

    Serial.print(F("\t SPO2Valid="));
    Serial.println(validSPO2, DEC);

    //LED Therapy
    unsigned long now = millis(); 
    unsigned long breathElapsed = (now - breathStartTime) % totalBreathCycle;
    int brightness = 0; 

    if (breathElapsed < inhaleTime) //Inhale
    {
      float iprogress = (float)breathElapsed / inhaleTime; //Fraction so light builds
      brightness = (int)(255 * iprogress); 
    }
    else if (breathElapsed < inhaleTime + holdTime) //Hold
    {
      brightness = 255;
    }
    else
    {
      float eprogress = (float)(breathElapsed - inhaleTime - holdTime) / exhaleTime; //Exhale
      brightness = (int)(255 * (1.0 - eprogress)); 
    }
    ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, brightness);
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0);
 
  }
}

