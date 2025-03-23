#include "BLE.hpp"

#include "experimental/ble/impl/nimble/NimBLE_Host.hpp"

Host* BLE::host() {
  if (!host_) {
    host_ = std::make_unique<NimBLE_Host>();
  }
  return host_.get();
}
