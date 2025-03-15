#pragma once

#include <stdint.h>

#define BLE_CCCD_DESCRIPTOR_UUID 0x2902

typedef struct __ble_cccd_data {
  uint8_t notif_enabled : 1;
  uint8_t ind_enabled : 1;
  uint8_t reserved : 6;
  uint8_t reserved2;
} __attribute__((packed)) ble_cccd_data_t;
