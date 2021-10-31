/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

#include <Arduino.h>
//Screen Imports
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//Bluetooth Imports
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

//Local Variables
static int power = 0;
static int resistance = 0;
static int cadence = 0;
static bool connected_to_server = false;

//Echelon Services
static BLEUUID     deviceUUID("0bf669f0-45f2-11e7-9598-0800200c9a66");
static BLEUUID    connectUUID("0bf669f1-45f2-11e7-9598-0800200c9a66");
static BLEUUID      writeUUID("0bf669f2-45f2-11e7-9598-0800200c9a66");
static BLEUUID     sensorUUID("0bf669f4-45f2-11e7-9598-0800200c9a66");

//Screen Constants
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     4 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//Bluetooth Variables
int scanTime = 5; //In seconds
BLEScan* pBLEScan;
BLEClient* pBLEClient;
static BLERemoteCharacteristic* sensorCharacteristic;
static BLERemoteCharacteristic* writeCharacteristic;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      Serial.printf("Advertised Device: %s \n", advertisedDevice.toString().c_str());
    }
};

class ClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("Connected!");
  }
  void onDisconnect(BLEClient* pclient) {
    connected_to_server = false;
    sensorCharacteristic = NULL;
    writeCharacteristic = NULL;
    power = 0;
    cadence = 0;
    resistance = 0;
    Serial.println("Disconnected!");
  }
};

// Called when device sends update notification
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* data, size_t length, bool isNotify) {
  switch(data[1]) {
    // Cadence notification
    case 0xD1:
      cadence = int((data[9] << 8) + data[10]);
      power = int(pow(1.090112, resistance) * pow(1.015343, cadence) * 7.228958);
      break;
    // Resistance notification
    case 0xD2:
      resistance = int(data[3]);
      power = int(pow(1.090112, resistance) * pow(1.015343, cadence) * 7.228958);
      break;
  }
}

void clear_display(){
  display.clearDisplay();
}

void draw_string(const char* string, int cursor_x, int cursor_y){
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(cursor_x, cursor_y);
  display.println(F(string));
}

void draw_main_status(){
  char cadence_str[5];
  char power_str[5];
  char resistance_str[5];

  sprintf(cadence_str,"%d",cadence);
  sprintf(power_str,"%d",power);
  sprintf(resistance_str,"%d",resistance);

  clear_display();
  
  draw_string("Cad",10,0);
  draw_string(cadence_str,10,10);
  draw_string("Pow",40,0);
  draw_string(power_str,40,10);
  draw_string("Res",70,0);
  draw_string(resistance_str,70,10);

  display.display();
}

void get_bt_devices(){
  // We need to figure out proper object checking here, don't
  // want to accidentally use an uninitialized pBLEScan pointer.
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  for(int i = 0;i < foundDevices.getCount(); i++){
    BLEAdvertisedDevice dev = foundDevices.getDevice(i);
    if(dev.getName().size() < 1) continue;
    
    Serial.print("Name -> ");
    Serial.println(dev.getName().c_str());

    pBLEClient = BLEDevice::createClient();
    pBLEClient->setClientCallbacks(new ClientCallback());
    bool conn = pBLEClient->connect(&dev);

    delay(200);
    if(!conn){
      continue;
    }
    
    BLERemoteService* remoteService = pBLEClient->getService(connectUUID);
    if (remoteService == NULL) {
      Serial.print("Failed to find service UUID: ");
      Serial.println(connectUUID.toString().c_str());
      pBLEClient->disconnect();
      delay(200);
      continue;
    }
    Serial.println("Found device!");

    // Look for the sensor
    sensorCharacteristic = remoteService->getCharacteristic(sensorUUID);
    if (sensorCharacteristic == NULL) {
      Serial.print("Failed to find sensor characteristic UUID: ");
      Serial.println(sensorUUID.toString().c_str());
      pBLEClient->disconnect();
      continue;
    }
    sensorCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Enabled sensor notifications.");
  
    // Look for the write service
    BLERemoteCharacteristic* writeCharacteristic = remoteService->getCharacteristic(writeUUID);
    if (writeCharacteristic == NULL) {
      Serial.print("Failed to find write characteristic UUID: ");
      Serial.println(writeUUID.toString().c_str());
      pBLEClient->disconnect();
      continue;
    }
    // Enable device notifications
    byte message[] = {0xF0, 0xB0, 0x01, 0x01, 0xA2};
    writeCharacteristic->writeValue(message, 5);
    Serial.println("Activated status callbacks.");
    
    connected_to_server = true;
    break;
  }
}


void setup() {
  Serial.begin(9600);

  //Check for display
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    //TODO Lets do more than just print to serial, in the
    //case that we don't have serial available
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  clear_display();
  
  //Setup our Bluetooth scanner
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value

}

void loop() {
  while(!connected_to_server){
    draw_string("Connecting...",10,15);
    get_bt_devices();
  }
  clear_display();
  draw_main_status();
  delay(200);
  
}
