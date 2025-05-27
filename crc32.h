// See http://home.thep.lu.se/~bjorn/crc/ for more information on simple CRC32 calculations.
uint32_t crc32_for_byte(uint32_t r) {
  for (int j = 0; j < 8; ++j) {
    r = (r & 1? 0: (uint32_t)0xedb88320L) ^ r >> 1;
  }
  return r ^ (uint32_t)0xff000000L;
}

uint32_t crc32(const uint8_t* data, size_t data_length) {
  constexpr int table_size = 256;
  static uint32_t table[table_size];
  static bool is_table_initialized = false;
  if (!is_table_initialized) {
    for(size_t i = 0; i < table_size; ++i) {
      table[i] = crc32_for_byte(i);
    }
    is_table_initialized = true;
  }
  uint32_t crc = 0;
  for (size_t i = 0; i < data_length; ++i) {
    const uint8_t crc_low_byte = static_cast<uint8_t>(crc);
    const uint8_t data_byte = data[i];
    const uint8_t table_index = crc_low_byte ^ data_byte;
    crc = table[table_index] ^ (crc >> 8);
  }
  return crc;
}

// This is a small test function for the CRC32 implementation, not normally called but left in
// for debugging purposes. We know the expected CRC32 of [97, 98, 99, 100, 101] is 2240272485,
// or 0x8587d865, so if anything else is output we know there's an error in the implementation.
void testCrc32() {
  constexpr int test_array_length = 5;
  const uint8_t test_array[test_array_length] = {97, 98, 99, 100, 101};
  const uint32_t test_array_crc32 = crc32(test_array, test_array_length);
  Serial.println(String("CRC32 for [97, 98, 99, 100, 101] is 0x") + String(test_array_crc32, 16) + 
    String(" (") + String(test_array_crc32) + String(")"));
}

/*
#include "crc32.h"
void setup()
{
  char test[] = "hello-world";
  Serial.begin(9600);
  while(!Serial);
  uint32_t test_array_crc32 = crc32((uint8_t*)test, String(test).length());
  Serial.println(String("The awaited CRC32 is ")+String(test)+String(" 0x") + String(test_array_crc32, 16) + 
    String(" (") + String(test_array_crc32) + String(")"));
}
void loop()
{
  delay(1000);
}*/