#include "MyIMULogger.h"
#include "crc32.h"

#define _debug 0

#include <ArduinoBLE.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
// Macro based on a master UUID that can be modified for each characteristic.
#define BLE_ASYNC_IMU_UUID(val) ("3701be4f-" val "-4fa7-a6f9-617c3c7f8c0f")

// see https://docs.arduino.cc/libraries/arduinoble/#BLECharacteristic%20Class
// https://github.com/arduino-libraries/ArduinoBLE/blob/master/src/BLETypedCharacteristics.cpp
// https://github.com/arduino-libraries/ArduinoBLE/blob/master/src/BLETypedCharacteristic.h
// https://github.com/arduino-libraries/ArduinoBLE/blob/master/src/BLECharacteristic.h
// Number of samples recorded (updated AFTER last recording finishes).
BLEService service(BLE_ASYNC_IMU_UUID("0000"));
BLEUnsignedIntCharacteristic samples_count_characteristic(BLE_ASYNC_IMU_UUID("9912"), BLERead | BLENotify);
BLEUnsignedIntCharacteristic command_characteristic(BLE_ASYNC_IMU_UUID("9916"), BLEWrite);
BLEUnsignedIntCharacteristic crc32_characteristic(BLE_ASYNC_IMU_UUID("9917"), BLEWrite);
BLELongCharacteristic sampling_time_characteristic(BLE_ASYNC_IMU_UUID("9913"), BLERead | BLENotify);
constexpr int32_t samples_block_characteristic_byte_count = 128;
static BLECharacteristic samples_block_characteristic(BLE_ASYNC_IMU_UUID("9914"), BLERead | BLENotify, samples_block_characteristic_byte_count);
constexpr int32_t samples_type_name_byte_count = 128;
BLECharacteristic samples_type_name_characteristic(BLE_ASYNC_IMU_UUID("9915"), BLERead | BLENotify, samples_type_name_byte_count);

// For Arduino Nano 33, specify wire parameter to Wire1
MyIMULogger myIMU (Wire1);
uint32_t command_value = 0;
// block transfer and crc32 values
uint32_t crc32_value = 0;
int block_id=0;
uint32_t latest_sent_block_crc32=0;
const int nb_float_per_block = samples_block_characteristic_byte_count / sizeof(float);

void send_type_name_characteristic_to_central()
{
  String samples_type_name;
  char separator = ',';
  for (int i=0; i<myIMU.nb_floats_per_sample; i++)
  {
    if (i>0) samples_type_name += String(separator);
    samples_type_name += String(myIMU.samples_type_name[i]);
  }
  while (samples_type_name.length() < samples_type_name_byte_count-1)
    samples_type_name += String(" ");
  char charArray[samples_type_name_byte_count]="";
  samples_type_name.toCharArray(charArray, samples_type_name_byte_count);
  samples_type_name_characteristic.writeValue(charArray);
  if (_debug) Serial.println(String("send_type_name_characteristic_to_central() -> ")+String(charArray));
}

void setMode(unsigned mode)
{
  myIMU.setMode(mode);
  send_type_name_characteristic_to_central();
}

  /**	0 : stop IMU recording
  1-511 : starts IMU recording corresponding to   command =  
  (az << 2) | (ay << 1) | (ax << 0)|
      (gz << 5) | (gy << 4) | (gx << 3)|
      (mz << 8) | (my << 7) | (mx << 6)
  After recording, the number of samples is written in characteristic “samples_count_characteristic” UUID 3701be4f-9912-4fa7-a6f9-617c3c7f8c0f
  512 	Begin file transfer
  513	Cancel file transfer */
  void onCommandWritten(BLEDevice central, BLECharacteristic characteristic) {  
    characteristic.readValue(command_value);
    if (_debug) Serial.println(String("Received command # ")+String(command_value));
    if (command_value == 0)
    {
      myIMU.samples_end_time = millis();
      send_type_name_characteristic_to_central();
      samples_count_characteristic.writeValue(myIMU.samples_floats_count);
      sampling_time_characteristic.writeValue(myIMU.samples_end_time-myIMU.samples_start_time);
    }

    if ((command_value >0) && (command_value < 512)) {
      myIMU.clearSamples();
      setMode(command_value);
      myIMU.samples_start_time = millis();
      return;
    }

    if (command_value == 512) {
      samples_count_characteristic.writeValue(myIMU.samples_floats_count);
      sampling_time_characteristic.writeValue(myIMU.samples_end_time-myIMU.samples_start_time);
      if (_debug) Serial.println("Start transfer");
      if (_debug) Serial.print("Samples Buffer contains * ");
      if (_debug) Serial.print(myIMU.samples_floats_count);
      if (_debug) Serial.print(" *  float values \n");
      block_id = 0;
    } else if (command_value == 513) {
      if (_debug) Serial.println("Cancel transfer");
    }

    if (command_value >= 10000 && command_value <= 10511)
    {
      setMode(command_value - 10000);
    }

    if (command_value >= 10513 && command_value <= 11512)
    {
      // this is treated in loop()
    }
  }

void onCrc32Written(BLEDevice central, BLECharacteristic characteristic) { 
  if (_debug) Serial.print(" / Received = 0x");
  characteristic.readValue(crc32_value);
  if (_debug) Serial.println(String(crc32_value, 16));
}

void setupBLE() {
  // Start the core BLE engine.
  if (!BLE.begin()) {
    if (_debug) Serial.println("Failed to initialize BLE!");
    // Stop if BLE couldn't be initialized.
    while (1);
  }
  // Get BLE device Address
  String address = BLE.address();

  // Output BLE settings over Serial.
  if (_debug) Serial.print("address = ");
  if (_debug) Serial.println(address);

  address.toUpperCase();

  static String device_name = "BLEAsynchronousModeIMU-";
  device_name += address[address.length() - 5];
  device_name += address[address.length() - 4];
  device_name += address[address.length() - 2];
  device_name += address[address.length() - 1];

  if (_debug) Serial.print("device_name = ");
  if (_debug) Serial.println(device_name);

  // Set up properties for the whole service.
  BLE.setLocalName(device_name.c_str());
  BLE.setDeviceName(device_name.c_str());
  BLE.setAdvertisedService(service);

  // Add in the characteristics we'll be making available.
  service.addCharacteristic(samples_count_characteristic);
  service.addCharacteristic(sampling_time_characteristic);
  service.addCharacteristic(samples_block_characteristic);
  service.addCharacteristic(samples_type_name_characteristic);

  command_characteristic.setEventHandler(BLEWritten, onCommandWritten);
  service.addCharacteristic(command_characteristic);

  crc32_characteristic.setEventHandler(BLEWritten, onCrc32Written);
  service.addCharacteristic(crc32_characteristic);

  // Start up the service itself.
  BLE.addService(service);
  BLE.advertise();
}

void setup() {
  // for debugging purposes
  if (_debug) {
    Serial.begin(115200);
    while (!Serial && millis()<500);
    myIMU.debug(Serial);
  }
  // Initialize IMU
  myIMU.begin();
  // Set I2C clock to Fast mode, 
  Wire1.setClock(400000);
  // sample during 100 ms
  myIMU.setMode(511);
  myIMU.sample(100);

  setupBLE();
}

void loop() {  
  BLEDevice central = BLE.central(); 
  static bool was_connected_last = false;  
  if (central && !was_connected_last) {
    if (_debug) Serial.print("Connected to central: ");
    if (_debug) Serial.println(central.address());

    // send current values
    samples_count_characteristic.writeValue(myIMU.samples_floats_count);
    sampling_time_characteristic.writeValue(myIMU.samples_end_time-myIMU.samples_start_time);
    send_type_name_characteristic_to_central();
  }
  was_connected_last = central;  

  // while RECORD_DURING_SECONDS is working
  while (command_value >= 10513 && command_value <= 11512)
  {
    // STOP BLE

    BLE.disconnect();
    BLE.end();

    // When the central disconnects, print it out:
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());    

    //https://github.com/arduino-libraries/Arduino_BMI270_BMM150/issues/25
    // Set I2C clock to Fast mode, 
    Wire1.setClock(400000);
    unsigned long target_millis = 1000*(command_value - 10512);
    myIMU.clearSamples();
    unsigned long nSamples = myIMU.sample(target_millis);
    // Reconnect
    was_connected_last = false;
    setupBLE();
    command_value = 512; // to resume loop()
  }

  if (command_value >0 && command_value < 512)
  {
    myIMU.addSample();
    if(_debug){
      Serial.print("Samples Buffer contains * ");
      Serial.print(myIMU.samples_floats_count);
      Serial.print(" *  float values \n");
    }
  }

  //for (int i=0; i < myIMU.samples_floats_count; i+=nb_float_per_block) 
  // if block_id=0: send block_id and update latest_sent_block_crc32
  // if block_id>0: Wait until the good CRC32 is received
  if (command_value == 512 && (block_id == 0 || (crc32_value == latest_sent_block_crc32))) {
    samples_block_characteristic.writeValue(myIMU.afIMU+block_id*nb_float_per_block, samples_block_characteristic_byte_count);

    if (_debug) Serial.print("Send floats [ ");
    if (_debug) Serial.print(block_id*nb_float_per_block);
    if (_debug) Serial.print(" - ");
    if (_debug) Serial.print(block_id*nb_float_per_block+nb_float_per_block);
    if (_debug) Serial.print(" ]... ");
    latest_sent_block_crc32 = crc32((uint8_t*)(myIMU.afIMU+block_id*nb_float_per_block), samples_block_characteristic_byte_count);
    if (_debug) Serial.print("CRC32 sent = ");
    if (_debug) Serial.print(String("0x") + String(latest_sent_block_crc32, 16));
    block_id ++;
    
    // Test sending is finished
    if (block_id*nb_float_per_block>=myIMU.samples_floats_count) 
    {
      command_value = 0;
      block_id = 0;
    }
  } else if (command_value == 513) {
    block_id = 0;
  }
}
