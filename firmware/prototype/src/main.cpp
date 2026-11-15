#include <WiFi.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include "esp_wpa2.h"
#include "esp_wifi.h"
#include <ArduinoJson.h>
#include "secrets.h"   // credentials and network settings, not in the repository


const char* ssid = SECRET_WPA2_ENTERPRISE_SSID;
const char* username = SECRET_WPA2_ENTERPRISE_USERNAME;  
const String serverPassword = SECRET_SERVER_PASSWORD;
String password;
uint64_t startTime;

IPAddress local_IP(NET_LOCAL_IP);
IPAddress gateway(NET_GATEWAY);
IPAddress subnet(NET_SUBNET);
IPAddress dns(NET_DNS);

void loadPassword();
void enterWifi();
void sendMsgToRasPi(const String& message);
void receiveMsgFromRasPi();

void setup() {
    Serial.begin(115200);
    enterWifi();
    startTime = 10000;
}

void loop() {
    if (millis() - startTime > 10000) {
        startTime = millis();
        sendMsgToRasPi("Dies ist eine Nachricht vom ESP32");
        receiveMsgFromRasPi();
    }
}

void loadPassword() {
  if (!SPIFFS.begin(true)) {
      Serial.println("SPIFFS konnte nicht gemountet werden!");
      return;
  }

  File file = SPIFFS.open("/pw.key", "r");
  if (!file) {
      Serial.println("Fehler beim Öffnen von pw.key!");
      return;
  }

  password = file.readStringUntil('\n');
  password.trim(); 
  file.close();
}

void enterWifi() {
  loadPassword();  

  if (password.length() == 0) {
      Serial.println("Passwort konnte nicht geladen werden!");
      return;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);

  WiFi.config(local_IP, gateway, subnet, dns);

  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)username, strlen(username));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)username, strlen(username));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)password.c_str(), password.length());
  esp_wifi_sta_wpa2_ent_enable();

  WiFi.begin(ssid);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
  }

  Serial.println("\nConnected to WiFi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void sendMsgToRasPi(const String& message) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;

        String serverUrl = "http://" SECRET_SERVER_IP ":5000/send_data"; // Angepasste URL
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");  // Header auf JSON setzen

        // JSON-Payload erstellen
        String payload = "{\"esp_id\": 1, \"message\": \"" + message + "\"}";

        int httpResponseCode = http.POST(payload);

        if (httpResponseCode > 0) {
            Serial.println("Nachricht erfolgreich gesendet");
        } else {
            Serial.println("Fehler beim Senden der Nachricht");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("WiFi nicht verbunden!");
    }
}

void receiveMsgFromRasPi() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String serverUrl = "http://" SECRET_SERVER_IP ":5000/send_message";


        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");  // Header setzen

        String payload = "{\"esp_id\": 1}";  // Beispiel-Daten für die Anfrage

        int httpResponseCode = http.POST(payload);  // POST statt GET verwenden

        if (httpResponseCode > 0) {
            String response = http.getString();

            // JSON parsen
            DynamicJsonDocument doc(1024);  // Erstelle ein JSON-Dokument
            DeserializationError error = deserializeJson(doc, response);  // JSON parsen

            if (error) {
                Serial.println("Fehler beim Parsen der JSON-Antwort");
            } else {
                // Zugriff auf JSON-Daten
                String messageContent = doc["message_content"].as<String>();
                Serial.print("Erhaltene Nachricht: ");
                Serial.println(messageContent);


                // Weitere Felder, die du senden willst
                if (doc.containsKey("esp_ip")) {
                    String espIp = doc["esp_ip"].as<String>();
                    Serial.print("ESP-IP: ");
                    Serial.println(espIp);
                }
            }
        } else {
            Serial.println("Fehler beim Empfangen der Nachricht");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        Serial.println("WiFi nicht verbunden!");
    }
}

