import asyncio
import sys
from bleak import BleakScanner, BleakClient
import numpy as np
import pandas as pd
from xlsxwriter import Workbook
from datetime import datetime

def BLE_ASYNC_IMU_UUID(val):
    return "3701be4f-" + val + "-4fa7-a6f9-617c3c7f8c0f"

SERVICE_UUID = BLE_ASYNC_IMU_UUID("0000")
samples_count_characteristic_uuid = BLE_ASYNC_IMU_UUID("9912")
command_characteristic_uuid = BLE_ASYNC_IMU_UUID("9916")
sampling_time_characteristic_uuid = BLE_ASYNC_IMU_UUID("9913")
samples_block_characteristic_byte_count = 128
samples_block_characteristic_uuid = BLE_ASYNC_IMU_UUID("9914")
samples_type_name_characteristic_byte_count = 128
samples_type_name_characteristic_uuid = BLE_ASYNC_IMU_UUID("9915")
crc32_characteristic_uuid = BLE_ASYNC_IMU_UUID("9917")

client = None

# # Example usage:
# def main:
#     data = b'hello-world'  # Example data as bytes
#     checksum = hex(crc32(data))
#     print(f"CRC32 checksum: {checksum}")
#     return 
import zlib
def crc32(data) -> int:
    return zlib.crc32(data) & 0xffffffff

#     byte_array = np.array([16], dtype=np.int32).tobytes()
#     print (byte_array)
#     print(number_to_byte_array(16))
#     return
def number_to_byte_array(number):
    return number.to_bytes(4, byteorder='little')

def byte_array_to_uint32(data) -> np.uint32:
    return np.frombuffer(data, dtype=np.uint32, count=1).astype(np.uint32)[0]

def byte_array_to_ulong(data) -> np.ulong:
    return np.frombuffer(data, dtype=np.ulong, count=1).astype(np.ulong)[0]

class BLEAsynchronousModeIMUClient(object):
    def __init__(self, uuid:str, debug) -> None:
        super().__init__()
        self._client = None
        self._device = None
        self._service = None
        self._connected = False
        self._running = False
        self._uuid = uuid
        self._found = False
        self._sampling_time: np.ulong
        self._samples_count: np.uint32 = np.uint32(0)
        self._last_block_crc32: int = 0
        self._samples_type_name: str = ""
        self._samples_type_name_array: List[str] = []
        self._type_name: str
        self._samples = []
        self.debug = debug
        self.disconnected_event = asyncio.Event()
        self._float_index = 0
        self._samples_read_ok = False
        
    @property
    def connected(self) -> bool:
        return self._connected
    
    @property
    def uuid(self) -> str:
        return self._uuid
        

    @property
    def running(self) -> bool:
        return self._running
    
    @property
    def device(self):
        return self._device
    
    async def send_command(self, cmd:int):
        return await self._client.write_gatt_char(self._service.get_characteristic(command_characteristic_uuid), data=number_to_byte_array(cmd), response=True) 
    
    async def send_last_block_crc32(self):
        characteristic = self._service.get_characteristic(crc32_characteristic_uuid)
        return await self._client.write_gatt_char(characteristic, data=number_to_byte_array(self._last_block_crc32), response=True) 

    def disconnected_callback(self, client: BleakClient):
        print("Disconnected callback called!")
        self.disconnected_event.set()
        if self._connected:
            # Stop notification first.
            self._connected = False
            self._running = False
    
    async def start(self) -> None:
        if self._connected:
            # Start notification
            if(self.debug): print("BLE: start notify")
            await self._client.start_notify(samples_block_characteristic_uuid, self.samples_block_characteristic_hndlr)
            await self._client.start_notify(sampling_time_characteristic_uuid, self.sampling_time_characteristic_hndlr)
            await self._client.start_notify(samples_count_characteristic_uuid, self.samples_count_characteristic_hndlr)
            await self._client.start_notify(samples_type_name_characteristic_uuid, self.samples_type_name_characteristic_hndlr)
            
            self._running = True
            if(self.debug): print("BLE: running")
    
    async def stop(self) -> None:
        if self._connected:
            # Start notification
            if(self.debug): print("BLE: stopping notifications !")
            await self._client.stop_notify(samples_block_characteristic_uuid)
            await self._client.stop_notify(sampling_time_characteristic_uuid)
            await self._client.stop_notify(samples_count_characteristic_uuid)
            await self._client.stop_notify(samples_type_name_characteristic_uuid)
            self._running = True
            if(self.debug): print("BLE: stopped")
    
    async def samples_block_characteristic_hndlr(self, sender, data):
        block = np.frombuffer(data, dtype=np.float32, count=32)     
        nrows = int(self._samples_count/self._sample_unit_size)
        ncols = self._sample_unit_size
        if (len(self._samples) == 0):
            self._samples = np.empty((nrows, ncols))
            self._float_index = 0
            self._samples_read_ok = False
        for i in range(0, block.size):
            col = self._float_index % ncols
            row = int((self._float_index-col) / ncols)
            self._samples[row][col] = block[i]
            self._float_index += 1
            if self._float_index == self._samples_count:
                self._samples_read_ok = True
                break
            sys.stdout.write(f"\rProgression = {int(100.0*self._float_index/self._samples_count)} %\t\t\r")
            sys.stdout.flush()
        self._last_block_crc32 = crc32(block)
        if(self.debug): print(f"block crc32 is{hex(self._last_block_crc32)}")
        await self.send_last_block_crc32()
        if self._samples_read_ok == True:
            self.write_excel()
        
    def write_excel(self): 
        my_date = datetime.now().strftime("_%m%d%Y_%H%M%S")
        filename = f"out{my_date}_{self._device.address.replace(':', '-')}.xlsx"
        print(f"Writing {self._samples_count} samples of {self._samples_type_name_array} to file {filename}")
        df = pd.DataFrame(self._samples, columns=self._samples_type_name_array)
        if(self.debug): print(self._samples_type_name_array)
        time_step = self._sampling_time / (len(self._samples) - 1)
        # insert a time column that converts milliseconds into seconds
        df.insert(0, 'time', np.arange(0.0, 1E-3*(self._sampling_time+0.1*time_step), 1E-3*time_step))
        with pd.ExcelWriter(filename, engine='xlsxwriter') as writer:
            df.to_excel(writer)
        
    def sampling_time_characteristic_hndlr(self, sender, data):
        self._sampling_time = byte_array_to_ulong(data)
        if(self.debug): print(f"sampling time = {self._sampling_time}ms" )
    async def read_sampling_time_characteristic(self):
        data = await self._client.read_gatt_char(self._client.services.get_characteristic(sampling_time_characteristic_uuid))
        return byte_array_to_ulong(data)
        
    def samples_count_characteristic_hndlr(self, sender, data):
        self._samples_count = byte_array_to_uint32(data)
        if(self.debug): print(f"_samples_count = {self._samples_count} values" )
    async def read_samples_count_characteristic(self) -> int:
        data = await self._client.read_gatt_char(self._client.services.get_characteristic(samples_count_characteristic_uuid))
        return byte_array_to_uint32(data)
    
    async def read_samples_type_name_characteristic(self) -> str:
        data = await self._client.read_gatt_char(self._client.services.get_characteristic(samples_type_name_characteristic_uuid))
        self._samples_type_name = data.decode('utf-8')
        self._samples_type_name_array = self._samples_type_name.rstrip().split(',')
        if(self.debug): print (self._samples_type_name_array)
        self._sample_unit_size = len(self._samples_type_name_array)
        if(self.debug): print(f"Number of sampled IMU values = {self._sample_unit_size}" )
        return self._samples_type_name
    def samples_type_name_characteristic_hndlr(self, sender, data):
        self._samples_type_name = data.decode('utf-8')
        self._samples_type_name_array = self._samples_type_name.rstrip().split(',')
        if(self.debug): print (self._samples_type_name_array)
        self._sample_unit_size = len(self._samples_type_name_array)
        if(self.debug): print(f"Number of sampled IMU values = {self._sample_unit_size}" )
    
    # Function to connect to the peripheral device
    async def connect(self) -> None:
#        global samples_count_characteristic_uuid
#        global client
#        global SERVICE_UUID
        print("- Discovering peripheral device...")
        # https://github.com/hbldh/bleak/discussions/1271
        devices = await BleakScanner.discover(return_adv=True)
        self._device = None
        self._service = None
        self._connected = False
        for key, dev_and_adv_dat in devices.items():
            device = dev_and_adv_dat[0]
            name = device.name or ""
            adv_dat = dev_and_adv_dat[1]
            if self._uuid.upper() == device.address.upper():
                self._device = device
                break

        if self._device:
            print("* Peripheral device found!")
            print(f"* Device address: {self._device.address}")
            print(f"* Device name: {self._device.name}")
            
        await self.start()
            
    # RECORDING_TYPE: 10513 --> RECORD_DURING_SECONDS
    # 							BLE communication will be stopped during recording. 
    #							This sampling rate will be higher than RECORD_UNTIL_STOP command.
    # RECORDING_TYPE: 0 --> RECORD_UNTIL_STOP The sampling rate will be slower than RECORD_DURING_SECONDS command.
    async def sampling(self, seconds: int, mode: int, RECORDING_TYPE: int) -> None:      
        if self._device:
            async with BleakClient(self._device.address, disconnected_callback=self.disconnected_callback) as self._client:
                self._connected = True
                self._service = self._client.services.get_service(SERVICE_UUID.upper())
                # starts notifications
                await self.start()
                print(f"Start logging for {seconds}s")
                if RECORDING_TYPE == 0: # RECORD_UNTIL_STOP 
                    await self.send_command(mode) # start to log "gz"
                    await asyncio.sleep(seconds) # wait 
                    # send through BLE stop sampling command
                    print("Stops logging")
                    await self.send_command(0)     
                if RECORDING_TYPE == 10513: # RECORD_DURING_SECONDS
                    await self.send_command(10000 + mode) # select log mode                    await self.stop() # stop notifications
                    await self.send_command(10512+seconds) # log during <seconds> seconds
                    await self.disconnected_event.wait()
                    # Reconnects through BLE
                    async with BleakClient(self._device.address, disconnected_callback=self.disconnected_callback, timeout=seconds+1) as self._client:
                        self._connected = True
                        self._service = self._client.services.get_service(SERVICE_UUID.upper())
                        return
                    # await asyncio.sleep(seconds) # wait
                    #await self._client.disconnect()                    
                    #print("Sleeping until device disconnects...")
                    #await self.disconnected_event.wait()
                
    async def get_samples(self) -> None:
        if self._device:
            async with BleakClient(self._device.address, disconnected_callback=self.disconnected_callback) as self._client:
                self._connected = True
                self._service = self._client.services.get_service(SERVICE_UUID.upper())
                # starts notifications
                await self.start()
                # clear samples
                self._float_index = 0
                self._samples = []
                self._samples_count = await self.read_samples_count_characteristic()
                self._sampling_time = await self.read_sampling_time_characteristic()
                print(f"Sampling rate was {round(1000*self._samples_count/(self._sample_unit_size*self._sampling_time))} Hz... Sampling time was {round(0.001*self._sampling_time)}s... \nPlease Wait until the {self._samples_count} values are transfered :-)")
                await self.read_samples_type_name_characteristic()
                await self.send_command(512) # transfer values
                while self._samples_read_ok == False:                            
                    await asyncio.sleep(0.1) # wait           
                print("Sleeping until device disconnects...")

#default values of parameters
# activate all IMU fileds                
ax=ay=az=gx=gy=gz=mx=my=mz=1
# log during <int> seconds
duration=60
# recording_type: 0 (slower sampling rate, BLE communication is not stopped)
# or 10513 (highest frame rate, BLE communication is stopped during sampling)
recording_type = 10513
# Debug mode: False by default
debug_mode = False
# BLE Peripheral Address copied from the Arduino serial monitor
ble_peripheral_address='14:2a:5f:05:b4:f7'
# Main function to run the program
async def main():
    # USER should set which IMU fields to ignore
    # e.g. "mx=0" to disable mx (x axis of magnetometer)
    gx=gz=ax=ay=az=mx=my=mz=0
    
    imu_client1 = BLEAsynchronousModeIMUClient(ble_peripheral_address, debug_mode)
    await imu_client1.connect()
    await imu_client1.sampling(seconds=duration, mode=((az << 2) | (ay << 1) | (ax << 0)| (gz << 5) | (gy << 4) | (gx << 3) | (mz << 8) | (my << 7) | (mx << 6)), RECORDING_TYPE=recording_type)
    await imu_client1.get_samples()
    await asyncio.sleep(1)
    return

# Run the main function
asyncio.run(main())
