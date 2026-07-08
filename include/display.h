/**
 * display.h — 屏幕显示管理类
 * 
 * 负责在 128×64 像素的 ST7920 LCD 上显示内容。
 * 底层使用 U8G2 图形库驱动屏幕。
 * 
 * 主要功能：
 *   - 显示短信内容（自动滚动）
 *   - 显示等待界面（显示时间、模式）
 *   - 显示验证码（自动识别并放大显示）
 *   - 显示配置信息
 *   - 背光控制（常亮/随消息亮灭）
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <U8g2lib.h>
#include "sms_types.h"

// 显示管理类
class Display {
public:
  Display();
  
  // 初始化显示屏
  void begin();
  
  // 显示短信内容（带滚动）
  void showSms(const SmsData& sms);
  
  // 手动滚动控制
  void scrollLeft(int step = 10);   // 向左滚动（文本向左移动）
  void scrollRight(int step = 10);  // 向右滚动（文本向右移动）
  void enableAutoScroll();          // 启用自动滚动
  void disableAutoScroll();         // 禁用自动滚动
  bool isAutoScrollEnabled();       // 检查是否启用自动滚动
  
  // 显示等待消息
  void showWaiting(WorkMode mode, const String& date, const String& time);
  
  // 显示文本消息
  void showMessage(const String& line1, const String& line2 = "", const String& line3 = "");
  
  // 显示配置信息
  void showConfigInfo(const String& apIP, const String& staIP, bool wifiConnected);
  
  // 显示验证码
  void showVerificationCode(const String& code, const String& sender);
  
  // 背光控制
  void setBacklightMode(int mode);
  void updateBacklight();
  void turnOnBacklight();
  void turnOffBacklight();
  

  
private:
  U8G2_ST7920_128X64_F_SW_SPI u8g2;
  int scrollOffset;
  unsigned long lastScrollTime;
  int backlightBrightness;
  bool autoScrollEnabled;  // 自动滚动开关
  int maxScrollOffset;     // 最大滚动偏移量
  int backlightMode;       // 背光模式
  unsigned long backlightOnTime;  // 背光开启时间
  bool backlightOn;        // 背光状态
};

#endif
