#include "host/ble_gap.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// somehow needed this declaration because
// it was declared nowhere else
void ble_store_config_init(void);

// centralize the entry point to characteristics access
static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt* ctxt, void* arg);

static int batt_level_dsc_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt* ctxt, void* arg);

static int gap_event_handler(struct ble_gap_event* event, void* arg);

static void ble_host_task(void* param);

static void adv_init(void);

static void start_advertising(void);

static void ble_on_stack_sync(void);
static void ble_on_stack_reset(int reason);

static const char* TAG = "BLE_KEYBOARD_C";
static const char* DEVICE_NAME = "M5STICK-C";

typedef struct __ble2904_data {
  uint8_t format;
  uint8_t exponent;
  uint16_t unit;
  uint8_t namespace;
  uint16_t description;
} __attribute__((packed)) ble2904_data_t;

typedef struct __pnp_id_data {
  uint8_t vid_src;
  uint16_t vid;
  uint16_t pid;
  uint16_t ver;
} __attribute__((packed)) pnp_id_data_t;

static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t device_manuf_handle;
static uint16_t device_pnp_handle;
static uint16_t battery_level_handle;
static uint16_t battery_level_ccc_value = 0;
static ble2904_data_t battery_level_cpf = {
    .format = 0x04,  // uint8_t format
    .exponent = 0x00,
    .unit = 0x27AD,
    .namespace = 0x01,
    .description = 0x0000,
};

static pnp_id_data_t pnp_id = {
    .vid_src = 0x01,  // USB
    .vid = 0x303A,    // Espressif
    .pid = 0x1001,    // M5Stick-C
    .ver = 0x0100,
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180A),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A29),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &device_manuf_handle,
                    .arg = NULL,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(0x2A50),
                    .access_cb = chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &device_pnp_handle,
                    .arg = NULL,
                },
                {0},
            },
    },
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(0x180F),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {.uuid = BLE_UUID16_DECLARE(0x2A19),
                 .access_cb = chr_access,
                 .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                 .val_handle = &battery_level_handle,
                 .arg = NULL,
                 .descriptors =
                     (struct ble_gatt_dsc_def[]){
                         {
                             .uuid = BLE_UUID16_DECLARE(0x2902),
                             .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                             .access_cb = batt_level_dsc_access,
                             .arg = NULL,
                         },
                         {
                             .uuid = BLE_UUID16_DECLARE(0x2904),
                             .att_flags = BLE_ATT_F_READ,
                             .access_cb = batt_level_dsc_access,
                             .arg = NULL,
                         },
                         {0},
                     }},
                {0},
            },
    },
    {0},
};

void app_main() {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
      err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }

  ESP_ERROR_CHECK(err);

  ESP_ERROR_CHECK(nimble_port_init());
  ble_svc_gap_init();

  int rc = ble_svc_gap_device_name_set(DEVICE_NAME);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to set device name, error code: %d", rc);
    return;
  }

  ble_svc_gatt_init();
  rc = ble_gatts_count_cfg(gatt_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to count GATT services, error code: %d", rc);
    return;
  }

  rc = ble_gatts_add_svcs(gatt_svcs);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to add GATT services, error code: %d", rc);
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

static int chr_access(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt* ctxt, void* arg) {
  switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
      if (attr_handle == device_manuf_handle) {
        ESP_LOGI(TAG, "Reading device manufacturer (op=%d)", ctxt->op);
        const char* manufacturer = "Espressif";
        int rc = os_mbuf_append(ctxt->om, manufacturer, strlen(manufacturer));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      if (attr_handle == battery_level_handle) {
        ESP_LOGI(TAG, "Reading battery level (op=%d)", ctxt->op);
        uint8_t battery_level = 100;  // Example battery level
        int rc =
            os_mbuf_append(ctxt->om, &battery_level, sizeof(battery_level));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }

      if (attr_handle == device_pnp_handle) {
        ESP_LOGI(TAG, "Reading PNP ID (op=%d)", ctxt->op);
        int rc = os_mbuf_append(ctxt->om, &pnp_id, sizeof(pnp_id));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      break;
    default:
      ESP_LOGI(TAG, "Unexpected access to chr_access, opcode: %d", ctxt->op);
      return BLE_ATT_ERR_UNLIKELY;
  }
  return 0;
}

static int batt_level_dsc_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt* ctxt, void* arg) {
  const ble_uuid16_t* uuid16 = (const ble_uuid16_t*)ctxt->dsc->uuid;
  ESP_LOGI(TAG, "Accessing battery level descriptor (op=%d)", ctxt->op);
  ESP_LOGI(TAG, "Descriptor UUID: %04X", uuid16->value);

  switch (uuid16->value) {
    case 0x2902:
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
      } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        ESP_LOGI(TAG, "Reading battery level CCC descriptor");
        int rc = os_mbuf_append(ctxt->om, &battery_level_ccc_value,
                                sizeof(battery_level_ccc_value));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      }
      break;
    case 0x2904:
      if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        ESP_LOGI(TAG, "Reading battery level client characteristic config");
        int rc = os_mbuf_append(ctxt->om, &battery_level_cpf,
                                sizeof(battery_level_cpf));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
      } else {
        ESP_LOGI(TAG, "Unexpected access to battery level descriptor");
        return BLE_ATT_ERR_UNLIKELY;
      }
      break;
    default:
      break;
  }

  return 0;
}

static int gap_event_handler(struct ble_gap_event* event, void* arg) {
  int rc = 0;
  struct ble_gap_conn_desc desc;
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      ESP_LOGI(TAG, "Connection established, status=%d", event->connect.status);
      if (event->connect.status == 0) {
        conn_handle = event->connect.conn_handle;
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
        rc = ble_gap_security_initiate(conn_handle);
        if (rc != 0) {
          ESP_LOGE(TAG, "Failed to initiate security, error code: %d", rc);
        } else {
          ESP_LOGI(TAG, "Security initiated");
        }
      }
      break;
    default:
      ESP_LOGI(TAG, "Caught event: %d", event->type);
      break;
  }
  return 0;
}

static void adv_init() {
  int rc = 0;

  rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to ensure address, error code: %d", rc);
    return;
  }

  start_advertising();
}

static void start_advertising(void) {
  struct ble_hs_adv_fields fields = {0};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.name = (uint8_t*)DEVICE_NAME;
  fields.name_len = strlen(DEVICE_NAME);
  fields.name_is_complete = 1;

  ble_gap_adv_set_fields(&fields);

  struct ble_gap_adv_params adv_params = {
      .conn_mode = BLE_GAP_CONN_MODE_UND,
      .disc_mode = BLE_GAP_DISC_MODE_GEN,
  };

  int rc = ble_gap_adv_start(BLE_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params,
                             gap_event_handler, NULL);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
  } else {
    ESP_LOGI(TAG, "Advertising started");
  }
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
