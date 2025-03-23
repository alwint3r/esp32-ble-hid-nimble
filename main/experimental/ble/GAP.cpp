#include "GAP.hpp"

#include "impl/nimble/NimBLE_Advertiser.hpp"

Advertiser* GAP::advertiser() {
  if (!advertiser_) {
    advertiser_ = std::make_unique<NimBLE_Advertiser>();
  }
  return advertiser_.get();
}
