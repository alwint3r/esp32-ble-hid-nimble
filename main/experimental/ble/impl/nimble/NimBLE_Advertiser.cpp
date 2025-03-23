
#include "NimBLE_Advertiser.hpp"

#include <vector>

#include "experimental/ble/Advertiser.hpp"
#include "experimental/ble/data/Advertisement.hpp"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static const char* TAG = "NimBLE Advertiser";

void NimBLE_Advertiser::start() {
  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to ensure address, error code: %d", rc);
    return;
  }

  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(BLE_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params,
                         nullptr, nullptr);
  if (rc != 0) {
    ESP_LOGE(TAG, "Failed to start advertising; rc=%d", rc);
  } else {
    ESP_LOGI(TAG, "Advertising started");
  }
}

void NimBLE_Advertiser::stop() { ble_gap_adv_stop(); }

void NimBLE_Advertiser::setAdvertisementData(AdvertisementData& data) {
  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  fields.flags = data.flags;

  std::vector<ble_uuid16_t> uuid16;
  if (data.uuids16) {
    uuid16.reserve(data.num_uuids16);

    for (size_t i = 0; i < (size_t)data.num_uuids16; i++) {
      ble_uuid16_t uuid{};
      uuid.u.type = BLE_UUID_TYPE_16;
      uuid.value = data.uuids16[i];

      uuid16.push_back(uuid);
    }

    fields.uuids16 = uuid16.data();
    fields.num_uuids16 = uuid16.size();
    fields.uuids16_is_complete = 1;
  }

  if (data.name.size() > 0) {
    fields.name = (uint8_t*)data.name.data();

    ble_svc_gap_device_name_set(data.name.data());
    fields.name_len = data.name.size();
    fields.name_is_complete = 1;
  }

  if (data.tx_pwr_lvl) {
    fields.tx_pwr_lvl = *data.tx_pwr_lvl;
    fields.tx_pwr_lvl_is_present = 1;
  }

  if (data.appearance) {
    fields.appearance = *data.appearance;
    fields.appearance_is_present = 1;
  }

  if (data.svc_data_uuid16) {
    fields.svc_data_uuid16 = data.svc_data_uuid16;
    fields.svc_data_uuid16_len = data.svc_data_uuid16_len;
  }

  if (data.svc_data_uuid32) {
    fields.svc_data_uuid32 = data.svc_data_uuid32;
    fields.svc_data_uuid32_len = data.svc_data_uuid32_len;
  }

  if (data.svc_data_uuid128) {
    fields.svc_data_uuid128 = data.svc_data_uuid128;
    fields.svc_data_uuid128_len = data.svc_data_uuid128_len;
  }

  if (data.uri) {
    fields.uri = data.uri;
    fields.uri_len = data.uri_len;
  }

  if (data.mfg_data) {
    fields.mfg_data = data.mfg_data;
    fields.mfg_data_len = data.mfg_data_len;
  }

  ble_gap_adv_set_fields(&fields);
}
