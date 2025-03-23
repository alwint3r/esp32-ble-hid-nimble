#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

/**
 * @brief Currently incomplete
 *
 */
struct AdvertisementData {
  uint8_t flags{0};
  // these two can be vectorized, but for now let's keep it basic
  const uint16_t *uuids16{nullptr};
  uint8_t num_uuids16{0};

  const uint32_t *uuids32{nullptr};
  uint8_t num_uuids32{0};

  const uint8_t **uuids128{nullptr};
  uint8_t num_uuids128{0};

  std::string_view name{};

  std::optional<int8_t> tx_pwr_lvl{std::nullopt};

  std::optional<uint8_t> sec_oob_flag{std::nullopt};

  const uint8_t *svc_data_uuid16{nullptr};
  uint8_t svc_data_uuid16_len{0};

  std::optional<uint8_t> appearance{std::nullopt};

  const uint8_t *svc_data_uuid32{nullptr};
  uint8_t svc_data_uuid32_len{0};

  const uint8_t *svc_data_uuid128{nullptr};
  uint8_t svc_data_uuid128_len{0};

  const uint8_t *uri{nullptr};
  uint8_t uri_len{0};

  const uint8_t *mfg_data{nullptr};
  uint8_t mfg_data_len{0};
};
