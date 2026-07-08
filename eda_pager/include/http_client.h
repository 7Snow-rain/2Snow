/**
 * http_client.h — HTTP 请求客户端类
 * 
 * 功能：把收到的短信通过 HTTP 请求转发出去。
 * 比如转发到微信（通过 ServerChan API）、邮箱、你自己写的服务器等。
 * 
 * 支持 GET 和 POST 两种方法，URL 中可以包含占位符。
 * 会自动处理：
 *   - URL 编码/解码
 *   - JSON 字符串转义
 *   - 查询参数转 JSON
 *   - 请求频率限制（最小间隔 5 秒）
 */

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>

// HTTP客户端类
class HttpClient {
public:
  HttpClient();
  
  // 发送HTTP请求
  bool sendRequest(const String& url, const String& method, const String& smsContent);
  
  // 获取最后一次错误信息
  String getLastError() const { return lastError; }
  
  // 获取最后一次HTTP状态码
  int getLastHttpCode() const { return lastHttpCode; }
  
private:
  String lastError;
  int lastHttpCode;
  unsigned long lastSendTime;
  
  // 替换URL中的占位符
  String replacePlaceholder(const String& url, const String& smsContent);
  
  // URL编码
  String urlEncode(const String& str);
  
  // URL解码
  String urlDecode(const String& str);
  
  // JSON字符串转义
  String escapeJsonString(const String& str);
  
  // 解析查询参数并转换为JSON
  String parseQueryParamsToJson(const String& queryParams, const String& smsContent);
  
  // 发送GET请求
  bool sendGetRequest(const String& url);
  
  // 发送POST请求
  bool sendPostRequest(const String& url, const String& smsContent);
};

#endif
