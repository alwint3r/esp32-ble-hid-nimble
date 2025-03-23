#pragma once

#include <memory>

#include "Advertiser.hpp"

struct BLE;

struct GAP {
  static GAP& instance() {
    static GAP instance;
    return instance;
  }

  Advertiser* advertiser();

 private:
  GAP() = default;
  ~GAP() = default;
  GAP(const GAP&) = delete;
  GAP& operator=(const GAP&) = delete;
  GAP(GAP&&) = delete;
  GAP& operator=(GAP&&) = delete;
  friend struct BLE;

 private:
  std::unique_ptr<Advertiser> advertiser_{nullptr};
};
