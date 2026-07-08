#include "web_server.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

// HTML页面
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>SMS Pager 配置</title>
  <style>
    body { font-family: Arial; margin: 0; padding: 0; background: #f0f0f0; }
    .container { max-width: 800px; margin: 0 auto; background: white; }
    .tabs { display: flex; background: #2196F3; }
    .tab { flex: 1; padding: 15px; text-align: center; color: white; cursor: pointer; border: none; background: none; font-size: 16px; }
    .tab.active { background: #1976D2; }
    .tab:hover { background: #1565C0; }
    .content { padding: 20px; }
    .section { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 5px; }
    h1 { color: #333; text-align: center; margin: 20px 0; }
    h2 { color: #555; margin-top: 0; }
    label { display: block; margin: 10px 0 5px; font-weight: bold; color: #555; }
    input, select { width: 100%; padding: 10px; margin: 5px 0; border: 1px solid #ddd; border-radius: 5px; box-sizing: border-box; }
    button { width: 100%; padding: 12px; margin: 10px 0; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    button:hover { background: #45a049; }
    .status { text-align: center; padding: 10px; margin: 10px 0; border-radius: 5px; }
    .connected { background: #c8e6c9; color: #2e7d32; }
    .disconnected { background: #ffcdd2; color: #c62828; }
    .sms-list { max-height: 500px; overflow-y: auto; }
    .sms-item { padding: 15px; margin: 10px 0; background: white; border: 1px solid #ddd; border-radius: 5px; cursor: pointer; transition: all 0.3s; }
    .sms-item:hover { box-shadow: 0 2px 8px rgba(0,0,0,0.1); transform: translateY(-2px); }
    .sms-header { display: flex; justify-content: space-between; margin-bottom: 8px; }
    .sms-phone { font-weight: bold; color: #2196F3; font-size: 16px; }
    .sms-time { color: #999; font-size: 14px; }
    .sms-content { color: #333; line-height: 1.5; word-wrap: break-word; }
    .sms-content.collapsed { max-height: 60px; overflow: hidden; position: relative; }
    .sms-content.collapsed::after { content: '...'; position: absolute; bottom: 0; right: 0; background: white; padding-left: 20px; }
    .empty-state { text-align: center; padding: 40px; color: #999; }
    .empty-state svg { width: 80px; height: 80px; margin-bottom: 20px; opacity: 0.3; }
  </style>
</head>
<body>
  <div class="container">
    <h1>EDA Pager寻呼机</h1>
    
    <div class="tabs">
      <button class="tab active" onclick="showTab('config')">配置</button>
      <button class="tab" onclick="showTab('sms')">收件箱</button>
    </div>
    
    <div class="content">
      <div id="configTab">
        <div id="status" class="status"></div>
        
        <div class="section">
          <h2>WiFi配置</h2>
          <label>WiFi 名称:</label>
          <input type="text" id="ssid" placeholder="Enter WiFi SSID">
          
          <label>WiFi 密码:</label>
          <input type="password" id="password" placeholder="Enter WiFi Password">
          
          <button onclick="saveWiFi()">保存WIFI配置</button>
        </div>
        
        <div class="section">
          <h2>HTTP 推送配置</h2>
          <label>
            <input type="checkbox" id="forwardEnabledHttp" onchange="toggleForwardHttp()">
            启用 HTTP 推送配置
          </label>
          <div id="forwardSettingsHttp" style="display: none; margin-top: 15px;">
            <label>Request Method:</label>
            <select id="method">
              <option value="GET">GET(不推荐)</option>
              <option value="POST">POST</option>
            </select>
            
            <label>URL 地址:</label>
            <input type="text" id="url" placeholder="https://example.com/api?msg={sms}">
            <strong>提示:</strong> 使用 {sms} 替换内容<br>
            <button onclick="saveHttp()">保存HTTP配置</button>
          </div>
        </div>
        
        <div class="section">
          <h2>SMS 转发配置</h2>
          <label>
            <input type="checkbox" id="forwardEnabledPhone" onchange="toggleForwardPhone()">
            启用 SMS 转发
          </label>
          
          <div id="forwardSettingsPhone" style="display: none; margin-top: 15px;">
            <label>目标收信手机号:</label>
            <input type="text" id="forwardNumber" placeholder="+86...">
            <button onclick="saveSmsForward()">保存SMS转发配置</button>
          </div>
        </div>
        
        <div class="section">
          <h2>提示音配置</h2>
          <label>
            <input type="checkbox" id="muteSwitch" onchange="toggleMute()">
            Mute Speaker
          </label>
        </div>
        
        <div class="section">
          <h2>BLK 背光控制</h2>
          <label>背光模式:</label>
          <select id="backlightMode" onchange="setBacklight()">
            <option value="0">关闭</option>
            <option value="1">常亮</option>
            <option value="2">收到消息时开启</option>
          </select>
          <div class="info">
            <strong>关闭:</strong> 背光始终关闭<br>
            <strong>常亮:</strong> 背光始终开启<br>
            <strong>收到消息时开启:</strong> 收到短信时自动开启，消息已读时关闭
          </div>
        </div>
        
        <div class="section">
          <button onclick="testHttp()">测试HTTP请求</button>
          <button onclick="completeConfig()" style="background: #2196F3;">完成配置</button>
          <button onclick="reboot()" style="background: #f44336;">重启</button>
        </div>
      </div>
      
      <div id="smsTab" style="display: none;">
        <div class="section">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
            <h2 style="margin: 0;">Inbox (<span id="smsCount">0</span>)</h2>
            <button onclick="refreshSms()" style="width: auto; padding: 8px 20px; margin: 0;">刷新</button>
          </div>
          <div id="smsList" class="sms-list"></div>
        </div>
      </div>
    </div>
  </div>
  
  <script>
    function showTab(tab) {
      document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
      if (tab === 'config') {
        document.querySelector('.tab:nth-child(1)').classList.add('active');
        document.getElementById('configTab').style.display = 'block';
        document.getElementById('smsTab').style.display = 'none';
        loadConfig();
      } else if (tab === 'sms') {
        document.querySelector('.tab:nth-child(2)').classList.add('active');
        document.getElementById('configTab').style.display = 'none';
        document.getElementById('smsTab').style.display = 'block';
        loadSms();
      }
    }
    
    function loadConfig() {
      fetch('/config')
        .then(r => r.json())
        .then(data => {
          document.getElementById('ssid').value = data.ssid || '';
          document.getElementById('password').value = data.password || '';
          document.getElementById('method').value = data.method || 'GET';
          document.getElementById('url').value = data.url || '';
          document.getElementById('forwardEnabledHttp').checked = data.forwardEnabledHttp || false;
          document.getElementById('forwardEnabledPhone').checked = data.forwardEnabledPhone || false;
          document.getElementById('forwardNumber').value = data.forwardNumber || '';
          document.getElementById('muteSwitch').checked = data.muted || false;
          document.getElementById('backlightMode').value = data.backlightMode || 1;
          toggleForward();
          updateStatus(data.wifiConnected, data.localIP, data.wifiStatus);
        });
    }
    
    function toggleForwardPhone() {
      const enabled = document.getElementById('forwardEnabledPhone').checked;
      document.getElementById('forwardSettingsPhone').style.display = enabled ? 'block' : 'none';
    }
    function toggleForwardHttp() {
      const enabled = document.getElementById('forwardEnabledHttp').checked;
      document.getElementById('forwardSettingsHttp').style.display = enabled ? 'block' : 'none';
    }
    function updateStatus(connected, localIP, wifiStatus) {
      const status = document.getElementById('status');
      if (connected) {
        status.className = 'status connected';
        status.textContent = 'WiFi Connected - IP: ' + (localIP || 'N/A');
      } else {
        status.className = 'status disconnected';
        status.textContent = 'WiFi Disconnected (Status: ' + (wifiStatus || 'N/A') + ')';
      }
    }
    
    function saveWiFi() {
      const data = {
        ssid: document.getElementById('ssid').value,
        password: document.getElementById('password').value
      };
      
      if (!data.ssid) {
        alert('请输入WIFI名称');
        return;
      }
      
      fetch('/saveWiFi', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.text())
      .then(msg => {
        alert(msg);
        setTimeout(() => loadConfig(), 2000);
      });
    }
    
    function saveHttp() {
      const data = {
        enabled: document.getElementById('forwardEnabledHttp').checked,
        method: document.getElementById('method').value,
        url: document.getElementById('url').value
      };
      
      if (!data.url) {
        alert('Please enter HTTP URL');
        return;
      }
      
      fetch('/saveHttp', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.text())
      .then(msg => alert(msg));
    }
    
    function saveSmsForward() {
      const data = {
        enabled: document.getElementById('forwardEnabledPhone').checked,
        number: document.getElementById('forwardNumber').value
      };
      
      if (data.enabled && !data.number) {
        alert('请输入目标手机号码');
        return;
      }
      
      fetch('/saveSmsForward', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(data)
      })
      .then(r => r.text())
      .then(msg => alert(msg));
    }
    
    function toggleMute() {
      const muted = document.getElementById('muteSwitch').checked;
      
      fetch('/toggleMute', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({mute: muted})
      })
      .then(r => r.text())
      .then(msg => {
        console.log(msg);
      })
      .catch(err => {
        console.error('Failed to toggle mute:', err);
        alert('操作失败');
      });
    }
    
    function setBacklight() {
      const mode = parseInt(document.getElementById('backlightMode').value);
      
      fetch('/setBacklight', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({mode: mode})
      })
      .then(r => r.text())
      .then(msg => {
        console.log(msg);
      })
      .catch(err => {
        console.error('Failed to set backlight:', err);
        alert('设置背光失败');
      });
    }
    
    function testHttp() {
      fetch('/test')
        .then(r => r.text())
        .then(msg => alert(msg));
    }
    
    function completeConfig() {
      if (confirm('确认保存所有配置?')) {
        fetch('/complete').then(r => r.text()).then(msg => {
          alert(msg);
          setTimeout(() => location.reload(), 2000);
        });
      }
    }
    
    function reboot() {
      if (confirm('Reboot device?')) {
        fetch('/reboot').then(() => {
          alert('设备重启中...');
          setTimeout(() => location.reload(), 5000);
        });
      }
    }
    
    function loadSms() {
      fetch('/sms')
        .then(r => r.json())
        .then(data => {
          const list = document.getElementById('smsList');
          const count = document.getElementById('smsCount');
          
          count.textContent = data.length;
          
          if (data.length === 0) {
            list.innerHTML = '<div class="empty-state"><p>无消息</p></div>';
          } else {
            list.innerHTML = data.map((sms, index) => `
              <div class="sms-item" onclick="toggleSms(${index})">
                <div class="sms-header">
                  <span class="sms-phone">${sms.phone}</span>
                  <span class="sms-time">${sms.date} ${sms.time}</span>
                </div>
                <div class="sms-content collapsed" id="sms-${index}">
                  ${sms.content}
                </div>
              </div>
            `).join('');
          }
        });
    }
    
    function toggleSms(index) {
      const content = document.getElementById('sms-' + index);
      content.classList.toggle('collapsed');
    }
    
    function refreshSms() {
      loadSms();
    }
    
    loadConfig();
  </script>
</body>
</html>
)rawliteral";


WebServer::WebServer(ConfigManager& config, HttpClient& httpClient, Display& display,
                     SmsData* smsHistory, int* smsHistoryCount, Speaker& speaker)
  : server(WEB_SERVER_PORT),
    configManager(config),
    httpClient(httpClient),
    display(display),
    speaker(speaker),
    smsHistory(smsHistory),
    smsHistoryCount(smsHistoryCount),
    configCompleted(false),
    wifiConnected(false) {
}

void WebServer::begin() {
  setupRoutes();
  server.begin();
}

void WebServer::setupRoutes() {
  // Main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  
  // Get config
  server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *request){
    JsonDocument doc;
    doc["ssid"] = configManager.wifiSSID;
    doc["password"] = configManager.wifiPassword;
    doc["method"] = configManager.httpMethod;
    doc["url"] = configManager.httpUrl;
    doc["forwardEnabledHttp"] = configManager.httpForwardEnabled;
    doc["forwardEnabledPhone"] = configManager.smsForwardEnabled;
    doc["forwardNumber"] = configManager.smsForwardNumber;
    doc["wifiConnected"] = wifiConnected;
    doc["muted"] = speaker.isMuted();
    doc["backlightMode"] = configManager.backlightMode;
    
    // 添加详细的 WiFi 状态信息用于调试
    doc["wifiStatus"] = WiFi.status();
    doc["localIP"] = WiFi.localIP().toString();
    doc["apIP"] = WiFi.softAPIP().toString();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // Get SMS list
  server.on("/sms", HTTP_GET, [this](AsyncWebServerRequest *request){
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (int i = *smsHistoryCount - 1; i >= 0; i--) {
      JsonObject sms = array.add<JsonObject>();
      sms["phone"] = smsHistory[i].phoneNumber;
      sms["date"] = smsHistory[i].date;
      sms["time"] = smsHistory[i].time;
      sms["content"] = smsHistory[i].content;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // Save WiFi config
  server.on("/saveWiFi", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      
      configManager.wifiSSID = doc["ssid"].as<String>();
      configManager.wifiPassword = doc["password"].as<String>();
      configManager.save();
      
      request->send(200, "text/plain", "WiFi 配置已保存");
    });
  
  // Save HTTP config
  server.on("/saveHttp", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      configManager.httpForwardEnabled = doc["enabled"].as<bool>();
      configManager.httpMethod = doc["method"].as<String>();
      configManager.httpUrl = doc["url"].as<String>();
      configManager.save();
      
      request->send(200, "text/plain", "HTTP 转发配置已保存");
    });
  
  // Save SMS forward config
  server.on("/saveSmsForward", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      
      configManager.smsForwardEnabled = doc["enabled"].as<bool>();
      configManager.smsForwardNumber = doc["number"].as<String>();
      configManager.save();
      
      request->send(200, "text/plain", "SMS 转发配置已保存");
    });
  
  // Toggle mute
  server.on("/toggleMute", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      
      bool mute = doc["mute"].as<bool>();
      if (mute) {
        speaker.mute();
      } else {
        speaker.unmute();
      }
      
      request->send(200, "text/plain", mute ? "Speaker 已静音" : "Speaker 开启");
    });
  
  // Set backlight mode
  server.on("/setBacklight", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      JsonDocument doc;
      deserializeJson(doc, (const char*)data);
      
      int mode = doc["mode"].as<int>();
      configManager.backlightMode = mode;
      configManager.save();
      display.setBacklightMode(mode);
      
      String modeStr = "";
      switch(mode) {
        case 0: modeStr = "关闭"; break;
        case 1: modeStr = "常亮"; break;
        case 2: modeStr = "收到消息时开启"; break;
      }
      
      request->send(200, "text/plain", "背光模式: " + modeStr);
    });
  
  // Test HTTP request
  server.on("/test", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (configManager.httpUrl.length() == 0) {
      request->send(400, "text/plain", "Error: HTTP URL 未配置");
      return;
    }
    
    bool success = httpClient.sendRequest(configManager.httpUrl, configManager.httpMethod, "Test message");
    
    String message;
    if (success) {
      message = "Test successful!\n\n";
      message += httpClient.getLastError();
    } else {
      message = "Test failed\n\n";
      message += "Error: " + httpClient.getLastError();
      if (httpClient.getLastHttpCode() != 0) {
        message += "\nHTTP Code: " + String(httpClient.getLastHttpCode());
      }
    }
    
    request->send(success ? 200 : 500, "text/plain", message);
  });
  
  // Complete config
  server.on("/complete", HTTP_GET, [this](AsyncWebServerRequest *request){
    if (configManager.isConfigured()) {
      configCompleted = true;
      configManager.setConfigured(true);
      request->send(200, "text/plain", "配置完成");
    } else {
      request->send(400, "text/plain", "请先完成WIFI配置");
    }
  });
  
  // Reboot device
  server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "重启中...");
    delay(1000);
    ESP.restart();
  });
}
