#include "BLE.hpp"

#include "experimental/ble/impl/nimble/NimBLE_Host.hpp"
#include "experimental/ble/impl/nimble/NimBLE_GAP.hpp"

Host* BLE::host() {
  if (!host_) {
    host_ = std::make_unique<NimBLE_Host>();
  }
  return host_.get();
}

GAP* BLE::gap() {
  if (!gap_) {
    gap_ = std::make_unique<NimBLE_GAP>();
  }
  return gap_.get();
}
