#include "ble_module.h"

#include "gap.h"
#include "host/ble_gap.h"
#include "host/ble_uuid.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

static const char* TAG = "BLE_MODULE";
static const char* DEVICE_NAME = "M5STICK-C";

void ble_store_config_init(void);
static void ble_host_task(void* param);
static void ble_on_stack_sync(void);
static void ble_on_stack_reset(int reason);

void ble_module_init(void) {
  ESP_ERROR_CHECK(nimble_port_init());
  int rc = gap_init(DEVICE_NAME);
  if (rc != 0) {
    ESP_LOGE(TAG, "Gap initialization failed, error code: %d", rc);
    return;
  }

  ble_hs_cfg.reset_cb = ble_on_stack_reset;
  ble_hs_cfg.sync_cb = ble_on_stack_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  ble_hs_cfg.sm_bonding = 1;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_mitm = 1;
  ble_hs_cfg.sm_our_key_dist |=
      BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
  ble_hs_cfg.sm_their_key_dist |=
      BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

  ble_store_config_init();

  nimble_port_freertos_init(ble_host_task);
}

static void ble_on_stack_sync(void) {
  ESP_LOGI(TAG, "nimble stack synced");
  adv_init();
}

static void ble_on_stack_reset(int reason) {
  ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void ble_host_task(void* param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}
