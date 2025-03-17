#pragma once

#include <stdint.h>
#include <stddef.h>

#define BLE_HID_DEFAULT_REPORT_ID 0x01

#define BLE_HID_SERVICE_UUID 0x1812
#define BLE_HID_INFO_UUID 0x2A4A
#define BLE_HID_REPORT_MAP_UUID 0x2A4B
#define BLE_HID_CONTROL_POINT_UUID 0x2A4C
#define BLE_HID_REPORT_UUID 0x2A4D
#define BLE_HID_PROTOCOL_MODE_UUID 0x2A4E

#define BLE_REPORT_DESCRIPTOR_UUID 0x2908

typedef enum {
  BLE_HID_REPORT_TYPE_INPUT = 0x01,
  BLE_HID_REPORT_TYPE_OUTPUT = 0x02,
  BLE_HID_REPORT_TYPE_FEATURE = 0x03,
} ble_hid_report_type_t;

typedef struct hid_report_descriptor {
  uint8_t report_id;
  ble_hid_report_type_t report_type;
} __attribute__((packed)) ble_hid_report_descriptor_t;

typedef struct hid_info_data {
  uint8_t hid_version[2];
  uint8_t country_code;
  uint8_t flags;
} __attribute__((packed)) ble_hid_info_data_t;

typedef enum {
  BLE_HID_PROTOCOL_MODE_BOOT = 0x00,
  BLE_HID_PROTOCOL_MODE_REPORT = 0x01,
} ble_hid_protocol_mode_t;

int ble_hid_init(void);

#ifdef __cplusplus
extern "C" {
#endif

int ble_hid_send_report(uint8_t report_id, const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif
