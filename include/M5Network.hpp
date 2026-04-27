/**
 * Project-level “M5.Network” facade for Tab5 hosted Wi-Fi.
 *
 * M5Unified doesn't expose `M5.Network` in C++ on the global `M5` object. The Tab5 Wi-Fi bridge is
 * provided by `Tab5M5Comms` (single TU that includes <WiFi.h>), and this facade keeps app code
 * protocol-agnostic and free of <WiFi.h>.
 */
#pragma once

#include "Tab5M5Comms.hpp"

struct M5Network_ {
    M5Comms_::WiFi_ &WiFi;
    explicit M5Network_(M5Comms_::WiFi_ &w) : WiFi(w) {}
    bool begin();
};

extern M5Network_ M5Network;

