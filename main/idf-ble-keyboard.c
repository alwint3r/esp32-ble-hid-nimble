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
#include "services/gatt/ble_svc_gatt.h"

static const char* DEVICE_NAME = "BLE KEYBOARD";
static const char* TAG = "BLE_KEYBOARD";

static const ble_uuid16_t hid_service_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x1812,
};

static const ble_uuid16_t hid_info_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2a4a,
};
static const ble_uuid16_t report_map_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2a4b,
};
static const ble_uuid16_t hid_control_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2a4c,
};
static const ble_uuid16_t report_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2a4d,
};
static const ble_uuid16_t protocol_mode_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2a4e,
};

static const ble_uuid16_t report_ref_uuid = {
    .u =
        {
            .type = BLE_UUID_TYPE_16,
        },
    .value = 0x2908,
};

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t input_report_handle;
static uint16_t hid_info_handle;
static uint16_t report_map_handle;
static uint16_t hid_control_handle;
static uint16_t protocol_mode_handle;

int gap_init(void);
void adv_init(void);
void ble_store_config_init(void);

static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};

static const uint8_t hid_info[] = {
    0x11, 0x01,  // HID version 1.11
    0x00,        // no country code
    0x01,        // remotely wakeable
};

// HID Report Descriptor for a basic keyboard
static const uint8_t report_map[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Keyboard)
    0x19, 0xE0,  // Usage Minimum (Left Control)
    0x29, 0xE7,  // Usage Maximum (Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1 bit)
    0x95, 0x08,  // Report Count (8)
    0x81, 0x02,  // Input (Data, Variable, Absolute) - Modifier byte
    0x95, 0x01,  // Report Count (1)
    0x75, 0x08,  // Report Size (8 bits)
    0x81, 0x01,  // Input (Constant) - Reserved byte
    0x95, 0x06,  // Report Count (6)
    0x75, 0x08,  // Report Size (8 bits)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x65,  // Logical Maximum (101)
    0x05, 0x07,  // Usage Page (Keyboard)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x65,  // Usage Maximum (101)
    0x81, 0x00,  // Input (Data, Array) - Key codes
    0xC0         // End Collection
};

static const uint8_t report_ref_input[] = {0x01, 0x01};  // Report ID 1, input

static const uint8_t protocol_mode = 0x01;  // report mode

static int generic_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt* ctxt, void* arg) {
  int rc = 0;
  if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    ESP_LOGE(TAG, "Connection handle is none");
    return BLE_ERR_UNK_CONN_ID;
  }
  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
      if (attr_handle == protocol_mode_handle) {
        rc = os_mbuf_append(ctxt->om, &protocol_mode, sizeof(protocol_mode));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      if (attr_handle == report_map_handle) {
        rc = os_mbuf_append(ctxt->om, report_map, sizeof(report_map));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      if (attr_handle == hid_info_handle) {
        rc = os_mbuf_append(ctxt->om, hid_info, sizeof(hid_info));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      break;
    case BLE_GATT_ACCESS_OP_READ_DSC:
      if (ctxt->dsc->uuid == &report_ref_uuid.u) {
        rc = os_mbuf_append(ctxt->om, report_ref_input,
                            sizeof(report_ref_input));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      break;
    default:
      ESP_LOGI(TAG, "unexpected access to generic_chr_access, opcode: %d",
               ctxt->op);
      return BLE_ATT_ERR_UNLIKELY;
  }

  return rc;
}

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &hid_service_uuid,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &hid_info_uuid.u,
                    .access_cb = generic_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &hid_info_handle,
                    .arg = NULL,
                },
                {
                    .uuid = &report_map_uuid.u,
                    .access_cb = generic_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &report_map_handle,
                    .arg = NULL,
                },
                {
                    .uuid = &hid_control_uuid.u,
                    .access_cb = generic_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &hid_control_handle,
                    .arg = NULL,
                },
                {
                    .uuid = &protocol_mode_uuid.u,
                    .access_cb = generic_chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                    .val_handle = &protocol_mode_handle,
                    .arg = NULL,
                },
                {
                    .uuid = &report_uuid.u,
                    .access_cb = generic_chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &input_report_handle,
                    .descriptors =
                        (struct ble_gatt_dsc_def[]){
                            {
                                .uuid = &report_ref_uuid.u,
                                .att_flags = BLE_ATT_F_READ,
                                .access_cb = generic_chr_access,
                                .arg = NULL,
                            },
                            {0},
                        },
                },
                {0},
            },
    },
    {0},
};

static void send_keypress(void) {
  if (conn_handle == BLE_HS_CONN_HANDLE_NONE) {
    ESP_LOGW(TAG, "Not connected, cannot send keypress");
    return;
  }

  uint8_t key_report[] = {0x00, 0x00, 0x04, 0x00,
                          0x00, 0x00, 0x00, 0x00};  // "A" (keycode 0x04)
  struct os_mbuf* om = ble_hs_mbuf_from_flat(key_report, sizeof(key_report));
  int rc = ble_gatts_notify_custom(conn_handle, input_report_handle, om);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to send keypress, error code: %d", rc);
  }

  vTaskDelay(pdMS_TO_TICKS(50));

  uint8_t release_report[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  om = ble_hs_mbuf_from_flat(release_report, sizeof(release_report));
  rc = ble_gatts_notify_custom(conn_handle, input_report_handle, om);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to send key release, error code: %d", rc);
  }
}

static int gap_event(struct ble_gap_event* event, void* arg) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "Connection established successfully, status=%d",
               event->connect.status);
      if (event->connect.status == 0) {
        conn_handle = event->connect.conn_handle;
      }
      break;

    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
      conn_handle = BLE_HS_CONN_HANDLE_NONE;
      // TODO: start advertise
      adv_init();
      break;

    case BLE_GAP_EVENT_SUBSCRIBE:
      ESP_LOGI(TAG, "Subscription event: attr_handle=%d, subscribed=%d",
               event->subscribe.attr_handle, event->subscribe.cur_notify);
    default:
      break;
  }

  return 0;
}

static void start_advertising(void) {
  struct ble_hs_adv_fields fields = {0};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids16 = (ble_uuid16_t[]){hid_service_uuid};
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;
  fields.name = (uint8_t*)DEVICE_NAME;
  fields.name_len = strlen(DEVICE_NAME);
  fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params adv_params = {
      .conn_mode = BLE_GAP_CONN_MODE_UND,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };

  int rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                             gap_event, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
  } else {
    ESP_LOGI(TAG, "Advertising started");
  }
}

static void ble_host_task(void* param) {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

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
  ble_gatts_count_cfg(gatt_svcs);
  ble_gatts_add_svcs(gatt_svcs);

  /* Store host configuration */
  ble_store_config_init();
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

  xTaskCreate(ble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

  while (true) {
    if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
      ESP_LOGI(TAG, "Sending keypress");
      send_keypress();
    }

    vTaskDelay(pdMS_TO_TICKS(3000));  // Send keypress every second
  }
}

int gap_init(void) {
  int rc = 0;
  ble_svc_gap_init();

  rc = ble_svc_gap_device_name_set(DEVICE_NAME);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set device name, error code: %d", rc);
    return rc;
  }

  ble_svc_gatt_init();
  rc = ble_gatts_count_cfg(gatt_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to count GATT services, error code: %d", rc);
    return rc;
  }

  rc = ble_gatts_add_svcs(gatt_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to add GATT services, error code: %d", rc);
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
