#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"

#define BLE_GAP_APPEARANCE_GENERIC_TAG 0x0200
#define BLE_GAP_URI_PREFIX_HTTPS 0x17
#define BLE_GAP_LE_ROLE_PERIPHERAL 0x00

static const char* DEVICE_NAME = "BLE KEYBOARD";
static const char* TAG = "BLE_KEYBOARD";

void ble_store_config_init(void);

static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};
static uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS,
                            '/',
                            '/',
                            'e',
                            's',
                            'p',
                            'r',
                            'e',
                            's',
                            's',
                            'i',
                            'f',
                            '.',
                            'c',
                            'o',
                            'm'};

int gap_init(void);
void start_advertising(void);
void adv_init(void);

inline static void format_addr(char* addr_str, uint8_t addr[]) {
  sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2],
          addr[3], addr[4], addr[5]);
}

// Called when host resets BLE stack due to errors
static void on_stack_reset(int reason) {
  /* On reset, print reset reason to console */
  ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

// Called when host is synced with the controller
static void on_stack_sync(void) {
  /* On stack sync, do advertising initialization */
  ESP_LOGI(TAG, "nimble stack synced");
  adv_init();
}

static void nimble_host_config_init(void) {
  /* Set host callbacks */
  ble_hs_cfg.reset_cb = on_stack_reset;
  ble_hs_cfg.sync_cb = on_stack_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  /* Store host configuration */
  ble_store_config_init();
}

static void nimble_host_task(void* param) {
  /* Task entry log */
  ESP_LOGI(TAG, "nimble host task has been started!");

  /* This function won't return until nimble_port_stop() is executed */
  nimble_port_run();

  /* Clean up at exit */
  vTaskDelete(NULL);
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_ERROR_CHECK(nimble_port_init());

  int rc = gap_init();
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to initialize GAP service, error code: %d", rc);
    return;
  }

  nimble_host_config_init();

  xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);
}

int gap_init(void) {
  int rc = 0;
  ble_svc_gap_init();

  rc = ble_svc_gap_device_name_set(DEVICE_NAME);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set device name, error code: %d", rc);
    return rc;
  }

  rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set device appearance, error code: %d", rc);
    return rc;
  }

  return rc;
}

void adv_init(void) {
  int rc = 0;
  char addr_str[18] = {0};

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to ensure address, error code: %d", rc);
    return;
  }

  rc = ble_hs_id_infer_auto(0, &own_addr_type);
  if (rc != 0) {
    ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
    return;
  }

  rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to copy address, error code: %d", rc);
    return;
  }

  format_addr(addr_str, addr_val);
  ESP_LOGI(TAG, "device address: %s", addr_str);

  start_advertising();
}

void start_advertising() {
  int rc = 0;
  const char* name;
  struct ble_hs_adv_fields adv_fields = {0};
  struct ble_hs_adv_fields rsp_fields = {0};
  struct ble_gap_adv_params adv_params = {0};

  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  name = ble_svc_gap_device_name();
  adv_fields.name = (uint8_t*)name;
  adv_fields.name_len = strlen(name);
  adv_fields.name_is_complete = 1;

  adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  adv_fields.tx_pwr_lvl_is_present = 1;

  adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
  adv_fields.appearance_is_present = 1;

  adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
  adv_fields.le_role_is_present = 1;

  rc = ble_gap_adv_set_fields(&adv_fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set advertising fields, error code: %d", rc);
    return;
  }

  rsp_fields.device_addr = addr_val;
  rsp_fields.device_addr_type = own_addr_type;
  rsp_fields.device_addr_is_present = 1;

  rsp_fields.uri = esp_uri;
  rsp_fields.uri_len = sizeof(esp_uri);

  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set scan response fields, error code: %d", rc);
    return;
  }

  adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL,
                         NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising, error code: %d", rc);
    return;
  }

  ESP_LOGI(TAG, "Advertising started successfully");
}
