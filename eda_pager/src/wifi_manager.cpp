#include "wifi_manager.h"

WiFiManager::WiFiManager() : connected(false) {
}

void WiFiManager::startAP(const char* ssid, const char* password) {
  // 使用 AP+STA 模式，这样可以同时提供配置界面和连接到外部网络
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid, password);
}

bool WiFiManager::connect(const String& ssid, const String& password) {
  if (ssid.length() == 0) {
    return false;
  }
  
  // 确保是 AP+STA 模式
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }
  
  connected = (WiFi.status() == WL_CONNECTED);
  return connected;
}

bool WiFiManager::isConnected() const {
  return connected && WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getAPIP() const {
  return WiFi.softAPIP().toString();
}

String WiFiManager::getSTAIP() const {
  return WiFi.localIP().toString();
}
