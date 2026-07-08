#include "http_client.h"
#include "config.h"
#include <WiFi.h>

HttpClient::HttpClient() {
  lastError = "";
  lastHttpCode = 0;
  lastSendTime = 0;
}

bool HttpClient::sendRequest(const String& url, const String& method, const String& smsContent) {
  // 检查WiFi连接 - 使用 localIP 来判断是否真正连接到网络
  if (WiFi.localIP().toString() == "0.0.0.0" || WiFi.localIP()[0] == 0) {
    lastError = "WiFi not connected (No IP)";
    lastHttpCode = 0;
    return false;
  }
  
  // 检查发送间隔
  unsigned long currentTime = millis();
  if (currentTime - lastSendTime < MIN_SEND_INTERVAL) {
    lastError = "Send too frequent, please wait";
    lastHttpCode = 0;
    return false;
  }
  
  bool success = false;
  
  if (method == "POST") {
    success = sendPostRequest(url, smsContent);
  } else {
    // 默认使用GET
    String finalUrl = replacePlaceholder(url, smsContent);
    success = sendGetRequest(finalUrl);
  }
  
  if (success) {
    lastSendTime = currentTime;
  }
  
  return success;
}

bool HttpClient::sendGetRequest(const String& url) {
  HTTPClient http;
  
  http.begin(url);
  http.setTimeout(10000);  // 10秒超时
  
  lastHttpCode = http.GET();
  
  if (lastHttpCode > 0) {
    String payload = http.getString();
    
    if (lastHttpCode == 200) {
      lastError = "发送成功\n响应: " + payload.substring(0, 100);
      http.end();
      return true;
    } else {
      lastError = "HTTP错误: " + String(lastHttpCode) + "\n响应: " + payload.substring(0, 100);
      http.end();
      return false;
    }
  } else {
    lastError = "请求失败: " + http.errorToString(lastHttpCode);
    http.end();
    return false;
  }
}

bool HttpClient::sendPostRequest(const String& url, const String& smsContent) {
  HTTPClient http;
  
  // 解析 URL，提取基础 URL 和查询参数
  String baseUrl = url;
  String queryParams = "";
  
  int questionMarkPos = url.indexOf('?');
  if (questionMarkPos != -1) {
    baseUrl = url.substring(0, questionMarkPos);
    queryParams = url.substring(questionMarkPos + 1);
  }
  
  http.begin(baseUrl);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  
  // 构建 JSON 数据
  String jsonData = "";
  
  if (queryParams.length() > 0) {
    // 解析查询参数并转换为 JSON
    jsonData = parseQueryParamsToJson(queryParams, smsContent);
  } else {
    // 如果没有查询参数，使用默认格式
    String escapedContent = escapeJsonString(smsContent);
    jsonData = "{\"message\":\"" + escapedContent + "\"}";
  }
  
  lastHttpCode = http.POST(jsonData);
  
  if (lastHttpCode > 0) {
    String payload = http.getString();
    
    if (lastHttpCode == 200 || lastHttpCode == 201) {
      lastError = "Send success\nResponse: " + payload.substring(0, 100);
      http.end();
      return true;
    } else {
      lastError = "HTTP error: " + String(lastHttpCode) + "\nResponse: " + payload.substring(0, 100);
      http.end();
      return false;
    }
  } else {
    lastError = "Request failed: " + http.errorToString(lastHttpCode);
    http.end();
    return false;
  }
}

String HttpClient::replacePlaceholder(const String& url, const String& smsContent) {
  String result = url;
  String encoded = urlEncode(smsContent);
  result.replace("{sms}", encoded);
  return result;
}

String HttpClient::urlEncode(const String& str) {
  String encoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) {
        code1 = (c & 0xf) - 10 + 'A';
      }
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) {
        code0 = c - 10 + 'A';
      }
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  
  return encoded;
}


String HttpClient::escapeJsonString(const String& str) {
  String escaped = str;
  escaped.replace("\\", "\\\\");
  escaped.replace("\"", "\\\"");
  escaped.replace("\n", "\\n");
  escaped.replace("\r", "\\r");
  escaped.replace("\t", "\\t");
  escaped.replace("\b", "\\b");
  escaped.replace("\f", "\\f");
  return escaped;
}

String HttpClient::urlDecode(const String& str) {
  String decoded = "";
  char c;
  char code0;
  char code1;
  
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%') {
      if (i + 2 < str.length()) {
        code0 = str.charAt(i + 1);
        code1 = str.charAt(i + 2);
        
        int value = 0;
        if (code0 >= '0' && code0 <= '9') value = (code0 - '0') << 4;
        else if (code0 >= 'A' && code0 <= 'F') value = (code0 - 'A' + 10) << 4;
        else if (code0 >= 'a' && code0 <= 'f') value = (code0 - 'a' + 10) << 4;
        
        if (code1 >= '0' && code1 <= '9') value += (code1 - '0');
        else if (code1 >= 'A' && code1 <= 'F') value += (code1 - 'A' + 10);
        else if (code1 >= 'a' && code1 <= 'f') value += (code1 - 'a' + 10);
        
        decoded += (char)value;
        i += 2;
      } else {
        decoded += c;
      }
    } else {
      decoded += c;
    }
  }
  
  return decoded;
}

String HttpClient::parseQueryParamsToJson(const String& queryParams, const String& smsContent) {
  String json = "{";
  bool firstParam = true;
  
  // 分割查询参数
  int startPos = 0;
  int ampPos = queryParams.indexOf('&');
  
  while (startPos < queryParams.length()) {
    String param;
    if (ampPos == -1) {
      param = queryParams.substring(startPos);
      startPos = queryParams.length();
    } else {
      param = queryParams.substring(startPos, ampPos);
      startPos = ampPos + 1;
      ampPos = queryParams.indexOf('&', startPos);
    }
    
    // 解析键值对
    int equalPos = param.indexOf('=');
    if (equalPos != -1) {
      String key = param.substring(0, equalPos);
      String value = param.substring(equalPos + 1);
      
      // URL 解码
      key = urlDecode(key);
      value = urlDecode(value);
      
      // 替换占位符
      if (value.indexOf("{sms}") != -1) {
        value.replace("{sms}", smsContent);
      }
      
      // 添加到 JSON
      if (!firstParam) {
        json += ",";
      }
      json += "\"" + escapeJsonString(key) + "\":\"" + escapeJsonString(value) + "\"";
      firstParam = false;
    }
  }
  
  json += "}";
  return json;
}
