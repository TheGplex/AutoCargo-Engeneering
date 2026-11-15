#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include "esp_wpa2.h"
#include "esp_wifi.h"
#include "secrets.h"   // credentials and network settings, not in the repository

#define USE_WPA2_ENTERPRISE true // Set to true for WPA2 Enterprise, false for WPA2 Personal

const String espName = "ESP2";
const String serverIP = SECRET_SERVER_IP;  // RASPI IP
const String serverPassword = SECRET_SERVER_PASSWORD;
uint64_t startTime;

const char *WPA2_PERSONAL_SSID = SECRET_WPA2_PERSONAL_SSID;
const char *WPA2_PERSONAL_PASSWORD = SECRET_WPA2_PERSONAL_PASSWORD;

const char *WPA2_ENTERPRISE_SSID = SECRET_WPA2_ENTERPRISE_SSID;
const char *WPA2_ENTERPRISE_USERNAME = SECRET_WPA2_ENTERPRISE_USERNAME;
const String WPA2_ENTERPRISE_PASSWORD = SECRET_WPA2_ENTERPRISE_PASSWORD;

IPAddress local_IP(NET_LOCAL_IP);
IPAddress gateway(NET_GATEWAY);
IPAddress subnet(NET_SUBNET);
IPAddress dns(NET_DNS);
WebServer server(80);

String receivedMessage;

void enterWifi();
void enterWPA2Personal();
void enterWPA2Enterprise();
void handleReceive();
void sendMessage(String recipient, String message);

void setup() {
    Serial.begin(115200);
    enterWifi();

    server.on("/receive", HTTP_POST, handleReceive);
    server.begin();

    sendMessage("ESP1", "Hello from " + espName + "!");
}

void loop() {
    server.handleClient();
}

void enterWifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_STA);

    WiFi.config(local_IP, gateway, subnet, dns);

    if (USE_WPA2_ENTERPRISE) {
        Serial.println("Connecting to WPA2 Enterprise...");
        enterWPA2Enterprise();
    } else {
        Serial.println("Connecting to WPA2 Personal...");
        enterWPA2Personal();
    }

    while (WiFi.status() != WL_CONNECTED) {}

    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}


void enterWPA2Personal() {
    WiFi.begin(WPA2_PERSONAL_SSID, WPA2_PERSONAL_PASSWORD);
}

void enterWPA2Enterprise() {
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *) WPA2_ENTERPRISE_USERNAME, strlen(WPA2_ENTERPRISE_USERNAME));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *) WPA2_ENTERPRISE_USERNAME, strlen(WPA2_ENTERPRISE_USERNAME));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *) WPA2_ENTERPRISE_PASSWORD.c_str(), WPA2_ENTERPRISE_PASSWORD.length());
    esp_wifi_sta_wpa2_ent_enable();

    WiFi.begin(WPA2_ENTERPRISE_SSID);
}

void handleReceive() {
    String data = server.arg("plain");
    
    int firstSep = data.indexOf('&');
    int secondSep = data.indexOf('&', firstSep + 1);
    int thirdSep = data.indexOf('&', secondSep + 1);

    if (firstSep != -1 && secondSep != -1 && thirdSep != -1) {
        receivedMessage = data.substring(thirdSep + 1);
    } else {
        receivedMessage = "[Invalid Format]";
    }

    Serial.println("Nachricht: " + receivedMessage);
    server.send(200, "text/plain", "OK");
}


void sendMessage(const String recipient, const String message) {
    String protocol = recipient + "&" + espName + "&" + serverPassword + "&" + message;
    String httpBegin = "http://" + serverIP + "/receive";
    HTTPClient http;

    http.begin(httpBegin);

    http.addHeader("Content-Type", "text/plain");
    int httpResponseCode = http.POST(protocol);

    if (httpResponseCode > 0) {
        Serial.println("Message sent successfully!");
    } else {
        Serial.println("Error sending message: " + String(httpResponseCode));
    }

    http.end();
}