#include "gap.h"

#include <string.h>

#include "ble_battery.h"
#include "ble_device_info.h"
#include "ble_hid.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char* TAG = "GAP";

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;

int gap_event_handler(struct ble_gap_event* event, void* arg);

int gap_init(const char* device_name) {
  ble_svc_gap_init();

  int rc = ble_svc_gap_device_name_set(device_name);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set device name, error code: %d", rc);
    return rc;
  }

  ble_svc_gatt_init();

  rc = ble_device_info_init();
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to initialize device info, error code: %d", rc);
    return rc;
  }

  rc = ble_battery_init();
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to initialize battery service, error code: %d", rc);
    return rc;
  }

  rc = ble_hid_init();
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to initialize HID service, error code: %d", rc);
    return rc;
  }

  return 0;
}

static void start_advertising(void) {
  // First set up advertising data fields
  struct ble_hs_adv_fields fields = {0};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

  // Set up appearance
  fields.appearance = 0x03C1;
  fields.appearance_is_present = 1;

  ble_uuid16_t services[] = {
      {
          .u =
              {
                  .type = BLE_UUID_TYPE_16,
              },
          .value = BLE_HID_SERVICE_UUID,
      },
  };

  fields.uuids16 = services;
  fields.num_uuids16 = sizeof(services) / sizeof(services[0]);
  fields.uuids16_is_complete = 1;

  // Configure advertising data
  int rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error setting advertisement data; rc=%d", rc);
    return;
  }

  // Set up scan response fields with the device name
  struct ble_hs_adv_fields rsp_fields = {0};
  rsp_fields.name = (uint8_t*)ble_svc_gap_device_name();
  rsp_fields.name_len = strlen(ble_svc_gap_device_name());
  rsp_fields.name_is_complete = 1;

  // Set scan response data
  rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
  if (rc != 0) {
    ESP_LOGE(TAG, "Error setting scan response data; rc=%d", rc);
    return;
  }

  // Start advertising
  struct ble_gap_adv_params adv_params = {
      .conn_mode = BLE_GAP_CONN_MODE_UND,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };

  rc = ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                         gap_event_handler, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
  } else {
    ESP_LOGI(TAG, "Advertising started");
  }
}

void adv_init() {
  int rc = 0;

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to ensure address, error code: %d", rc);
    return;
  }

  start_advertising();
}

int gap_event_handler(struct ble_gap_event* event, void* arg) {
  int rc = 0;
  struct ble_gap_conn_desc desc;
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "Connection established, status=%d", event->connect.status);
      if (event->connect.status == 0) {
        conn_handle = event->connect.conn_handle;
      }

      rc = ble_gap_security_initiate(conn_handle);
      if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initiate security, error code: %d", rc);
      } else {
        ESP_LOGI(TAG, "Security initiated");
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
      conn_handle = BLE_HS_CONN_HANDLE_NONE;
      adv_init();
      break;
    case BLE_GAP_EVENT_MTU:
      ESP_LOGI(TAG, "MTU exchange complete, MTU=%d", event->mtu.value);
      break;
    case BLE_GAP_EVENT_ENC_CHANGE:
      if (event->enc_change.status == 0) {
        ESP_LOGI(TAG, "Encryption established");
      } else {
        ESP_LOGE(TAG, "Encryption failed, status=%d", event->enc_change.status);
      }
      break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      if (rc != 0) {
        ESP_LOGE(TAG, "Failed to find connection, error code: %d", rc);
        return rc;
      }

      ble_store_util_delete_peer(&desc.peer_id_addr);
      ESP_LOGI(TAG, "Re-pairing...");
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    case BLE_GAP_EVENT_SUBSCRIBE:
      rc = ble_gap_conn_find(event->subscribe.conn_handle, &desc);
      if (rc != 0) {
        ESP_LOGE(TAG, "Failed to find connection, error code: %d", rc);
        return rc;
      }

      if (!desc.sec_state.encrypted) {
        ESP_LOGI(TAG, "Not encrypted, ignoring subscribe event");
      }
      break;
    default:
      ESP_LOGI(TAG, "Caught event: %d", event->type);
      break;
  }
  return 0;
}
