#pragma once

#include <stdint.h>

#define BLE_DEVICE_INFO_SERVICE_UUID 0x180A
#define BLE_DEVICE_MANUFACTURER_UUID 0x2A29
#define BLE_DEVICE_PNP_UUID 0x2A50

typedef struct __pnp_id_data {
  uint8_t vid_src;
  uint16_t vid;
  uint16_t pid;
  uint16_t ver;
} __attribute__((packed)) pnp_id_data_t;

int ble_device_info_init(void);
