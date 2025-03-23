#include "NimBLE_Host.hpp"

#include <esp_log.h>

#include "host/ble_gap.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char* TAG = "NimBLE Host";

static void ble_host_task(void* data) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

static void ble_on_stack_sync(void);
static void ble_on_stack_reset(int reason);

extern "C" void ble_store_config_init(void);

void NimBLE_Host::init() {
  nimble_port_init();

  ble_hs_cfg.reset_cb = ble_on_stack_reset;
  ble_hs_cfg.sync_cb = ble_on_stack_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_store_config_init();
}

void NimBLE_Host::start() { nimble_port_freertos_init(ble_host_task); }

static void ble_on_stack_sync(void) { ESP_LOGI(TAG, "nimble stack synced"); }

static void ble_on_stack_reset(int reason) {
  ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}
