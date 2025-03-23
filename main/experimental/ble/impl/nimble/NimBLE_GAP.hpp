#pragma once

#include "experimental/ble/GAP.hpp"

struct NimBLE_GAP : public GAP {
  void init() override;

  Advertiser* advertiser() override;

 private:
  std::unique_ptr<Advertiser> advertiser_{nullptr};
};
