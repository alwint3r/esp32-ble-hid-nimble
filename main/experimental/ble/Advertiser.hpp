#pragma once

#include "experimental/ble/data/Advertisement.hpp"

struct Advertiser {
  virtual void start() = 0;
  virtual void stop() = 0;

  virtual void setAdvertisementData(AdvertisementData& data) = 0;
};
