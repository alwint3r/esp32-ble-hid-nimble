#include "ble_hid.h"

#include <esp_log.h>
#include <string.h>

#include "ble_cccd.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"

static const char* TAG = "BLE_HID";

static int hid_info_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg);
static int hid_report_map_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt* ctxt, void* arg);
static int hid_control_point_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg);
static int hid_output_report_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg);
static int hid_input_report_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt* ctxt,
                                   void* arg);
static int hid_input_report_dsc_access(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt* ctxt,
                                       void* arg);
static int hid_output_report_dsc_access(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt* ctxt,
                                        void* arg);
static int hid_protocol_mode_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg);

static uint16_t input_report_chr_handle;

static const struct ble_gatt_svc_def hid_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BLE_HID_SERVICE_UUID),
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_HID_INFO_UUID),
                    .access_cb = &hid_info_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_HID_REPORT_MAP_UUID),
                    .access_cb = &hid_report_map_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_HID_CONTROL_POINT_UUID),
                    .access_cb = &hid_control_point_access,
                    .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {
                    .uuid = BLE_UUID16_DECLARE(
                        BLE_HID_REPORT_UUID),  // input report
                    .access_cb = &hid_input_report_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &input_report_chr_handle,
                    .arg = NULL,
                    .descriptors =
                        (struct ble_gatt_dsc_def[]){
                            {
                                .uuid = BLE_UUID16_DECLARE(
                                    BLE_REPORT_DESCRIPTOR_UUID),
                                .access_cb = &hid_input_report_dsc_access,
                                .att_flags = BLE_ATT_F_READ,
                                .arg = NULL,
                            },
                            {
                                .uuid = BLE_UUID16_DECLARE(
                                    BLE_CCCD_DESCRIPTOR_UUID),
                                .access_cb = &hid_input_report_dsc_access,
                                .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                                .arg = &input_report_chr_handle,
                            },
                            {0},
                        },
                },
                {
                    .uuid = BLE_UUID16_DECLARE(
                        BLE_HID_REPORT_UUID),  // output report
                    .access_cb = &hid_output_report_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = NULL,
                    .arg = NULL,
                    .descriptors =
                        (struct ble_gatt_dsc_def[]){
                            {
                                .uuid = BLE_UUID16_DECLARE(
                                    BLE_REPORT_DESCRIPTOR_UUID),
                                .access_cb = &hid_output_report_dsc_access,
                                .att_flags = BLE_ATT_F_READ,
                                .arg = NULL,
                            },
                            {0},
                        },
                },
                {
                    .uuid = BLE_UUID16_DECLARE(BLE_HID_PROTOCOL_MODE_UUID),
                    .access_cb = &hid_protocol_mode_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = NULL,
                    .arg = NULL,
                },
                {0},
            },
    },
    {0},
};

static const ble_hid_info_data_t hid_info = {
    .hid_version = {0x11, 0x01},  // HID version 1.1
    .country_code = 0x00,         // No country code
    .flags = 0x02,                // Remote wakeup and NDO supported
};

static const ble_hid_protocol_mode_t hid_protocol_mode =
    BLE_HID_PROTOCOL_MODE_REPORT;

static const uint8_t report_map[] = {
    // Interface
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)

    // Report ID (if multiple reports)
    0x85, BLE_HID_DEFAULT_REPORT_ID,  // Report ID (1)

    // Modifier Keys (Shift, Ctrl, Alt, GUI)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0xE0,  // Usage Minimum (224 = Left Control)
    0x29, 0xE7,  // Usage Maximum (231 = Right GUI)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x01,  // Logical Maximum (1)
    0x75, 0x01,  // Report Size (1 bit)
    0x95, 0x08,  // Report Count (8 bits)
    0x81, 0x02,  // Input (Data, Variable, Absolute)

    // Reserved Byte
    0x95, 0x01,  // Report Count (1)
    0x75, 0x08,  // Report Size (8)
    0x81, 0x01,  // Input (Constant)

    // LED Status (Num Lock, Caps Lock, etc)
    0x95, 0x05,  // Report Count (5)
    0x75, 0x01,  // Report Size (1)
    0x05, 0x08,  // Usage Page (LEDs)
    0x19, 0x01,  // Usage Minimum (1)
    0x29, 0x05,  // Usage Maximum (5)
    0x91, 0x02,  // Output (Data, Variable, Absolute)

    // LED Padding
    0x95, 0x01,  // Report Count (1)
    0x75, 0x03,  // Report Size (3)
    0x91, 0x01,  // Output (Constant)

    // Regular Keys
    0x95, 0x06,  // Report Count (6)
    0x75, 0x08,  // Report Size (8)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0x65,  // Logical Maximum (101)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0x65,  // Usage Maximum (101)
    0x81, 0x00,  // Input (Data, Array)

    0xC0  // End Collection
};

static ble_hid_report_descriptor_t input_descriptor = {
    .report_id = 0x01,
    .report_type = BLE_HID_REPORT_TYPE_INPUT,
};

static ble_hid_report_descriptor_t output_descriptor = {
    .report_id = 0x01,
    .report_type = BLE_HID_REPORT_TYPE_OUTPUT,
};

static ble_cccd_data_t input_report_cccd = {
    .ind_enabled = 0,
    .notif_enabled = 0,
    .reserved2 = 0,
    .reserved = 0,
};

int ble_hid_init(void) {
  int rc;
  rc = ble_gatts_count_cfg(hid_defs);
  if (rc != 0) {
    return rc;
  }

  rc = ble_gatts_add_svcs(hid_defs);
  if (rc != 0) {
    return rc;
  }

  ESP_LOGI(TAG, "Battery service initialized");
  return 0;
}

static int hid_info_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Accessing HID info (op=%d)", ctxt->op);
    int rc = os_mbuf_append(ctxt->om, &hid_info, sizeof(hid_info));
    if (rc != 0) {
      ESP_LOGE(TAG, "Failed to append HID info, error code: %d", rc);
      return rc;
    }
    ESP_LOGI(TAG, "HID info read successfully");
    return 0;
  }

  ESP_LOGE(TAG, "Invalid operation for HID info (op=%d)", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}
static int hid_report_map_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt* ctxt, void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Accessing HID report map (op=%d)", ctxt->op);
    int rc = os_mbuf_append(ctxt->om, report_map, sizeof(report_map));
    if (rc != 0) {
      ESP_LOGE(TAG, "Failed to append HID report map, error code: %d", rc);
      return rc;
    }
    ESP_LOGI(TAG, "HID report map read successfully");

    return 0;
  }
  ESP_LOGE(TAG, "Invalid operation for HID report map (op=%d)", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

static int hid_control_point_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    ESP_LOGI(TAG, "Accessing HID control point (op=%d)", ctxt->op);
    ESP_LOGI(TAG, "Writing HID control point (op=%d) %d bytes", ctxt->op,
             (int)ctxt->om->om_len);
    ESP_LOG_BUFFER_HEX(TAG, ctxt->om->om_data, ctxt->om->om_len);
    return 0;
  }
  return 0;
}
static int hid_input_report_access(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt* ctxt,
                                   void* arg) {
  return 0;
}

static int hid_input_report_dsc_access(uint16_t conn_handle,
                                       uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt* ctxt,
                                       void* arg) {
  const ble_uuid16_t* uuid = (const ble_uuid16_t*)ctxt->dsc->uuid;
  ESP_LOGI(TAG, "Accessing input report descriptor (op=%d)", ctxt->op);

  if (uuid->value == BLE_REPORT_DESCRIPTOR_UUID) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
      ESP_LOGI(TAG, "Reading input report descriptor (op=%d)", ctxt->op);
      int rc =
          os_mbuf_append(ctxt->om, &input_descriptor, sizeof(input_descriptor));
      if (rc != 0) {
        ESP_LOGE(TAG,
                 "Failed to append input report descriptor, error code: %d",
                 rc);
        return rc;
      }
      ESP_LOGI(TAG, "Input report descriptor read successfully");
      return 0;
    }
  }

  if (uuid->value == BLE_CCCD_DESCRIPTOR_UUID) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
      ESP_LOGI(TAG, "Reading input report cccd descriptor (op=%d)", ctxt->op);
      int rc = os_mbuf_append(ctxt->om, &input_report_cccd,
                              sizeof(input_report_cccd));
      if (rc != 0) {
        ESP_LOGE(
            TAG,
            "Failed to append input report cccd descriptor, error code: %d",
            rc);
        return rc;
      }
      ESP_LOGI(TAG, "Input report descriptor read successfully");
      return 0;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
      ESP_LOGI(TAG, "Writing input report descriptor (op=%d)", ctxt->op);
      struct os_mbuf* om = ctxt->om;
      int rc = os_mbuf_copydata(om, 0, sizeof(input_report_cccd),
                                &input_report_cccd);
      if (rc != 0) {
        ESP_LOGE(TAG, "Failed to copy data from om, error code: %d", rc);
        return rc;
      }
      ESP_LOGI(TAG, "Input report descriptor written successfully");
      return 0;
    }
  }

  ESP_LOGE(TAG,
           "Invalid operation for input report descriptor (op=%d, uuid=%04x)",
           ctxt->op, uuid->value);
  return BLE_ATT_ERR_UNLIKELY;
}

static int hid_output_report_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg) {
  ESP_LOGI(TAG, "Accessing output report (op=%d)", ctxt->op);
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    ESP_LOGI(TAG, "Writing output report (op=%d) %d bytes", ctxt->op,
             (int)ctxt->om->om_len);
    ESP_LOG_BUFFER_HEX(TAG, ctxt->om->om_data, ctxt->om->om_len);
    return 0;
  }

  ESP_LOGE(TAG, "Invalid operation for output report (op=%d)", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}

static int hid_output_report_dsc_access(uint16_t conn_handle,
                                        uint16_t attr_handle,
                                        struct ble_gatt_access_ctxt* ctxt,
                                        void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    ESP_LOGI(TAG, "Accessing output report descriptor (op=%d)", ctxt->op);
    const ble_uuid16_t* uuid = (const ble_uuid16_t*)ctxt->dsc->uuid;
    if (uuid->value == BLE_REPORT_DESCRIPTOR_UUID) {
      ESP_LOGI(TAG, "Reading output report descriptor (op=%d)", ctxt->op);
      int rc = os_mbuf_append(ctxt->om, &output_descriptor,
                              sizeof(output_descriptor));
      if (rc != 0) {
        ESP_LOGE(TAG,
                 "Failed to append output report descriptor, error code: %d",
                 rc);
        return rc;
      }
      ESP_LOGI(TAG, "Output report descriptor read successfully");
      return 0;
    }
  }

  ESP_LOGE(TAG, "Invalid operation for output report descriptor (op=%d)",
           ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}
static int hid_protocol_mode_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt* ctxt,
                                    void* arg) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    ESP_LOGI(TAG, "Accessing HID protocol mode (op=%d)", ctxt->op);
    int rc =
        os_mbuf_append(ctxt->om, &hid_protocol_mode, sizeof(hid_protocol_mode));
    if (rc != 0) {
      ESP_LOGE(TAG, "Failed to append HID protocol mode, error code: %d", rc);
      return rc;
    }
    ESP_LOGI(TAG, "HID protocol mode read successfully");
    return 0;
  }

  ESP_LOGE(TAG, "Invalid operation for HID protocol mode (op=%d)", ctxt->op);
  return BLE_ATT_ERR_UNLIKELY;
}
