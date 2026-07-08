/**
 * wifi_manager.h — WiFi 管理类
 * 
 * 负责两种 WiFi 模式：
 *   - AP 模式：ESP32 自己开热点（默认 SSID: EDA-Pager），你连上去配网
 *   - STA 模式：ESP32 连你家路由器，用来上网（发 HTTP 请求等）
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>

// WiFi管理类
class WiFiManager {
public:
  WiFiManager();
  
  // 启动AP模式
  void startAP(const char* ssid, const char* password);
  
  // 连接到WiFi
  bool connect(const String& ssid, const String& password);
  
  // 检查是否已连接
  bool isConnected() const;
  
  // 获取AP IP地址
  String getAPIP() const;
  
  // 获取STA IP地址
  String getSTAIP() const;
  
private:
  bool connected;
};

#endif
