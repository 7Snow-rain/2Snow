/**
 * config_manager.h — 配置管理类
 * 
 * 功能：把用户设置（WiFi密码、转发规则等）存到 ESP32 的非易失存储中，
 *       断电后不会丢失（类似电脑的 BIOS 设置）。
 * 
 * 存的数据：
 *   - WiFi 的 SSID 和密码
 *   - HTTP 转发的方法（GET/POST）和 URL
 *   - 是否开启 HTTP 转发 / 短信转发
 *   - 短信转发的目标号码
 *   - 屏幕亮度、背光模式
 * 
 * 底层用的是 ESP32 的 Preferences 库（NVS 非易失存储）。
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

// 配置管理类
class ConfigManager {
public:
  ConfigManager();
  
  // 加载配置
  void load();
  
  // 保存配置
  void save();
  
  // 检查是否已配置
  bool isConfigured() const;
  
  // 设置配置完成标志
  void setConfigured(bool configured);
  
  // WiFi配置
  String wifiSSID;
  String wifiPassword;
  
  // HTTP配置
  String httpMethod;
  String httpUrl;
  bool httpForwardEnabled;
  // 短信转发配置
  bool smsForwardEnabled;
  String smsForwardNumber;
  
  // 显示配置
  int backlightBrightness;
  int backlightMode;  // 0=关闭, 1=常亮, 2=收到消息时开启
  
private:
  Preferences preferences;
};

#endif
