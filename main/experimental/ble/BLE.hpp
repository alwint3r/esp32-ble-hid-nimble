#pragma once

#include "Host.hpp"
#include "experimental/ble/GAP.hpp"

struct BLE {
  static BLE& instance() {
    static BLE instance;
    return instance;
  }

  Host* host();
  GAP* gap();

 private:
  std::unique_ptr<Host> host_{nullptr};
  std::unique_ptr<GAP> gap_{nullptr};

 private:
  BLE() = default;
  ~BLE() = default;
  BLE(const BLE&) = delete;
  BLE& operator=(const BLE&) = delete;
  BLE(BLE&&) = delete;
  BLE& operator=(BLE&&) = delete;
};
