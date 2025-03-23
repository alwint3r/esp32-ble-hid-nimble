#pragma once

#include "experimental/ble/Host.hpp"

struct NimBLE_Host : public Host {
  void init() override;
  void start() override;
};
