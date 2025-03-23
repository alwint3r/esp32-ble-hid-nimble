#include "NimBLE_GAP.hpp"

#include "NimBLE_Advertiser.hpp"
#include "services/gap/ble_svc_gap.h"

void NimBLE_GAP::init() { ble_svc_gap_init(); }

Advertiser* NimBLE_GAP::advertiser() {
  if (!advertiser_) {
    advertiser_ = std::make_unique<NimBLE_Advertiser>();
  }

  return advertiser_.get();
}
