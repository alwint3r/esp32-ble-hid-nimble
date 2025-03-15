#include "ble_device_info.h"

#include <esp_log.h>
#include <string.h>

#include "host/ble_gap.h"
#include "host/ble_gatt.h"

static const char* TAG = "BLE_DEVICE_INFO";

static int manufacturer_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt* ctxt, void* arg);

static int pnp_id_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg);

static const struct ble_gatt_svc_def device_info_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_DEVICE_INFO_SERVICE_UUID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_DEVICE_MANUFACTURER_UUID),
                    .access_cb = &manufacturer_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_DEVICE_PNP_UUID),
                    .access_cb = &pnp_id_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {0},
            },
    },
    {0},
};

static const char* manufacturer_name = "X";
static const pnp_id_data_t pnp_id = {
    .vid_src = 0x02,
    .vid = 0xe502,
    .pid = 0xa111,
    .ver = 0x0210,
};

int ble_device_info_init(void) {
  int rc;
  rc = ble_gatts_count_cfg(device_info_defs);
  if (rc != 0) {
    return rc;
  }

  rc = ble_gatts_add_svcs(device_info_defs);
  if (rc != 0) {
    return rc;
  }

  return 0;
}

static int manufacturer_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt* ctxt, void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Reading manufacturer name (op=%d)", ctxt->op);
    int rc =
        os_mbuf_append(ctxt->om, manufacturer_name, strlen(manufacturer_name));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }

  ESP_LOGI(TAG, "Unexpected access to manufacturer name, opcode: %d", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

static int pnp_id_access(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt* ctxt, void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Reading PNP ID (op=%d)", ctxt->op);
    int rc = os_mbuf_append(ctxt->om, &pnp_id, sizeof(pnp_id));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  ESP_LOGI(TAG, "Unexpected access to PNP ID, opcode: %d", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}
