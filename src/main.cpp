/**
 * main.cpp — 程序主入口
 * 
 * 这是整个寻呼机的"大脑"，所有的模块在这里被组织起来协同工作。
 * 
 * 【程序启动流程】
 * setup() 函数按顺序做：
 *   1. 初始化按键引脚（设置为上拉输入）
 *   2. 加载之前保存的配置（WiFi密码等）
 *   3. 初始化屏幕和蜂鸣器
 *   4. 让用户选择工作模式（按左键→收信模式，按右键→转发模式）
 *   5. 如果是转发模式 → 启动WiFi + Web配置后台
 *   6. 初始化LTE 4G模组
 *   7. 检查SIM卡里有没有未读短信
 *   8. 播放开机音效，准备就绪
 * 
 * 【主循环 loop() 流程】
 * 一直在重复做：
 *   1. （转发模式）每5秒检查WiFi是否还连着
 *   2. 更新屏幕背光状态
 *   3. 每60秒同步一次时间（从基站获取）
 *   4. 检查长短信是否超时
 *   5. 检查有没有新短信 →
 *      - 有新短信 → 读取 → 合并同联系人 → 显示在屏幕
 *      - 如果是转发模式 → HTTP转发 / 短信转发
 *      - 自动识别验证码 → 放大显示
 *   6. 按键控制：左/右滚动，确定键清除消息
 *   7. 没消息时 → 显示等待界面（时间+模式）
 */

#include <Arduino.h>
#include "config.h"
#include "sms_types.h"
#include "lte_module.h"
#include "display.h"
#include "wifi_manager.h"
#include "config_manager.h"
#include "http_client.h"
#include "web_server.h"
#include "speaker.h"

// ============================================================
// 全局对象（在整个程序的生命周期都存在）
// ============================================================

Speaker speaker(BTN_SPK);        // 蜂鸣器，接在 BTN_SPK 引脚
LTEModule lteModule;              // 4G模组
Display display;                  // 屏幕
WiFiManager wifiManager;          // WiFi管理
ConfigManager configManager;      // 配置存储（断电不丢）
HttpClient httpClient;            // HTTP请求
WebServer* webServer = nullptr;   // Web后台（指针，因为转发模式才需要）

// ============================================================
// 全局状态变量
// ============================================================

WorkMode currentMode = MODE_OFFLINE;  // 当前工作模式，默认收信模式
SmsData currentSms;                     // 当前正在显示的短信
String currentDate = "";                // 当前日期（从基站同步）
String currentTime = "";                // 当前时间（从基站同步）
unsigned long lastTimeSync = 0;         // 上次时间同步的时间戳

// 消息合并相关（多条同一联系人的短信合并显示）
String lastSmsPhoneNumber = "";         // 上一条消息的联系人
unsigned long lastSmsTime = 0;          // 上一条消息的时间
const unsigned long SMS_MERGE_WINDOW = 3000;  // 3秒内的消息合并在一起
int messageIndices[10];  // 存等待合并的消息索引
int messageCount = 0;    // 等待合并的消息数量

// 短信历史记录（最多50条）
#define MAX_SMS_HISTORY 50
SmsData smsHistory[MAX_SMS_HISTORY];
int smsHistoryCount = 0;

// 验证码显示状态
bool showingVerificationCode = false;
String currentVerificationCode = "";

// ============================================================
// 辅助函数
// ============================================================

/**
 * 将一条短信添加到历史记录
 * 如果历史记录满了（50条），就丢掉最旧的那条
 */
void addToHistory(const SmsData& sms) {
  if (smsHistoryCount < MAX_SMS_HISTORY) {
    smsHistory[smsHistoryCount++] = sms;
  } else {
    // 历史记录满了，所有记录往前移动一位，新记录放最后
    for (int i = 0; i < MAX_SMS_HISTORY - 1; i++) {
      smsHistory[i] = smsHistory[i + 1];
    }
    smsHistory[MAX_SMS_HISTORY - 1] = sms;
  }
}

/**
 * 从短信内容中提取验证码
 * 验证码是4-8位连续的数字
 * 比如短信内容 "【淘宝】您的验证码是123456，5分钟内有效"
 * 就会提取出 "123456"
 */
String extractVerificationCode(const String& content) {
  String code = "";
  int consecutiveDigits = 0;
  int startPos = -1;
  
  for (int i = 0; i < content.length(); i++) {
    char c = content.charAt(i);
    
    if (c >= '0' && c <= '9') {
      if (consecutiveDigits == 0) {
        startPos = i;  // 记下数字开始的位置
      }
      consecutiveDigits++;
      
      // 找到4-8位连续数字 → 这就是验证码
      if (consecutiveDigits >= 4 && consecutiveDigits <= 8) {
        if (i + 1 < content.length()) {
          char nextChar = content.charAt(i + 1);
          if (nextChar >= '0' && nextChar <= '9') {
            // 后面还有数字，继续累积（但不能超过8位）
            if (consecutiveDigits < 8) {
              continue;
            } else {
              code = content.substring(startPos, startPos + 8);
              return code;
            }
          } else {
            code = content.substring(startPos, i + 1);
            return code;
          }
        } else {
          code = content.substring(startPos, i + 1);
          return code;
        }
      }
    } else {
      consecutiveDigits = 0;
      startPos = -1;
    }
  }
  return code;
}

/**
 * 从短信内容中提取括号里的内容作为服务商名称
 * 比如 "【淘宝】验证码..." → 返回 "淘宝"
 * 比如 "[京东]验证码..."  → 返回 "京东"
 */
String extractBracketContent(const String& content) {
  int start = content.indexOf("【");
  int end = content.indexOf("】");
  
  if (start != -1 && end != -1 && end > start) {
    return content.substring(start + 3, end);  // UTF-8中文"【"占3字节
  }
  
  start = content.indexOf("[");
  end = content.indexOf("]");
  
  if (start != -1 && end != -1 && end > start) {
    return content.substring(start + 1, end);
  }
  
  return "";
}

/**
 * 检测按键是否被按下
 * 按键按下时引脚为 LOW（因为接了上拉电阻）
 */
bool checkButton(int pin) {
  return digitalRead(pin) == LOW;
}

// ============================================================
// 核心功能函数
// ============================================================

/**
 * 检查并处理未读消息
 * 开机时调用，用户可以选择是否查看 SIM 卡里存着的未读短信
 */
void checkUnreadMessages() {
  const int MAX_UNREAD = 20;
  int unreadIndices[MAX_UNREAD];
  int unreadCount = 0;
  
  display.showMessage("检查未读消息...");
  delay(1000);
  
  if (lteModule.getUnreadMessageList(unreadIndices, MAX_UNREAD, unreadCount)) {
    display.showMessage("发现未读消息", String(unreadCount) + " 条", "按MID键阅读");
    
    // 等用户按确定键，10秒没按就跳过
    unsigned long waitStart = millis();
    bool shouldRead = false;
    while (millis() - waitStart < 10000) {
      if (checkButton(BTN_MID)) {
        shouldRead = true;
        delay(200);
        break;
      }
      if (checkButton(BTN_LEFT) || checkButton(BTN_RIGHT)) {
        display.showMessage("跳过未读消息");
        delay(1000);
        return;
      }
      delay(50);
    }
    
    if (!shouldRead) {
      display.showMessage("超时，跳过");
      delay(1000);
      return;
    }
    
    // 逐条显示未读消息
    for (int i = 0; i < unreadCount; i++) {
      display.showMessage("读取消息", String(i + 1) + "/" + String(unreadCount));
      delay(500);
      
      SmsData sms = lteModule.readSms(unreadIndices[i]);
      addToHistory(sms);
      
      display.enableAutoScroll();
      currentSms = sms;
      currentVerificationCode = "";
      showingVerificationCode = false;
      
      // 转发模式：把这条消息通过 HTTP 或 短信 转发出去
      if (currentMode == MODE_ONLINE) {
        if(configManager.httpForwardEnabled && configManager.httpUrl.length() > 0){
          httpClient.sendRequest(configManager.httpUrl, configManager.httpMethod, sms.content);
        }
        if (configManager.smsForwardEnabled && configManager.smsForwardNumber.length() > 0) {
          display.showMessage("转发短信中...");
          bool success = lteModule.sendSms(configManager.smsForwardNumber, sms.content);
          if (success) {
            display.showMessage("转发成功!");
          } else {
            display.showMessage("转发失败!");
          }
          delay(1000);
        }
      }
      
      // 显示消息，按键翻页
      bool nextMessage = false;
      while (!nextMessage) {
        display.showSms(sms);
        
        if (checkButton(BTN_MID)) {
          delay(200);
          if (i < unreadCount - 1) {
            display.showMessage("下一条消息...");
            delay(500);
          }
          nextMessage = true;
        }
        
        if (checkButton(BTN_LEFT)) {
          display.scrollLeft(10);
          delay(100);
        } else if (checkButton(BTN_RIGHT)) {
          display.scrollRight(10);
          delay(100);
        }
        delay(50);
      }
    }
    
    display.showMessage("未读消息已读完", "共 " + String(unreadCount) + " 条");
    delay(2000);
    currentSms.content = "";
  } else {
    display.showMessage("无未读消息");
    delay(1000);
  }
}

/**
 * 用户选择工作模式
 * 左键 → 收信模式（纯接收）
 * 右键 → 转发模式（接收 + 转发）
 */
WorkMode selectMode() {
  display.showMessage("EDA-Pager寻呼机", "选择模式:","         收信   转发");
  
  unsigned long startTime = millis();
  while (true) {
    if (checkButton(BTN_LEFT)) {
      display.showMessage("收信模式");
      delay(1000);
      return MODE_OFFLINE;
    } else if (checkButton(BTN_RIGHT)) {
      display.showMessage("转发模式");
      delay(1000);
      return MODE_ONLINE;
    }
    delay(50);
  }
}

/**
 * 初始化转发模式相关的功能
 * 包括启动 WiFi AP（热点）、连接 WiFi、启动 Web 配置后台
 */
void initOnlineMode() {
  if (!configManager.isConfigured()) {
    currentMode = MODE_CONFIG;
    display.showMessage("进入配置模式");
    delay(1000);
  }
  
  // 启动AP模式（ESP32当热点）
  wifiManager.startAP(AP_SSID, AP_PASSWORD);
  webServer->begin();
  
  // 如果有配过的WiFi，尝试连接
  if (configManager.wifiSSID.length() > 0) {
    display.showMessage("连接WiFi...");
    bool connected = wifiManager.connect(configManager.wifiSSID, configManager.wifiPassword);
    
    delay(500);
    if (connected) {
      display.showMessage("WiFi已连接", wifiManager.getSTAIP());
      webServer->setWiFiConnected(true);
    } else {
      display.showMessage("WiFi连接失败");
      webServer->setWiFiConnected(false);
    }
    delay(2000);
  }
  
  display.showConfigInfo(wifiManager.getAPIP(), wifiManager.getSTAIP(), wifiManager.isConnected());
  
  // 如果是配置模式（第一次开机），等待用户在 Web 后台完成配置
  if (currentMode == MODE_CONFIG) {
    display.showMessage("请访问:", wifiManager.isConnected() ? wifiManager.getSTAIP() : wifiManager.getAPIP(), "完成配置");
    while (!webServer->isConfigCompleted()) {
      delay(100);
    }
    display.showMessage("配置完成!", wifiManager.isConnected() ? "IP: " + wifiManager.getSTAIP() : "");
    delay(2000);
  } else {
    delay(3000);
  }
}

/**
 * 初始化 LTE 4G 模块
 * 一直重试直到连接成功
 */
bool initLTE() {
  display.showMessage("发送: AT");
  while (!lteModule.begin()) {
    display.showMessage("4G模组连接失败","正在尝试重连...");
    delay(5000);
  }
  display.showMessage("4G模组连接成功", "SIM卡配置成功");
  delay(1000);
  return true;
}

// ============================================================
// Arduino 标准入口：setup() 只执行一次，loop() 无限循环
// ============================================================

void setup() {
  // 初始化按键引脚（内部上拉，不按时为 HIGH，按下为 LOW）
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MID, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SPK, OUTPUT);
  digitalWrite(BTN_SPK, LOW);
  noTone(BTN_SPK); 

  // 加载之前存好的配置（WiFi密码等）
  configManager.load();
  
  // 初始化屏幕和蜂鸣器
  display.begin();
  speaker.begin();
  
  // 设置背光模式
  display.setBacklightMode(configManager.backlightMode);

  // 让用户选择工作模式
  currentMode = selectMode();
  
  // 如果是转发模式，启动 WiFi 和 Web 后台
  if (currentMode == MODE_ONLINE) {
    webServer = new WebServer(configManager, httpClient, display, smsHistory, &smsHistoryCount, speaker);
    initOnlineMode();
  }
  
  // 初始化 LTE 4G 模组（插SIM卡才能收短信）
  if (!initLTE()) {
    return;
  }
  
  // 检查 SIM 卡里有没有未读短信
  checkUnreadMessages();
  
  // 播放开机音效，提示准备就绪
  display.showMessage("初始化完成", currentMode == MODE_OFFLINE ? "收信模式" : "转发模式");
  speaker.speakStart();
  delay(1000);
}

void loop() {
  // 转发模式：每5秒检查一次WiFi是否还连着
  if (currentMode == MODE_ONLINE) {
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 5000) {
      bool isConnected = wifiManager.isConnected();
      webServer->setWiFiConnected(isConnected);
      lastWiFiCheck = millis();
    }
  }
  
  // 更新背光状态（比如消息已读后自动熄灭）
  display.updateBacklight();
  
  // 每60秒从基站同步一次时间
  if (millis() - lastTimeSync > TIME_SYNC_INTERVAL || lastTimeSync == 0) {
        if (lteModule.syncTime(currentDate, currentTime)) {
          lastTimeSync = millis();
        }
      }
  
  // 检查长短信是否超时（60秒没收全就放弃）
  lteModule.checkConcatTimeout();
  
  // 检查是否有拼接完成的长短信
  SmsData concatSms;
  if (lteModule.getConcletedConcatSms(concatSms)) {
    display.showMessage("长短信已完成!", concatSms.phoneNumber);
    delay(500);
    
    if (configManager.backlightMode == 2) {
      display.turnOnBacklight();
    }
    
    currentSms = concatSms;
    currentVerificationCode = "";
    showingVerificationCode = false;
    lastSmsPhoneNumber = concatSms.phoneNumber;
    lastSmsTime = millis();
    messageCount = 0;
    
    addToHistory(currentSms);
    
    display.enableAutoScroll();
    
    // 转发
    if (currentMode == MODE_ONLINE && configManager.httpForwardEnabled && configManager.httpUrl.length() > 0) {
      httpClient.sendRequest(configManager.httpUrl, configManager.httpMethod, currentSms.content);
    }
    if (currentMode == MODE_ONLINE && configManager.smsForwardEnabled && configManager.smsForwardNumber.length() > 0) {
      display.showMessage("转发短信中...");
      bool success = lteModule.sendSms(configManager.smsForwardNumber, currentSms.content);
      if (success) display.showMessage("转发成功!");
      else display.showMessage("转发失败!");
      delay(1000);
    }
  }
  
  // 检查有没有新短信到
  int newMessageIndex = lteModule.checkForNewMessage();
  
  if (newMessageIndex > 0) {
    unsigned long currentTime = millis();
    SmsData newSms = lteModule.readSms(newMessageIndex);
    
    // 长短信分段不直接显示，等拼接完成
    if (newSms.content.startsWith("[长短信分段")) {
      return;
    }
    
    // 同一联系人在3秒内的消息 → 合并显示
    bool shouldMerge = (lastSmsPhoneNumber == newSms.phoneNumber && 
                        (currentTime - lastSmsTime) <= SMS_MERGE_WINDOW &&
                        currentSms.content.length() > 0);
    
    if (shouldMerge) {
      currentSms.content += " | " + newSms.content;
      if (newSms.date.length() > 0) currentSms.date = newSms.date;
      if (newSms.time.length() > 0) currentSms.time = newSms.time;
      display.showMessage("消息已合并!", newSms.phoneNumber);
      delay(500);
    } else {
      // 新消息 → 显示通知，收集后续同联系人的消息
      display.showMessage("收到新消息!");
      delay(300);
      
      messageCount = 0;
      messageIndices[messageCount++] = newMessageIndex;
      
      // 等最多500ms，看还有没有同联系人发来的其他消息
      unsigned long waitStart = millis();
      while (millis() - waitStart < 500) {
        int additionalIndex = lteModule.checkForNewMessage();
        if (additionalIndex > 0) {
          SmsData additionalSms = lteModule.readSms(additionalIndex);
          if (additionalSms.phoneNumber == newSms.phoneNumber) {
            if (messageCount < 10) {
              messageIndices[messageCount++] = additionalIndex;
            }
            waitStart = millis();
          } else {
            break;
          }
        }
        delay(50);
      }
      
      // 排序后合并
      for (int i = 0; i < messageCount - 1; i++) {
        for (int j = i + 1; j < messageCount; j++) {
          if (messageIndices[i] > messageIndices[j]) {
            int temp = messageIndices[i];
            messageIndices[i] = messageIndices[j];
            messageIndices[j] = temp;
          }
        }
      }
      
      String mergedContent = "";
      for (int i = 0; i < messageCount; i++) {
        SmsData sms = lteModule.readSms(messageIndices[i]);
        if (mergedContent.length() > 0) mergedContent += " | ";
        mergedContent += sms.content;
        if (sms.date.length() > 0) newSms.date = sms.date;
        if (sms.time.length() > 0) newSms.time = sms.time;
      }
      
      newSms.content = mergedContent;
      currentSms = newSms;
      currentVerificationCode = "";
      showingVerificationCode = false;
      
      if (configManager.backlightMode == 2) {
        display.turnOnBacklight();
      }
      
      addToHistory(currentSms);
      
      if (messageCount > 1) {
        display.showMessage("长信息合并,共", String(messageCount) + " 条");
        delay(500);
      }
      
      messageCount = 0;
      display.enableAutoScroll();
    }
    
    lastSmsPhoneNumber = newSms.phoneNumber;
    lastSmsTime = currentTime;
    
    // 转发模式：转发消息
    if (currentMode == MODE_ONLINE && configManager.httpForwardEnabled && configManager.httpUrl.length() > 0) {
      httpClient.sendRequest(configManager.httpUrl, configManager.httpMethod, currentSms.content);
    }
    if (currentMode == MODE_ONLINE && configManager.smsForwardEnabled && configManager.smsForwardNumber.length() > 0) {
      display.showMessage("转发短信中...");
      bool success = lteModule.sendSms(configManager.smsForwardNumber, currentSms.content);
      if (success) display.showMessage("转发成功!");
      else display.showMessage("转发失败!");
      delay(1000);
    }
  }
  
  // 显示短信内容或等待界面
  if (currentSms.content.length() > 0) {
    if (!showingVerificationCode && currentVerificationCode.length() == 0) {
      currentVerificationCode = extractVerificationCode(currentSms.content);
      if (currentVerificationCode.length() >= 4 && currentVerificationCode.length() <= 8) {
        showingVerificationCode = true;
        speaker.speakNotify();
      }
    }
    
    if (showingVerificationCode) {
      String senderName = extractBracketContent(currentSms.content);
      if (senderName.length() == 0) {
        senderName = currentSms.phoneNumber;
      }
      display.showVerificationCode(currentVerificationCode, senderName);
      
      if (checkButton(BTN_MID)) {
        showingVerificationCode = false;
        display.enableAutoScroll();
        delay(200);
      }
    } else {
      if (currentVerificationCode.length() == 0) {
        static String lastDisplayedContent = "";
        if (lastDisplayedContent != currentSms.content) {
          speaker.speakNotify();
          lastDisplayedContent = currentSms.content;
        }
      }
      
      display.showSms(currentSms);
      
      if (checkButton(BTN_LEFT)) {
        display.scrollLeft(10);
        delay(100);
      } else if (checkButton(BTN_RIGHT)) {
        display.scrollRight(10);
        delay(100);
      } else if (checkButton(BTN_MID)) {
        currentSms.content = "";
        currentVerificationCode = "";
        lastSmsPhoneNumber = "";
        messageCount = 0;
        display.enableAutoScroll();
        
        if (configManager.backlightMode == 2) {
          display.turnOffBacklight();
        }
        display.showMessage("消息已读！");
        delay(1000);
      }
    }
  } else {
    display.showWaiting(currentMode, currentDate, currentTime);
  }
  
  delay(50);
}
