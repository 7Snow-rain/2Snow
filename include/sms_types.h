/**
 * sms_types.h — 短信数据结构定义
 * 
 * 【SmsData 结构体】一条短信包含：
 *   - phoneNumber: 发送者手机号
 *   - date: 接收日期
 *   - time: 接收时间
 *   - content: 短信正文内容
 * 
 * 【WorkMode 枚举】工作模式：
 *   - MODE_OFFLINE: 收信模式（纯接收，不转发）
 *   - MODE_ONLINE:  转发模式（接收 + HTTP/短信转发）
 *   - MODE_CONFIG: 配置模式（进 Web 后台配 WiFi 等）
 * 
 * 【长短信】因为短信最长 140 字节（70 个汉字），
 *   长短信会拆成多个分段发。用 ConcatMessage/ConcatPart 来缓存拼装。
 *   MAX_CONCAT_MESSAGES=5 → 最多同时拼接 5 条长短信
 *   MAX_CONCAT_PARTS=10   → 每条最多 10 个分段
 *   CONCAT_TIMEOUT_MS=60000 → 超时 60 秒，超时没收全就放弃
 */

#ifndef SMS_TYPES_H
#define SMS_TYPES_H

#include <Arduino.h>

// 短信数据结构
struct SmsData {
  String phoneNumber;
  String date;
  String time;
  String content;
};

// 工作模式
enum WorkMode {
  MODE_OFFLINE,   // 收信模式
  MODE_ONLINE,    // 转发模式
  MODE_CONFIG     // 配置模式
};

// 长短信配置
#define MAX_CONCAT_MESSAGES 5   // 最多同时处理5条长短信
#define MAX_CONCAT_PARTS 10     // 每条长短信最多10个分段
#define CONCAT_TIMEOUT_MS 60000 // 长短信超时时间（60秒）

// 长短信分段结构
struct ConcatPart {
  bool valid;      // 该分段是否有效
  String text;     // 分段文本内容
};

// 长短信缓存结构
struct ConcatMessage {
  bool inUse;                        // 槽位是否在使用
  int refNumber;                     // 长短信参考号
  String sender;                     // 发送者号码
  String timestamp;                  // 时间戳
  int totalParts;                    // 总分段数
  int receivedParts;                 // 已接收分段数
  unsigned long firstPartTime;       // 第一个分段接收时间
  ConcatPart parts[MAX_CONCAT_PARTS]; // 各分段内容
};

#endif
