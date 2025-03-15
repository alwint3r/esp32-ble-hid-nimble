#include "ble_battery.h"

#include <esp_log.h>
#include <string.h>

#include "ble_cccd.h"
#include "ble_unit.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

static const char* TAG = "BLE_BATTERY";

static int battery_level_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt* ctxt, void* arg);

static int battery_level_dsc_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg);

static uint8_t cccd_desc_handle = 1;
static uint8_t unit_desc_handle = 2;

static const struct ble_gatt_svc_def device_info_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_BATTERY_SERVICE_UUID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {.uuid = BLE_UUID16_DECLARE(BLE_BATTERY_LEVEL_UUID),
                 .access_cb = &battery_level_access,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = NULL,
                 .arg = NULL,
                 .descriptors =
                     (struct ble_gatt_dsc_def[]){
                         {
                             .uuid =
                                 BLE_UUID16_DECLARE(BLE_CCCD_DESCRIPTOR_UUID),
                             .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                             .access_cb = battery_level_dsc_access,
                             .arg = &cccd_desc_handle,
                         },
                         {
                             .uuid =
                                 BLE_UUID16_DECLARE(BLE_UNIT_DESCRIPTOR_UUID),
                             .att_flags = BLE_ATT_F_READ,
                             .access_cb = battery_level_dsc_access,
                             .arg = &unit_desc_handle,
                         },
                         {0},
                     }},
                {0},
            },
    },
    {0},
};

static const uint8_t battery_level = 100;
static uint8_t battery_level_ccc_value = 0;
static const ble_unit_data_t battery_level_cpf = {
    .format = 0x04,  // uint8_t format
    .exponent = 0x00,
    .unit = BLE_UNIT_PERCENTAGE,
    .ns = 0x01,
    .description = 0x0000,
};

int ble_battery_init(void) {
  int rc;
  rc = ble_gatts_count_cfg(device_info_defs);
  if (rc != 0) {
    return rc;
  }

  rc = ble_gatts_add_svcs(device_info_defs);
  if (rc != 0) {
    return rc;
  }

  ESP_LOGI(TAG, "Battery service initialized");
  return 0;
}

static int battery_level_access(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt* ctxt, void* arg) {
  ESP_LOGI(TAG, "Accessing battery level (op=%d)", ctxt->op);
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Reading battery level (op=%d)", ctxt->op);
    int rc = os_mbuf_append(ctxt->om, &battery_level, sizeof(battery_level));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  ESP_LOGI(TAG, "Unexpected access to battery level, opcode: %d", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

static int battery_level_dsc_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg) {
  const ble_uuid16_t* uuid16 = (const ble_uuid16_t*)ctxt->dsc->uuid;
  if (uuid16->value == BLE_CCCD_DESCRIPTOR_UUID) {
    ESP_LOGI(TAG, "Accessing battery level CCC descriptor (op=%d)", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
      ESP_LOGI(TAG, "Reading battery level CCC descriptor");
      int rc = os_mbuf_append(ctxt->om, &battery_level_ccc_value,
                              sizeof(battery_level_ccc_value));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
      ESP_LOGI(TAG, "Writing battery level CCC descriptor");
      struct os_mbuf* om = ctxt->om;
      uint16_t ccc_value = 0;
      int rc = os_mbuf_copydata(om, 0, sizeof(ccc_value), &ccc_value);
      if (rc != 0) {
        ESP_LOGE(TAG, "Failed to copy data from om, error code: %d", rc);
        return BLE_ATT_ERR_UNLIKELY;
      }
      battery_level_ccc_value = ccc_value;
      ESP_LOGI(TAG, "Battery level CCC descriptor written, value: %04x",
               battery_level_ccc_value);
      return 0;
    }
  } else if (uuid16->value == BLE_UNIT_DESCRIPTOR_UUID) {
    ESP_LOGI(TAG, "Accessing battery level unit descriptor (op=%d)", ctxt->op);
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
      ESP_LOGI(TAG, "Reading battery level unit descriptor");
      int rc = os_mbuf_append(ctxt->om, &battery_level_cpf,
                              sizeof(battery_level_cpf));
      return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
  }

  ESP_LOGI(
      TAG,
      "Unexpected access to battery level descriptor, opcode: %d, uuid: %04x",
      ctxt->op, uuid16->value);
  return BLE_ATT_ERR_UNLIKELY;
}
