#pragma once

enum class WifiConnectResult {
    connected,
    missing_config,
    failed,
};

WifiConnectResult connect_wifi_station();
bool is_wifi_station_connected();
