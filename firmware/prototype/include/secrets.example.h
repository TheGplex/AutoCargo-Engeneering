// Copy this file to secrets.h and fill in your own values.
// secrets.h is listed in .gitignore and must never be committed.
// The WPA2 Enterprise password is read at runtime from data/pw.key (SPIFFS),
// which is not in the repository either.
#pragma once

#define SECRET_SERVER_IP                "192.168.0.10"
#define SECRET_SERVER_PASSWORD          "change-me"

#define SECRET_WPA2_ENTERPRISE_SSID     "your-enterprise-ssid"
#define SECRET_WPA2_ENTERPRISE_USERNAME "your-username"

#define NET_LOCAL_IP  192, 168, 0, 100
#define NET_GATEWAY   192, 168, 0, 1
#define NET_SUBNET    255, 255, 255, 0
#define NET_DNS       1, 1, 1, 1
