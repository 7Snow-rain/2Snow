#include "lte_module.h"
#include "config.h"

LTEModule::LTEModule() {
  responseBuffer = "";
  messageBuffer = "";
  initConcatBuffer();
}

bool LTEModule::begin() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  // 测试AT指令
  String resp = sendATCommand("AT", AT_TIMEOUT);
  if (resp.indexOf("OK") < 0) {
    return false;
  }
  
  // 设置PDU模式
  resp = sendATCommand("AT+CMGF=0", AT_TIMEOUT);
  if (resp.indexOf("OK") < 0) {
    return false;
  }
  
  // 设置字符集为UCS2（支持中文）
  resp = sendATCommand("AT+CSCS=\"UCS2\"", AT_TIMEOUT);
  
  return true;
}

String LTEModule::sendATCommand(const char* command, unsigned long timeout) {
  String response = "";
  
  Serial.println(command);
  Serial.flush();
  
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      char c = Serial.read();
      response += c;
      
      if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
        break;
      }
    }
  }
  
  return response;
}

int LTEModule::checkForNewMessage() {
  while (Serial.available()) {
    char c = Serial.read();
    messageBuffer += c;
    
    int cmtiPos = messageBuffer.indexOf("+CMTI:");
    if (cmtiPos != -1) {
      int newlinePos = messageBuffer.indexOf('\n', cmtiPos);
      if (newlinePos != -1) {
        String cmtiLine = messageBuffer.substring(cmtiPos, newlinePos);
        messageBuffer = messageBuffer.substring(newlinePos + 1);
        
        int commaPos = cmtiLine.lastIndexOf(',');
        if (commaPos != -1) {
          String indexStr = cmtiLine.substring(commaPos + 1);
          indexStr.trim();
          
          int messageIndex = indexStr.toInt();
          if (messageIndex > 0) {
            return messageIndex;
          }
        }
      }
    }
    
    if (messageBuffer.length() > 512) {
      messageBuffer = messageBuffer.substring(messageBuffer.length() - 256);
    }
  }
  
  return -1;
}

bool LTEModule::getUnreadMessageList(int* indices, int maxCount, int& count) {
  count = 0;
  
  // 发送 AT+CMGL=0 列举未读短信
  String response = sendATCommand("AT+CMGL=0", 5000);
  
  if (response.indexOf("OK") < 0) {
    return false;
  }
  
  // 解析响应，提取消息索引
  // 格式: +CMGL: 35,0,,22
  int pos = 0;
  while (pos < response.length() && count < maxCount) {
    int cmglPos = response.indexOf("+CMGL:", pos);
    if (cmglPos == -1) {
      break;
    }
    
    // 找到行尾
    int lineEnd = response.indexOf('\n', cmglPos);
    if (lineEnd == -1) {
      break;
    }
    
    // 提取这一行
    String line = response.substring(cmglPos, lineEnd);
    
    // 解析索引号（第一个数字）
    // +CMGL: 35,0,,22
    int colonPos = line.indexOf(':');
    if (colonPos != -1) {
      int commaPos = line.indexOf(',', colonPos);
      if (commaPos != -1) {
        String indexStr = line.substring(colonPos + 1, commaPos);
        indexStr.trim();
        
        int index = indexStr.toInt();
        if (index > 0) {
          indices[count] = index;
          count++;
        }
      }
    }
    
    pos = lineEnd + 1;
  }
  
  return count > 0;
}

SmsData LTEModule::readSms(int index) {
  String cmgrCommand = "AT+CMGR=" + String(index);
  responseBuffer = sendATCommand(cmgrCommand.c_str(), 3000);
  responseBuffer.trim();
  
  if (responseBuffer.length() == 0) {
    SmsData sms;
    sms.content = "无响应";
    return sms;
  }
  
  return parseSmsContent(responseBuffer);
}

bool LTEModule::syncTime(String& date, String& time) {
  String response = sendATCommand("AT+CCLK?", AT_TIMEOUT);
  
  int startPos = response.indexOf("\"");
  int endPos = response.indexOf("\"", startPos + 1);
  
  if (startPos != -1 && endPos != -1) {
    String timeStr = response.substring(startPos + 1, endPos);
    
    int commaPos = timeStr.indexOf(',');
    if (commaPos != -1) {
      String datePart = timeStr.substring(0, commaPos);
      String timePart = timeStr.substring(commaPos + 1);
      
      int slash1 = datePart.indexOf('/');
      int slash2 = datePart.lastIndexOf('/');
      if (slash1 != -1 && slash2 != -1) {
        String year = datePart.substring(0, slash1);
        String month = datePart.substring(slash1 + 1, slash2);
        String day = datePart.substring(slash2 + 1);
        
        int yearNum = year.toInt();
        if (yearNum < 50) {
          yearNum += 2000;
        } else {
          yearNum += 1900;
        }
        
        date = String(yearNum) + "/" + month + "/" + day;
      }
      
      int colon1 = timePart.indexOf(':');
      int colon2 = timePart.indexOf(':', colon1 + 1);
      int hour = timePart.substring(0, colon1).toInt();
      int minute = timePart.substring(colon1 + 1, colon2).toInt();

      // +8 小时，处理跨天
      hour = (hour + 8) % 24;

      // 重新格式化
      char buf[6];
      sprintf(buf, "%02d:%02d", hour, minute);
      time = String(buf);

      
      return true;
    }
  }
  
  return false;
}

String LTEModule::ucs2ToUtf8(String ucs2Hex) {
  String result = "";
  ucs2Hex.trim();
  
  for (int i = 0; i < ucs2Hex.length(); i += 4) {
    if (i + 3 < ucs2Hex.length()) {
      String hexChar = ucs2Hex.substring(i, i + 4);
      unsigned int unicode = strtol(hexChar.c_str(), NULL, 16);
      
      if (unicode < 0x80) {
        result += (char)unicode;
      } else if (unicode < 0x800) {
        result += (char)(0xC0 | (unicode >> 6));
        result += (char)(0x80 | (unicode & 0x3F));
      } else {
        result += (char)(0xE0 | (unicode >> 12));
        result += (char)(0x80 | ((unicode >> 6) & 0x3F));
        result += (char)(0x80 | (unicode & 0x3F));
      }
    }
  }
  
  return result;
}

SmsData LTEModule::parseSmsContent(String response) {
  SmsData sms;
  
  // PDU模式响应格式：
  // +CMGR: <stat>,,<length>
  // <PDU数据>
  
  int cmgrPos = response.indexOf("+CMGR:");
  if (cmgrPos == -1) {
    sms.content = response;
    return sms;
  }
  
  // 查找PDU数据行
  int firstNewline = response.indexOf('\n', cmgrPos);
  if (firstNewline == -1) {
    sms.content = response;
    return sms;
  }
  
  // 提取PDU十六进制字符串
  int secondNewline = response.indexOf('\n', firstNewline + 1);
  String pduHex;
  if (secondNewline == -1) {
    pduHex = response.substring(firstNewline + 1);
  } else {
    pduHex = response.substring(firstNewline + 1, secondNewline);
  }
  
  pduHex.trim();
  
  // 解析PDU
  return parsePduSms(pduHex);
}


// ==================== PDU解析函数 ====================

// 解码PDU时间戳
String LTEModule::decodePduTimestamp(String pduTime) {
  // PDU时间格式：YY MM DD HH MM SS TZ (每个2位，交换顺序)
  // 例如：12 80 13 90 44 60 23 -> 21/08/31 09:44:06
  
  if (pduTime.length() < 14) return "";
  
  String year = String((pduTime.charAt(1) - '0') * 10 + (pduTime.charAt(0) - '0'));
  String month = String((pduTime.charAt(3) - '0') * 10 + (pduTime.charAt(2) - '0'));
  String day = String((pduTime.charAt(5) - '0') * 10 + (pduTime.charAt(4) - '0'));
  String hour = String((pduTime.charAt(7) - '0') * 10 + (pduTime.charAt(6) - '0'));
  String minute = String((pduTime.charAt(9) - '0') * 10 + (pduTime.charAt(8) - '0'));
  String second = String((pduTime.charAt(11) - '0') * 10 + (pduTime.charAt(10) - '0'));
  
  int yearNum = year.toInt();
  if (yearNum < 50) {
    yearNum += 2000;
  } else {
    yearNum += 1900;
  }
  
  String date = String(yearNum) + "/" + month + "/" + day;
  String time = hour + ":" + minute;
  
  return date + "," + time;
}

// 解码PDU电话号码
String LTEModule::decodePduPhoneNumber(String pduPhone, int length) {
  // 电话号码格式：长度交换，F填充
  // 例如：91 68 81 88 23 48 96 F5 -> +8618812384965
  
  if (pduPhone.length() < 2) return "";
  
  String result = "";
  int typeOfAddress = ((pduPhone.charAt(0) - '0') << 4) | (pduPhone.charAt(1) - '0');
  
  // 91表示国际格式
  if (typeOfAddress == 0x91) {
    result = "+";
  }
  
  // 跳过类型字节，处理号码
  for (int i = 2; i < pduPhone.length(); i += 2) {
    if (i + 1 < pduPhone.length()) {
      char c1 = pduPhone.charAt(i + 1);
      char c2 = pduPhone.charAt(i);
      
      if (c1 != 'F' && c1 != 'f') {
        result += c1;
      }
      if (c2 != 'F' && c2 != 'f') {
        result += c2;
      }
    }
  }
  
  return result;
}

// 解码7bit编码
String LTEModule::decode7bit(String data, int length) {
  // 7bit LTE编码解码
  // 每7个字节包含8个字符
  String result = "";
  
  // 将十六进制字符串转换为字节数组
  int byteCount = data.length() / 2;
  if (byteCount == 0) return result;
  
  uint8_t* bytes = new uint8_t[byteCount];
  for (int i = 0; i < byteCount; i++) {
    if (i * 2 + 1 < data.length()) {
      char hex[3] = {data.charAt(i * 2), data.charAt(i * 2 + 1), 0};
      bytes[i] = strtol(hex, NULL, 16);
    }
  }
  
  // 7bit解码
  int bitOffset = 0;
  for (int i = 0; i < length && i < byteCount * 8 / 7; i++) {
    int byteIndex = (i * 7) / 8;
    int bitShift = (i * 7) % 8;
    
    if (byteIndex < byteCount) {
      uint8_t char7bit = (bytes[byteIndex] >> bitShift) & 0x7F;
      
      // 如果需要从下一个字节获取高位
      if (bitShift > 1 && byteIndex + 1 < byteCount) {
        char7bit |= (bytes[byteIndex + 1] << (8 - bitShift)) & 0x7F;
      }
      
      // LTE 7bit字符集映射（简化版，仅处理基本ASCII）
      if (char7bit >= 32 && char7bit < 127) {
        result += (char)char7bit;
      } else if (char7bit == 0x0D) {
        result += '\n';  // 回车
      } else if (char7bit == 0x1B) {
        // 转义字符，下一个字符需要特殊处理
        // 这里简化处理，跳过
        continue;
      }
    }
  }
  
  delete[] bytes;
  return result;
}

// 解码UCS2编码
String LTEModule::decodeUcs2(String data) {
  return ucs2ToUtf8(data);
}

// 解析PDU格式短信
SmsData LTEModule::parsePduSms(String pduHex) {
  SmsData sms;
  
  if (pduHex.length() < 20) {
    sms.content = "PDU数据太短";
    return sms;
  }
  
  int pos = 0;
  
  // 1. SMSC长度（1字节）
  String smscLenStr = pduHex.substring(pos, pos + 2);
  int smscLen = strtol(smscLenStr.c_str(), NULL, 16);
  pos += 2;
  
  // 2. 跳过SMSC地址
  pos += smscLen * 2;
  
  if (pos >= pduHex.length()) {
    sms.content = "PDU格式错误";
    return sms;
  }
  
  // 3. PDU类型（1字节）
  String pduTypeStr = pduHex.substring(pos, pos + 2);
  int pduType = strtol(pduTypeStr.c_str(), NULL, 16);
  pos += 2;
  
  bool hasUDHI = (pduType & 0x40) != 0;  // 是否有用户数据头（长短信标志）
  
  // 4. 发送者地址长度（1字节）
  String senderLenStr = pduHex.substring(pos, pos + 2);
  int senderLen = strtol(senderLenStr.c_str(), NULL, 16);
  pos += 2;
  
  // 5. 发送者地址类型（1字节）+ 号码
  int senderOctets = (senderLen + 1) / 2;
  String senderPdu = pduHex.substring(pos, pos + 2 + senderOctets * 2);
  sms.phoneNumber = decodePduPhoneNumber(senderPdu, senderLen);
  pos += 2 + senderOctets * 2;
  
  // 6. 协议标识（1字节）
  pos += 2;
  
  // 7. 数据编码方案（1字节）
  String dcsStr = pduHex.substring(pos, pos + 2);
  int dcs = strtol(dcsStr.c_str(), NULL, 16);
  pos += 2;
  
  // 8. 时间戳（7字节，14个十六进制字符）
  String timestamp = pduHex.substring(pos, pos + 14);
  String dateTime = decodePduTimestamp(timestamp);
  int commaPos = dateTime.indexOf(',');
  if (commaPos != -1) {
    sms.date = dateTime.substring(0, commaPos);
    sms.time = dateTime.substring(commaPos + 1);
  }
  pos += 14;
  
  // 9. 用户数据长度（1字节）
  String udlStr = pduHex.substring(pos, pos + 2);
  int udl = strtol(udlStr.c_str(), NULL, 16);
  pos += 2;
  
  // 10. 用户数据
  String userData = pduHex.substring(pos);
  
  // 处理用户数据头（长短信）
  int refNumber = -1;
  int partNumber = -1;
  int totalParts = -1;
  int contentStart = 0;
  
  if (hasUDHI && userData.length() >= 2) {
    // UDHL（用户数据头长度）
    String udhlStr = userData.substring(0, 2);
    int udhl = strtol(udhlStr.c_str(), NULL, 16);
    contentStart = 2 + udhl * 2;
    
    // 解析UDH
    int udhPos = 2;
    while (udhPos < contentStart) {
      String ieiStr = userData.substring(udhPos, udhPos + 2);
      int iei = strtol(ieiStr.c_str(), NULL, 16);
      udhPos += 2;
      
      String iedlStr = userData.substring(udhPos, udhPos + 2);
      int iedl = strtol(iedlStr.c_str(), NULL, 16);
      udhPos += 2;
      
      // IEI = 0x00 或 0x08 表示长短信
      if (iei == 0x00 || iei == 0x08) {
        if (iei == 0x00 && iedl == 3) {
          // 8位参考号
          String refStr = userData.substring(udhPos, udhPos + 2);
          refNumber = strtol(refStr.c_str(), NULL, 16);
          
          String totalStr = userData.substring(udhPos + 2, udhPos + 4);
          totalParts = strtol(totalStr.c_str(), NULL, 16);
          
          String partStr = userData.substring(udhPos + 4, udhPos + 6);
          partNumber = strtol(partStr.c_str(), NULL, 16);
        } else if (iei == 0x08 && iedl == 4) {
          // 16位参考号
          String refStr = userData.substring(udhPos, udhPos + 4);
          refNumber = strtol(refStr.c_str(), NULL, 16);
          
          String totalStr = userData.substring(udhPos + 4, udhPos + 6);
          totalParts = strtol(totalStr.c_str(), NULL, 16);
          
          String partStr = userData.substring(udhPos + 6, udhPos + 8);
          partNumber = strtol(partStr.c_str(), NULL, 16);
        }
      }
      
      udhPos += iedl * 2;
    }
  }
  
  // 提取实际内容
  String content = userData.substring(contentStart);
  
  // 根据编码方案解码
  if ((dcs & 0x0C) == 0x08) {
    // UCS2编码
    sms.content = decodeUcs2(content);
  } else if ((dcs & 0x0C) == 0x00) {
    // 7bit编码
    sms.content = decode7bit(content, udl);
  } else {
    // 8bit编码或其他
    sms.content = content;
  }
  
  // 处理长短信
  if (refNumber != -1 && partNumber != -1 && totalParts != -1) {
    int slot = findOrCreateConcatSlot(refNumber, sms.phoneNumber, totalParts);
    
    // 保存分段（partNumber从1开始）
    if (partNumber >= 1 && partNumber <= totalParts && partNumber <= MAX_CONCAT_PARTS) {
      int index = partNumber - 1;
      if (!concatBuffer[slot].parts[index].valid) {
        concatBuffer[slot].parts[index].valid = true;
        concatBuffer[slot].parts[index].text = sms.content;
        concatBuffer[slot].receivedParts++;
        
        // 保存时间戳
        if (concatBuffer[slot].timestamp.length() == 0) {
          concatBuffer[slot].timestamp = sms.date + "," + sms.time;
        }
      }
    }
    
    // 标记为长短信分段（让主循环知道不要立即显示）
    sms.content = "[长短信分段" + String(partNumber) + "/" + String(totalParts) + "]";
  }
  
  return sms;
}

// ==================== 长短信处理函数 ====================

// 初始化长短信缓存
void LTEModule::initConcatBuffer() {
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    concatBuffer[i].inUse = false;
    concatBuffer[i].receivedParts = 0;
    for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
      concatBuffer[i].parts[j].valid = false;
      concatBuffer[i].parts[j].text = "";
    }
  }
}

// 查找或创建长短信缓存槽位
int LTEModule::findOrCreateConcatSlot(int refNumber, const String& sender, int totalParts) {
  // 先查找是否已存在
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].inUse && 
        concatBuffer[i].refNumber == refNumber &&
        concatBuffer[i].sender.equals(sender)) {
      return i;
    }
  }
  
  // 查找空闲槽位
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (!concatBuffer[i].inUse) {
      concatBuffer[i].inUse = true;
      concatBuffer[i].refNumber = refNumber;
      concatBuffer[i].sender = sender;
      concatBuffer[i].totalParts = totalParts;
      concatBuffer[i].receivedParts = 0;
      concatBuffer[i].firstPartTime = millis();
      for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
        concatBuffer[i].parts[j].valid = false;
        concatBuffer[i].parts[j].text = "";
      }
      return i;
    }
  }
  
  // 没有空闲槽位，查找最老的槽位覆盖
  int oldestSlot = 0;
  unsigned long oldestTime = concatBuffer[0].firstPartTime;
  for (int i = 1; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].firstPartTime < oldestTime) {
      oldestTime = concatBuffer[i].firstPartTime;
      oldestSlot = i;
    }
  }
  
  // 覆盖最老的槽位
  concatBuffer[oldestSlot].inUse = true;
  concatBuffer[oldestSlot].refNumber = refNumber;
  concatBuffer[oldestSlot].sender = sender;
  concatBuffer[oldestSlot].totalParts = totalParts;
  concatBuffer[oldestSlot].receivedParts = 0;
  concatBuffer[oldestSlot].firstPartTime = millis();
  for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
    concatBuffer[oldestSlot].parts[j].valid = false;
    concatBuffer[oldestSlot].parts[j].text = "";
  }
  return oldestSlot;
}

// 合并长短信各分段
String LTEModule::assembleConcatSms(int slot) {
  String result = "";
  for (int i = 0; i < concatBuffer[slot].totalParts; i++) {
    if (concatBuffer[slot].parts[i].valid) {
      result += concatBuffer[slot].parts[i].text;
    } else {
      result += "[缺失分段" + String(i + 1) + "]";
    }
  }
  return result;
}

// 清空长短信槽位
void LTEModule::clearConcatSlot(int slot) {
  concatBuffer[slot].inUse = false;
  concatBuffer[slot].receivedParts = 0;
  concatBuffer[slot].sender = "";
  concatBuffer[slot].timestamp = "";
  for (int j = 0; j < MAX_CONCAT_PARTS; j++) {
    concatBuffer[slot].parts[j].valid = false;
    concatBuffer[slot].parts[j].text = "";
  }
}

// 检查长短信超时
void LTEModule::checkConcatTimeout() {
  unsigned long now = millis();
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].inUse) {
      if (now - concatBuffer[i].firstPartTime >= CONCAT_TIMEOUT_MS) {
        // 标记为完成（即使不完整）
        // 在 main.cpp 中会通过 getCompletedConcatSms 获取
        
        // 注意：不在这里清空，等待主循环获取后再清空
      }
    }
  }
}

// 获取已完成的长短信
bool LTEModule::getConcletedConcatSms(SmsData& sms) {
  unsigned long now = millis();
  
  for (int i = 0; i < MAX_CONCAT_MESSAGES; i++) {
    if (concatBuffer[i].inUse) {
      // 检查是否完整或超时
      bool isComplete = (concatBuffer[i].receivedParts == concatBuffer[i].totalParts);
      bool isTimeout = (now - concatBuffer[i].firstPartTime >= CONCAT_TIMEOUT_MS);
      
      if (isComplete || isTimeout) {
        // 合并消息
        sms.phoneNumber = concatBuffer[i].sender;
        sms.content = assembleConcatSms(i);
        sms.date = "";
        sms.time = "";
        
        // 从 timestamp 中提取日期和时间
        if (concatBuffer[i].timestamp.length() > 0) {
          int commaPos = concatBuffer[i].timestamp.indexOf(',');
          if (commaPos != -1) {
            String datePart = concatBuffer[i].timestamp.substring(0, commaPos);
            String timePart = concatBuffer[i].timestamp.substring(commaPos + 1);
            
            int slash1 = datePart.indexOf('/');
            int slash2 = datePart.lastIndexOf('/');
            if (slash1 != -1 && slash2 != -1) {
              String year = datePart.substring(0, slash1);
              String month = datePart.substring(slash1 + 1, slash2);
              String day = datePart.substring(slash2 + 1);
              
              int yearNum = year.toInt();
              if (yearNum < 50) {
                yearNum += 2000;
              } else {
                yearNum += 1900;
              }
              
              sms.date = String(yearNum) + "/" + month + "/" + day;
            }
            
            int colon1 = timePart.indexOf(':');
            int colon2 = timePart.indexOf(':', colon1 + 1);
            if (colon1 != -1 && colon2 != -1) {
              sms.time = timePart.substring(0, colon2);
            }
          }
        }
        
        // 清空槽位
        clearConcatSlot(i);
        
        return true;
      }
    }
  }
  
  return false;
}

// 处理长短信（简化版本，假设使用文本模式）
// 注意：完整的PDU解析比较复杂，这里提供一个简化的框架
bool LTEModule::processConcatSms(const SmsData& sms, SmsData& completedSms) {
  // 这是一个简化版本
  // 实际的长短信检测需要解析PDU格式或使用特殊的AT命令
  // 这里假设短信内容中包含特殊标记（实际应用中需要修改）
  
  // 检查是否是长短信的标记（这只是示例，实际需要PDU解析）
  // 格式示例: [CONCAT:refNum:partNum:totalParts]内容
  if (sms.content.startsWith("[CONCAT:")) {
    int firstColon = sms.content.indexOf(':', 8);
    int secondColon = sms.content.indexOf(':', firstColon + 1);
    int thirdColon = sms.content.indexOf(':', secondColon + 1);
    int closeBracket = sms.content.indexOf(']', thirdColon + 1);
    
    if (firstColon > 0 && secondColon > 0 && thirdColon > 0 && closeBracket > 0) {
      int refNumber = sms.content.substring(8, firstColon).toInt();
      int partNumber = sms.content.substring(firstColon + 1, secondColon).toInt();
      int totalParts = sms.content.substring(secondColon + 1, thirdColon).toInt();
      String content = sms.content.substring(closeBracket + 1);
      
      // 查找或创建槽位
      int slot = findOrCreateConcatSlot(refNumber, sms.phoneNumber, totalParts);
      
      // 保存分段（partNumber从1开始）
      if (partNumber >= 1 && partNumber <= totalParts && partNumber <= MAX_CONCAT_PARTS) {
        int index = partNumber - 1;
        if (!concatBuffer[slot].parts[index].valid) {
          concatBuffer[slot].parts[index].valid = true;
          concatBuffer[slot].parts[index].text = content;
          concatBuffer[slot].receivedParts++;
          
          // 保存时间戳（使用第一个分段的时间）
          if (concatBuffer[slot].timestamp.length() == 0) {
            concatBuffer[slot].timestamp = sms.date + "," + sms.time;
          }
        }
        
        // 检查是否完整
        if (concatBuffer[slot].receivedParts == totalParts) {
          completedSms.phoneNumber = concatBuffer[slot].sender;
          completedSms.content = assembleConcatSms(slot);
          completedSms.date = sms.date;
          completedSms.time = sms.time;
          
          clearConcatSlot(slot);
          return true;
        }
      }
    }
  }
  
  return false;
}


// ==================== PDU编码和发送函数 ====================

// 编码电话号码为PDU格式
String LTEModule::encodePduPhoneNumber(const String& phoneNumber) {
  String result = "";
  String number = phoneNumber;
  
  // 移除前导+号
  if (number.startsWith("+")) {
    number = number.substring(1);
    result = "91";  // 国际格式
  } else {
    result = "81";  // 国内格式
  }
  
  // 如果号码长度为奇数，添加F
  if (number.length() % 2 != 0) {
    number += "F";
  }
  
  // 交换每两位数字
  for (int i = 0; i < number.length(); i += 2) {
    result += number.charAt(i + 1);
    result += number.charAt(i);
  }
  
  return result;
}

// UTF-8转UCS2编码
String LTEModule::encodeUcs2(const String& text) {
  String result = "";
  
  for (int i = 0; i < text.length(); ) {
    unsigned char c = text.charAt(i);
    unsigned int unicode = 0;
    
    if (c < 0x80) {
      // ASCII字符
      unicode = c;
      i++;
    } else if ((c & 0xE0) == 0xC0) {
      // 2字节UTF-8
      if (i + 1 < text.length()) {
        unicode = ((c & 0x1F) << 6) | (text.charAt(i + 1) & 0x3F);
        i += 2;
      } else {
        i++;
      }
    } else if ((c & 0xF0) == 0xE0) {
      // 3字节UTF-8（中文）
      if (i + 2 < text.length()) {
        unicode = ((c & 0x0F) << 12) | 
                  ((text.charAt(i + 1) & 0x3F) << 6) | 
                  (text.charAt(i + 2) & 0x3F);
        i += 3;
      } else {
        i++;
      }
    } else {
      // 其他情况
      i++;
      continue;
    }
    
    // 转换为4位十六进制
    char hex[5];
    sprintf(hex, "%04X", unicode);
    result += hex;
  }
  
  return result;
}

// 7bit编码（简化版，仅支持ASCII）
String LTEModule::encode7bit(const String& text) {
  String result = "";
  
  // 简化实现：直接转换为十六进制
  for (int i = 0; i < text.length(); i++) {
    char hex[3];
    sprintf(hex, "%02X", (unsigned char)text.charAt(i));
    result += hex;
  }
  
  return result;
}

// 构建PDU格式短信
String LTEModule::buildPduSms(const String& phoneNumber, const String& message) {
  String pdu = "";
  
  // 1. SMSC（短信中心号码）- 使用默认，设为00
  pdu += "00";
  
  // 2. PDU类型 - 0x11 (SMS-SUBMIT, TP-VPF=relative)
  pdu += "11";
  
  // 3. 消息参考号 - 00（由模块自动分配）
  pdu += "00";
  
  // 4. 目标号码长度（十进制数字个数）
  String cleanNumber = phoneNumber;
  if (cleanNumber.startsWith("+")) {
    cleanNumber = cleanNumber.substring(1);
  }
  char lenHex[3];
  sprintf(lenHex, "%02X", cleanNumber.length());
  pdu += lenHex;
  
  // 5. 目标号码类型和号码
  pdu += encodePduPhoneNumber(phoneNumber);
  
  // 6. 协议标识 - 00
  pdu += "00";
  
  // 7. 数据编码方案 - 08 (UCS2)
  pdu += "08";
  
  // 8. 有效期 - AA (4天)
  pdu += "AA";
  
  // 9. 用户数据
  String userData = encodeUcs2(message);
  
  // 用户数据长度（字节数）
  int udLength = userData.length() / 2;
  sprintf(lenHex, "%02X", udLength);
  pdu += lenHex;
  
  // 用户数据内容
  pdu += userData;
  
  return pdu;
}

// 发送短信
bool LTEModule::sendSms(const String& phoneNumber, const String& message) {
  // 设置为PDU模式
  String resp = sendATCommand("AT+CMGF=0", AT_TIMEOUT);
  if (resp.indexOf("OK") < 0) {
    return false;
  }
  
  // 设置文本模式参数
  resp = sendATCommand("AT+CSMP=33,167,0,0", AT_TIMEOUT);
  
  // 构建PDU
  String pdu = buildPduSms(phoneNumber, message);
  
  // 计算PDU长度（不包括SMSC部分）
  int pduLength = (pdu.length() - 2) / 2;
  
  // 发送CMGS命令
  String cmgsCmd = "AT+CMGS=" + String(pduLength);
  Serial.println(cmgsCmd);
  Serial.flush();
  
  // 等待 > 提示符
  unsigned long startTime = millis();
  bool gotPrompt = false;
  String response = "";
  
  while (millis() - startTime < 5000) {
    if (Serial.available()) {
      char c = Serial.read();
      response += c;
      if (c == '>') {
        gotPrompt = true;
        break;
      }
    }
  }
  
  if (!gotPrompt) {
    return false;
  }
  
  // 发送PDU数据
  Serial.print(pdu);
  Serial.write(0x1A);  // Ctrl-Z
  Serial.flush();
  
  // 等待响应
  startTime = millis();
  response = "";
  while (millis() - startTime < 10000) {
    if (Serial.available()) {
      char c = Serial.read();
      response += c;
      
      if (response.indexOf("OK") != -1) {
        return true;
      }
      if (response.indexOf("ERROR") != -1) {
        return false;
      }
    }
  }
  
  return false;
}
