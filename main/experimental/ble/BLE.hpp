#pragma once

#include "Host.hpp"
#include "experimental/ble/GAP.hpp"

struct BLE {
  static BLE& instance() {
    static BLE instance;
    return instance;
  }

  Host* host();

  static GAP& gap() { return GAP::instance(); }

 private:
  std::unique_ptr<Host> host_{nullptr};

 private:
  BLE() = default;
  ~BLE() = default;
  BLE(const BLE&) = delete;
  BLE& operator=(const BLE&) = delete;
  BLE(BLE&&) = delete;
  BLE& operator=(BLE&&) = delete;
};
