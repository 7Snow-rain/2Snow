/**
 * lte_module.h — LTE 4G 模组通信类（核心模块）
 * 
 * 这是整个寻呼机最重要的模块，负责通过 AT 指令控制 ML307C 4G 模组。
 * ML307C 像是一个小手机，插 SIM 卡后能发短信、收短信、获取时间。
 * 
 * 主要功能：
 *   - 初始化模组（begin）
 *   - 发送 AT 指令并等待回复（sendATCommand）
 *   - 检查新消息（checkForNewMessage）
 *   - 获取未读列表（getUnreadMessageList）
 *   - 读取指定短信（readSms）
 *   - 同步时间（syncTime）
 *   - 发送短信（sendSms）
 *   - 处理长短信拼接（checkConcatTimeout / getConcletedConcatSms）
 *   - 解析各种编码格式（PDU、UCS2、7bit）
 * 
 * 短信有几种存储格式：
 *   - PDU 模式：最常见，短信内容用十六进制编码
 *   - 7bit 编码：英文短信的压缩格式
 *   - UCS2 编码：中文短信用 Unicode 编码
 */

#ifndef LTE_MODULE_H
#define LTE_MODULE_H

#include <Arduino.h>
#include "sms_types.h"

// LTE模块管理类
class LTEModule {
public:
  LTEModule();
  
  // 初始化LTE模块
  bool begin();
  
  // 发送AT指令并等待响应
  String sendATCommand(const char* command, unsigned long timeout = 2000);
  
  // 检查是否收到新消息通知，返回消息索引，-1表示没有收到
  int checkForNewMessage();
  
  // 获取未读消息列表
  bool getUnreadMessageList(int* indices, int maxCount, int& count);
  
  // 读取指定索引的短信
  SmsData readSms(int index);
  
  // 同步时间
  bool syncTime(String& date, String& time);
  
  // 发送短信（PDU模式）
  bool sendSms(const String& phoneNumber, const String& message);
  
  // 检查长短信超时
  void checkConcatTimeout();
  
  // 获取完整的长短信（如果已完成）
  bool getConcletedConcatSms(SmsData& sms);
  
private:
  String responseBuffer;
  String messageBuffer;
  
  // 长短信缓存
  ConcatMessage concatBuffer[MAX_CONCAT_MESSAGES];
  
  // 解析短信内容
  SmsData parseSmsContent(String response);
  
  // 解析PDU格式短信
  SmsData parsePduSms(String pduHex);
  
  // UCS2转UTF-8
  String ucs2ToUtf8(String ucs2Hex);
  
  // 解码7bit编码
  String decode7bit(String data, int length);
  
  // 解码8bit/UCS2编码
  String decodeUcs2(String data);
  
  // 从PDU中提取时间戳
  String decodePduTimestamp(String pduTime);
  
  // 从PDU中提取电话号码
  String decodePduPhoneNumber(String pduPhone, int length);
  
  // PDU编码相关函数
  String encodePduPhoneNumber(const String& phoneNumber);
  String encodeUcs2(const String& text);
  String encode7bit(const String& text);
  String buildPduSms(const String& phoneNumber, const String& message);
  
  // 长短信处理函数
  void initConcatBuffer();
  int findOrCreateConcatSlot(int refNumber, const String& sender, int totalParts);
  String assembleConcatSms(int slot);
  void clearConcatSlot(int slot);
  bool processConcatSms(const SmsData& sms, SmsData& completedSms);
  
  // 解析PDU格式的长短信头
  bool parseConcatHeader(const String& udh, int& refNumber, int& partNumber, int& totalParts);
};

#endif
