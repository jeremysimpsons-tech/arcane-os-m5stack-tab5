#pragma once

#include <cstddef>
#include <cstdint>

/** Load saved creds (decrypted) into buffers; true if a profile exists. */
bool arc_wifi_get_saved(char *ssid, size_t ssid_len, char *pass, size_t pass_len);
/** Encrypted at rest in NVS (device-bound key). */
void arc_wifi_save(const char *ssid, const char *pass);
void arc_wifi_forget();
bool arc_wifi_is_saved(const char *ssid);
/** Throttled: updates cached “has internet” (TCP probe); safe from LVGL timer. */
void arc_net_poll();
/** Result of last `arc_net_probe_internet` / poll. */
bool arc_net_is_online();
void arc_net_invalidate();
