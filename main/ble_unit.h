#pragma once

#include <stdint.h>

#define BLE_UNIT_DESCRIPTOR_UUID 0x2904
#define BLE_UNIT_PERCENTAGE 0x27AD

typedef struct _ble_unit_data {
  uint8_t format;
  uint8_t exponent;
  uint16_t unit;
  uint8_t ns;
  uint16_t description;
} __attribute__((packed)) ble_unit_data_t;
