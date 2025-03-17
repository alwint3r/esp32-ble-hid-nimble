#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ble_keyboard_report {
  uint8_t modifier;
  uint8_t reserved;
  uint8_t keycode[6];
} __attribute__((packed)) ble_keyboard_report_t;

#ifdef __cplusplus
}
#endif
