/**
 * config_manager.cpp — 配置管理类的实现
 * 
 * 把用户设置存到 ESP32 的 NVS（非易失存储）区域。
 * 数据存在 Flash 里，断电不会丢失。
 * 
 * 存储的"钥匙"是 "sms-config" 命名空间，
 * 每项配置有单独的 Key（比如 "ssid"、"password" 等）。
 * 
 * load() 和 save() 配对使用：
 *   - 开机时 load() → 把上次存的设置读出来
 *   - 配好网后 save() → 把新设置存进去
 */
#include "config_manager.h"

/**
 * 构造函数：设置所有配置项的默认值
 * wifiSSID/wifiPassword 默认空 → 需要用户配网
 * httpMethod 默认 GET
 * httpForwardEnabled/smsForwardEnabled 默认 false → 不转发
 * backlightBrightness 默认 255（最亮）
 * backlightMode 默认 1（常亮）
 */
ConfigManager::ConfigManager() 
  : wifiSSID(""),
    wifiPassword(""),
    httpMethod("GET"),
    httpUrl(""),
    httpForwardEnabled(false),
    smsForwardEnabled(false),
    smsForwardNumber(""),
    backlightBrightness(255),
    backlightMode(1) {
}

/**
 * 从 NVS 加载配置
 * begin("sms-config", false) → 以读写模式打开命名空间
 * getString/getBool/getInt → 按 key 读取，第二个参数是默认值
 * end() 一定要调用，否则可能丢数据
 */
void ConfigManager::load() {
  preferences.begin("sms-config", false);
  wifiSSID = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  httpMethod = preferences.getString("method", "GET");
  httpUrl = preferences.getString("url", "");
  httpForwardEnabled = preferences.getBool("httpFwdEnabled", false);
  smsForwardEnabled = preferences.getBool("fwdEnabled", false);
  smsForwardNumber = preferences.getString("fwdNumber", "");
  backlightBrightness = preferences.getInt("backlight", 255);
  backlightMode = preferences.getInt("backlightMode", 1);
  preferences.end();
}

/**
 * 保存配置到 NVS
 * putString/putBool/putInt → 按 key 写入
 * 和 load() 一一对应，存的 key 和读的 key 要相同
 */
void ConfigManager::save() {
  preferences.begin("sms-config", false);
  preferences.putString("ssid", wifiSSID);
  preferences.putString("password", wifiPassword);
  preferences.putString("method", httpMethod);
  preferences.putString("url", httpUrl);
  preferences.putBool("httpFwdEnabled", httpForwardEnabled);
  preferences.putBool("fwdEnabled", smsForwardEnabled);
  preferences.putString("fwdNumber", smsForwardNumber);
  preferences.putInt("backlight", backlightBrightness);
  preferences.putInt("backlightMode", backlightMode);
  preferences.end();
}

/**
 * 检查是否已完成配置
 * 三个条件：configured 标志为 true + WiFi名不为空 + 转发URL不为空
 */
bool ConfigManager::isConfigured() const {
  Preferences prefs;
  prefs.begin("sms-config", true);  // true = 只读模式
  bool configured = prefs.getBool("configured", false);
  prefs.end();
  return configured && wifiSSID.length() > 0 && httpUrl.length() > 0;
}

/**
 * 标记配置已完成
 * Web 后台配好后调用这个
 */
void ConfigManager::setConfigured(bool configured) {
  preferences.begin("sms-config", false);
  preferences.putBool("configured", configured);
  preferences.end();
}
