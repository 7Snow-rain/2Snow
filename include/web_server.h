/**
 * web_server.h — Web 配置后台管理类
 * 
 * 连上寻呼机的 WiFi（EDA-Pager）后，浏览器打开 192.168.4.1 就能进后台。
 * 
 * 后台能配：
 *   - WiFi 配网
 *   - HTTP 转发地址（微信/邮箱推送）
 *   - 短信转发号码
 *   - 屏幕亮度、背光模式
 *   - 查看收件箱历史
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include "config_manager.h"
#include "http_client.h"
#include "display.h"
#include "sms_types.h"
#include "speaker.h"

// Web服务器类
class WebServer {
public:
  WebServer(ConfigManager& config, HttpClient& httpClient, Display& display, 
            SmsData* smsHistory, int* smsHistoryCount, Speaker& speaker);
  
  // 启动Web服务器
  void begin();
  
  // 检查配置是否完成
  bool isConfigCompleted() const { return configCompleted; }
  
  // 获取WiFi连接状态
  bool isWiFiConnected() const { return wifiConnected; }
  
  // 设置WiFi连接状态
  void setWiFiConnected(bool connected) { wifiConnected = connected; }
  
private:
  AsyncWebServer server;
  ConfigManager& configManager;
  HttpClient& httpClient;
  Display& display;
  Speaker& speaker;
  SmsData* smsHistory;
  int* smsHistoryCount;
  bool configCompleted;
  bool wifiConnected;
  
  void setupRoutes();
};

#endif
