#pragma once

#include "experimental/ble/Advertiser.hpp"
#include "experimental/ble/data/Advertisement.hpp"

struct NimBLE_Advertiser : public Advertiser {
  void start() override;
  void stop() override;
  void setAdvertisementData(AdvertisementData& data) override;
};
