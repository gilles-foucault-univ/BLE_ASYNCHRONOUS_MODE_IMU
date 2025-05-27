/** Usage example:
in "main" Arduino program:

//https://github.com/arduino-libraries/Arduino_BMI270_BMM150/issues/25
#define TARGET_ARDUINO_NANO33BLE
#include "MyIMULogger.h"

MyIMULogger myIMU(Wire1);

void setup() {
  Serial.begin(115200);
  while (!Serial);
  myIMU.debug(Serial);
  myIMU.begin();
  Wire1.setClock(400000);

  bool ax=false, ay=false, az=false;
  bool gx=true, gy=true, gz=true;
  bool mx=false, my=false, mz=false;
  myIMU.setMode(ax,ay,az,gx,gy,gz,mx,my,mz);
}

void loop() {  
  myIMU.addSample();
  if (myIMU.samples_end_time - myIMU.samples_start_time > 20000)
  {
    myIMU.printSamples();
    myIMU.clearSamples();
    delay(10000);
  }
}*/

#ifndef  MyIMULogger_H
#define MyIMULogger_H

#include "MyBoschSensor.h"

class MyIMULogger : public MyBoschSensor {
  public:
    MyIMULogger(TwoWire& wire = Wire);

  public:
    float *afIMU = NULL;
    unsigned int samples_floats_count = 0;
    unsigned int samples_count = 0;
    unsigned long samples_start_time = 0; // ms
    unsigned long samples_end_time = 0; // ms
    unsigned int nb_floats_per_sample = 0;
    static const int afIMU_maximum_byte_size = (100 * 1024);

    int mode = 0;
    // temporary array of floats for a sample
    float f[9];
    // temporary array of addresses for a sample (pointing to f[])
    float* ptr[9];
    char  samples_type_name_all [9][3] = {"ax","ay","az","gx","gy","gz","mx","my","mz"};
    char  *samples_type_name [9];
    void setMode (int mode);
    void setMode (bool ax,bool ay, bool az, bool gx, bool gy, bool gz, bool mx, bool my, bool mz);

    void decode_imu_mode (int x, bool &ax, bool &ay, bool &az, bool &gx, bool &gy, bool &gz, bool &mx, bool &my, bool &mz);
    void clearSamples();
    bool addSample();
    unsigned long sample(unsigned long milliseconds);

    // @param - pointers on float numbers representing the raw values of the IMU
    // to increase sampling rate, and save memory, unnecessary pointers should be NULL
    bool readIMU(float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz
    );

    void printSamples();

};

#endif // MyIMULogger_H

