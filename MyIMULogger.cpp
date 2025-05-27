#include "MyIMULogger.h"

    MyIMULogger::MyIMULogger(TwoWire& wire) : MyBoschSensor(wire) {
      afIMU = new float[afIMU_maximum_byte_size/sizeof(float)];
    };

    
    void MyIMULogger::setMode (int mode)
    {      
        nb_floats_per_sample=0;
        for (unsigned j=0;j<9;j++) 
          if (mode & (1<<j)) 
          {
            ptr[j] = f+j;
            samples_type_name[nb_floats_per_sample] = samples_type_name_all[j];
            nb_floats_per_sample++;
          }
          else 
          {
            ptr[j] = NULL; 
          }
        for (unsigned j=0;j<9;j++) 
          Serial.println(String("ptr[") + String(j) + String("]=0x") + String(uint32_t(ptr[j]),HEX));
    }
    void MyIMULogger::setMode (bool ax,bool ay, bool az, bool gx, bool gy, bool gz, bool mx, bool my, bool mz)
    {
        return setMode((az << 2) | (ay << 1) | (ax << 0)|
        (gz << 5) | (gy << 4) | (gx << 3)|
        (mz << 8) | (my << 7) | (mx << 6));
    }

    void MyIMULogger::decode_imu_mode (int x, 
    bool &ax, bool &ay, bool &az, bool &gx, bool &gy, bool &gz, bool &mx, bool &my, bool &mz)
    {
      bool *array[9] = { &ax, &ay, &az, &gx, &gy, &gz, &mx, &my, &mz};
      for (int j=0;j<9;j++) *array[j] = (x & (1<<j));
    }

    void MyIMULogger::clearSamples()
    {
      samples_count = 0;
      samples_floats_count = 0;
    }

    bool MyIMULogger::addSample()
    {
      if (samples_count == 0) 
        samples_end_time = samples_start_time = millis();
      else
        samples_end_time = millis();
      
      if (sizeof(float)*(samples_floats_count + nb_floats_per_sample) > afIMU_maximum_byte_size)
        {
          Serial.println("Maximal memory reached");
          return false;
        }

      bool result = readIMU(ptr[0],ptr[1],ptr[2],ptr[3],ptr[4],ptr[5],ptr[6],ptr[7],ptr[8]);
      if (result == false) return 0;
      
      for (unsigned j=0;j<9;j++) 
        if (ptr[j]) 
          afIMU[samples_floats_count++] = *(ptr[j]); 

      samples_count = samples_floats_count / nb_floats_per_sample;
      return result;
    }

    // @param - pointers on float numbers representing the raw values of the IMU
    // to increase sampling rate, and save memory, unnecessary pointers should be NULL
    bool MyIMULogger::readIMU(float *ax, float *ay, float *az,
    float *gx, float *gy, float *gz,
    float *mx, float *my, float *mz
    ) {
      float x,y,z;
      int reads = 0;
      // Update the sensor values whenever new data is available
      if ((ax||ay||az) && this->accelerationAvailable()) {
        if (this->readAcceleration(x,y,z))
        {
          if(ax) {*ax = x; reads++; }
          if(ay) {*ay = y; reads++; }
          if(az) {*az = z; reads++; }
        } else
          Serial.println("IMU.readAcceleration failed");
      }
      if ((gx||gy||gz) && this->gyroscopeAvailable()) {
        if (this->readGyroscope(x,y,z)) {
          if(gx) {*gx = x; reads++; }
          if(gy) {*gy = y; reads++; }
          if(gz) {*gz = z; reads++; }
        } else
          Serial.println("IMU.readGyroscope failed");
      }
      if ((mx||my||mz) && this->magneticFieldAvailable()) {
        if (this->readMagneticField(x,y,z)) {
          if(mx) {*mx = x; reads++; }
          if(my) {*my = y; reads++; }
          if(mz) {*mz = z; reads++; }
        } else
          Serial.println("IMU.readAcceleration failed");
      }
      return (reads != 0);
    }

    void MyIMULogger::printSamples()
    {
      char separator = ',';
      for (int i=0; i<nb_floats_per_sample; i++)
      {
        Serial.print(samples_type_name[i]);
        Serial.print(separator);
      }
      Serial.println("");
      for (int i=0; i<samples_floats_count; i++)
      {
        //Serial.print("Sample [ ");
        //Serial.print(i/nb_floats_per_sample);
        //Serial.print("] (type ");
        //Serial.print(samples_type_name[i%nb_floats_per_sample]);
        //Serial.print(") = ");
        Serial.print(afIMU[i]);
        Serial.print(separator);
        if ((i+1)%nb_floats_per_sample == 0) Serial.println("");
      }
      Serial.println(String("Sample count = ") + String(this->samples_count));
      Serial.println(String("Sample float count = ") + String(this->samples_floats_count));
      Serial.print("Sampling rate = ");
      Serial.print(samples_count / (1E-3 * (samples_end_time - samples_start_time)));
      Serial.println(" Hz");
    }

    
unsigned long MyIMULogger::sample (unsigned long milliseconds) {  
  this->samples_start_time = millis();
  while((this->samples_end_time=millis()) - this->samples_start_time < milliseconds)
    this->addSample();
  Serial.print("Sampling rate = ");
  Serial.print(samples_count / (1E-3 * (samples_end_time - samples_start_time)));
  Serial.println(" Hz");
  return this->samples_end_time - this->samples_start_time;
}