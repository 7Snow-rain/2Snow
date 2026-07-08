#include "display.h"
#include "config.h"

Display::Display() 
  : u8g2(U8G2_R0, LCD_CLK, LCD_MOSI, LCD_CS),
    scrollOffset(0),
    lastScrollTime(0),
    backlightBrightness(255),
    autoScrollEnabled(true),
    maxScrollOffset(0),
    backlightMode(1),  // 默认常亮
    backlightOnTime(0),
    backlightOn(false) {
}

void Display::begin() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  
  // 初始化背光引脚
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, LOW);  // 默认关闭
}

void Display::showSms(const SmsData& sms) {
  u8g2.clearBuffer();
  
  // 第一行：SMS标题和日期
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.setCursor(0, 8);
  u8g2.print("SMS:");
  
  if (sms.date.length() > 0 && sms.time.length() > 0) {
    String dateTimeStr = sms.date + " " + sms.time;
    int dateWidth = u8g2.getStrWidth(dateTimeStr.c_str());
    u8g2.setCursor(MAX_DISPLAY_WIDTH - dateWidth, 8);
    u8g2.print(dateTimeStr);
  }
  
  // 第二行：电话号码
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.setCursor(0, 20);
  u8g2.print(sms.phoneNumber);
  
  // 第三行：短信内容（滚动）
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  
  // 计算文本宽度（使用更保守的估算方法）
  int textWidth = u8g2.getStrWidth(sms.content.c_str());
  // 如果文本很长，使用字符数估算（中文字符约12像素宽）
  int estimatedWidth = sms.content.length() * 6;  // 保守估计每字符6像素
  if (estimatedWidth > textWidth) {
    textWidth = estimatedWidth;
  }
  
  // 更新最大滚动偏移量
  maxScrollOffset = textWidth + MAX_DISPLAY_WIDTH ;
  
  // 自动滚动模式
  if (autoScrollEnabled && millis() - lastScrollTime > SCROLL_DELAY) {
    scrollOffset += SCROLL_STEP;
    
    // 当文本完全滚出屏幕后重置（从右侧重新开始）
    if (scrollOffset > maxScrollOffset) {
      scrollOffset = 0;
    }
    lastScrollTime = millis();
  }
  
  // 限制手动滚动范围
  if (scrollOffset < 0) {
    scrollOffset = 0;
  }
  if (scrollOffset > maxScrollOffset) {
    scrollOffset = maxScrollOffset;
  }
  
  // 显示滚动文本（从右侧进入，向左滚动）
  int xPos = MAX_DISPLAY_WIDTH - scrollOffset;
  u8g2.setCursor(xPos, 30);
  u8g2.print(sms.content);
  
  // 显示滚动模式指示器
  if (!autoScrollEnabled) {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
    u8g2.setCursor(0, 63);
    u8g2.print("[M]");  // Manual mode indicator
  }
  
  u8g2.sendBuffer();
}

void Display::showWaiting(WorkMode mode, const String& date, const String& time) {
  u8g2.clearBuffer();
  
  if (date.length() > 0) {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
    u8g2.setCursor(0, 8);
    u8g2.print("EDA-Pager");
    u8g2.setCursor(65, 8);
    u8g2.print(date);
    
    u8g2.setFont(u8g2_font_10x20_mf);
    u8g2.setCursor(40,27);
    u8g2.print(time);
    
  } else {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
    u8g2.setCursor(0, 30);
    u8g2.print("等待新消息...");
  }
  
  u8g2.sendBuffer();
}

// 手动向左滚动（文本向左移动，显示后面的内容）
void Display::scrollLeft(int step) {
  autoScrollEnabled = false;  // 打断自动滚动
  scrollOffset += step;
  if (scrollOffset > maxScrollOffset) {
    scrollOffset = maxScrollOffset;
  }
}

// 手动向右滚动（文本向右移动，显示前面的内容）
void Display::scrollRight(int step) {
  autoScrollEnabled = false;  // 打断自动滚动
  scrollOffset -= step;
  if (scrollOffset < 0) {
    scrollOffset = 0;
  }
}

// 启用自动滚动
void Display::enableAutoScroll() {
  autoScrollEnabled = true;
  lastScrollTime = millis();  // 重置时间，避免立即跳动
}

// 禁用自动滚动
void Display::disableAutoScroll() {
  autoScrollEnabled = false;
}

// 检查是否启用自动滚动
bool Display::isAutoScrollEnabled() {
  return autoScrollEnabled;
}

void Display::showMessage(const String& line1, const String& line2, const String& line3) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  
  if (line1.length() > 0) {
    u8g2.setCursor(0, 10);
    u8g2.print(line1);
  }
  
  if (line2.length() > 0) {
    u8g2.setCursor(0, 20);
    u8g2.print(line2);
  }
  
  if (line3.length() > 0) {
    u8g2.setCursor(0, 30);
    u8g2.print(line3);
  }
  
  u8g2.sendBuffer();
}

void Display::showConfigInfo(const String& apIP, const String& staIP, bool wifiConnected) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  
  if (wifiConnected) {
    u8g2.setCursor(0, 10);
    u8g2.print("AP IP:");
    u8g2.setCursor(0, 20);
    u8g2.print(apIP);
  } else {
    u8g2.setCursor(0, 10);
    u8g2.print("AP: SMS-Pager");
    u8g2.setCursor(0, 20);
    u8g2.print("IP: ");
    u8g2.print(apIP);
    u8g2.setCursor(0, 30);
    u8g2.print("WiFi: Not Connected");
  }
  
  u8g2.sendBuffer();
}


void Display::showVerificationCode(const String& code, const String& sender) {
  u8g2.clearBuffer();
  
  // 第一行：标题和发送者
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.setCursor(0, 10);
  u8g2.print("验证码:");
  u8g2.setCursor(40, 10);
  u8g2.print(sender);
  
  // 第二行：验证码（大字体居中显示）
  u8g2.setFont(u8g2_font_10x20_mf);  // 大数字字体
  int codeWidth = u8g2.getStrWidth(code.c_str());
  int xPos = (MAX_DISPLAY_WIDTH - codeWidth) / 2;
  u8g2.setCursor(xPos, 27);
  u8g2.print(code);
  
  // 底部提示
  u8g2.setFont(u8g2_font_wqy12_t_gb2312b);
  u8g2.setCursor(30, 63);
  u8g2.print("按MID查看详情");
  
  u8g2.sendBuffer();
}

// 设置背光模式
void Display::setBacklightMode(int mode) {
  backlightMode = mode;
  updateBacklight();
}

// 更新背光状态
void Display::updateBacklight() {
  switch (backlightMode) {
    case 0:  // 关闭
      turnOffBacklight();
      break;
      
    case 1:  // 常亮
      turnOnBacklight();
      break;
      
    case 2:  // 收到消息时开启（消息已读时关闭）
      // 不在这里自动关闭，由main.cpp在消息已读时手动关闭
      break;
  }
}

// 开启背光
void Display::turnOnBacklight() {
  digitalWrite(LCD_BACKLIGHT, HIGH);
  backlightOn = true;
  backlightOnTime = millis();
}

// 关闭背光
void Display::turnOffBacklight() {
  digitalWrite(LCD_BACKLIGHT, LOW);
  backlightOn = false;
}
