// Copy this file to secrets.h and fill in your own values.
// secrets.h is listed in .gitignore and must never be committed.
#pragma once

// Raspberry Pi running the AutoCargo server
#define SECRET_SERVER_IP                "192.168.0.10"
#define SECRET_SERVER_PASSWORD          "change-me"   // must match DEVICE_SHARED_SECRET on the server

// WPA2 Personal (used when USE_WPA2_ENTERPRISE is false)
#define SECRET_WPA2_PERSONAL_SSID       "your-ssid"
#define SECRET_WPA2_PERSONAL_PASSWORD   "your-wifi-password"

// WPA2 Enterprise (used when USE_WPA2_ENTERPRISE is true)
#define SECRET_WPA2_ENTERPRISE_SSID     "your-enterprise-ssid"
#define SECRET_WPA2_ENTERPRISE_USERNAME "your-username"
#define SECRET_WPA2_ENTERPRISE_PASSWORD "your-password"

// Static network configuration of this ESP32 (comma-separated octets)
#define NET_LOCAL_IP  192, 168, 0, 100
#define NET_GATEWAY   192, 168, 0, 1
#define NET_SUBNET    255, 255, 255, 0
#define NET_DNS       1, 1, 1, 1
