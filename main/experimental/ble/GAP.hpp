#pragma once

#include <memory>

#include "Advertiser.hpp"

struct BLE;

struct GAP {
  virtual void init() = 0;
  virtual Advertiser* advertiser() = 0;
};
