# BLE Asynchronous transfer mode IMU
This code connects to the Arduino Nano 33 BLE Peripheral and executes commands to sample
Bosch BMI270 IMU axes (acceleration, gyroscope, magnetometer).
In order to maximize the sampling rate, data are transfered asynchronously *after* sampling.
The achieved sampling rate is:
- 167Hz for 6DOF (acceleration+gyroscope) when the sampling method *stops* BLE communication during transfer (RECORD_DURING_SECONDS)
- 96Hz for 6DOF (acceleration+gyroscope) when the sampling method *does not* stop BLE during transfer (RECORD_UNTIL_STOP)

Example of transferring file data over BLE to an Arduino Nano Sense using WebBLE are in:
- out_05272025_102118_14-2A-5F-05-B4-F7.xlsx (RECORD_DURING_SECONDS)
- out_05272025_102038_14-2A-5F-05-B4-F7.xlsx (RECORD_UNTIL_STOP)

The total number of samples is limited by the RAM of the arduino nano 33 ble: 100 * 1024 float values 

## Trying it out

To begin, in the Arduino IDE:
* install library Arduino_BMI270_BMM150
* flash the BLE_ASYNCHRONOUS_MODE_IMU.ino sketch onto an Arduino Nano BLE 33 board, open the serial monitor

Then, in your preferred python environment: 
* pip install XlsxWriter bleak numpy time pandas datetime
* edit BLE_ASYNCHRONOUS_MODE_IMU_Client.py into a python environment (preferably Thonny), and personalize the default values (duration,recording_type,debug_mode)
* run main

## How does the peripheral works?

command_characteristic 
0	STOP	stop IMU recording
1-511	RECORD_UNTIL_STOP	1-511 : starts IMU data recording corresponding to <command value> =  
(az << 2) | (ay << 1) | (ax << 0)|
    (gz << 5) | (gy << 4) | (gx << 3)|
    (mz << 8) | (my << 7) | (mx << 6)
After recording, the number of samples is written in characteristic “samples_count_characteristic” UUID 3701be4f-9912-4fa7-a6f9-617c3c7f8c0f
The sampling rate will be slower than RECORD_DURING_SECONDS command.
512 	BEGIN_TRANSFER	Begin transfer of samples
513	CANCEL_TRANSFER	Cancel transfer of samples
10000-10511	SELECT_IMU_MODE	Select IMU data to record: 
<command value> = 10000 + <mode> 
where <mode> = (az << 2) | (ay << 1) | (ax << 0)|
    (gz << 5) | (gy << 4) | (gx << 3)|
    (mz << 8) | (my << 7) | (mx << 6)
10513-11512	RECORD_DURING_SECONDS	Record IMU (previous call to SELECT_IMU_MODE) during <seconds> = <command value> -10512
The recording is impossible to cancel
BLE communication will be stopped during recording
This sampling rate will be higher than RECORD_UNTIL_STOP command.

BLE Service :
UUID : UUID 3701be4f-0000-4fa7-a6f9-617c3c7f8c0f
BLE Characteristics :
Name	UUID	Type	description	Type/Bytes
samples_count_characteristic	UUID 3701be4f-9912-4fa7-a6f9-617c3c7f8c0f	BLERead
	Number of samples recorded (updated AFTER last recording finishes).	Uint32

command_characteristic	UUID 3701be4f-9916-4fa7-a6f9-617c3c7f8c0f	BLEWrite
	Command for recording and data transfer	Uint32

sampling_time_characteristic	UUID 3701be4f-9913-4fa7-a6f9-617c3c7f8c0f	BLERead
	Total time in milliseconds of the last recording.	long
samples_block_characteristic	UUID 3701be4f-9914-4fa7-a6f9-617c3c7f8c0f	BLERead | BLENotify	Where each samples data block is notified to during the transfer.  Each block has a length of 128 bytes.	128 bytes

With recording_type=10513 set in .py file:
```
%Run BLE_ASYNCHRONOUS_MODE_IMU_Client.py
- Discovering peripheral device...
* Peripheral device found!
* Device address: 14:2A:5F:05:B4:F7
* Device name: None
Start logging for 1s
Stops logging
Disconnected callback called!
14:2A:5F:05:B4:F7: None
Sampling rate was 96 Hz
Writing 588 samples of ['ax', 'ay', 'az', 'gx', 'gy', 'gz'] to file out_05272025_102118_14-2A-5F-05-B4-F7.xlsx
Sleeping until device disconnects...
Disconnected callback called!
```

With recording_type=0 set in .py file:
```
%Run BLE_ASYNCHRONOUS_MODE_IMU_Client.py
- Discovering peripheral device...
* Peripheral device found!
* Device address: 14:2A:5F:05:B4:F7
* Device name: None
Start logging for 1s
Disconnected callback called!
Sleeping until device disconnects...
14:2A:5F:05:B4:F7: None
Sampling rate was 167 Hz
Sleeping until device disconnects...
Writing 1002 samples of ['ax', 'ay', 'az', 'gx', 'gy', 'gz'] to file out_05272025_102038_14-2A-5F-05-B4-F7.xlsx
Disconnected callback called!
```

## Known issues

## Thanks

