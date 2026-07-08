/**
 * config.h — 板级硬件配置
 * 
 * 这个文件定义了所有硬件引脚的接线关系和各种配置常量。
 * 如果你自己改了接线（比如换了屏幕引脚），就在这里改。
 * 
 * 【引脚定义】
 * LCD_CLK     → GPIO4   屏幕时钟
 * LCD_MOSI    → GPIO6   屏幕数据线
 * LCD_CS      → GPIO7   屏幕片选
 * BTN_LEFT    → GPIO0   左边按键→”收信模式键”
 * BTN_MID     → GPIO1   中间按键→”确定键”
 * BTN_RIGHT   → GPIO2   右边按键→”转发模式键”
 * BTN_SPK     → GPIO5   蜂鸣器
 * LCD_BACKLIGHT → GPIO3   屏幕背光控制
 * 
 * 【背光模式 enum BacklightMode】
 * BACKLIGHT_OFF=0       → 背光一直关闭
 * BACKLIGHT_ALWAYS_ON=1 → 背光常亮
 * BACKLIGHT_ON_MESSAGE=2 → 收到消息时才亮
 */

#ifndef CONFIG_H
#define CONFIG_H

// 硬件引脚定义
// ST7920显示屏引脚
#define LCD_CLK  4  // IO4 - 时钟
#define LCD_MOSI 6  // IO6 - 数据
#define LCD_CS   7  // IO7 - 片选

// 按键引脚
#define BTN_LEFT 0  // IO0 - 收信模式按键
#define BTN_MID 1   // IO1 - 确定按键
#define BTN_RIGHT 2 // IO2 - 转发模式按键

// 蜂鸣器引脚
#define BTN_SPK 5  // IO5 - 蜂鸣器

// 背光引脚
#define LCD_BACKLIGHT 3  // IO3 - 背光控制

// 背光模式
enum BacklightMode {
  BACKLIGHT_OFF = 0,      // 关闭
  BACKLIGHT_ALWAYS_ON = 1, // 常亮
  BACKLIGHT_ON_MESSAGE = 2 // 收到消息时开启
};

// 显示配置
#define MAX_DISPLAY_WIDTH 128
#define SCROLL_DELAY 150
#define SCROLL_STEP 2

// 通信配置
#define SERIAL_BAUD 115200
#define AT_TIMEOUT 2000

// HTTP配置
#define MIN_SEND_INTERVAL 5000  // 最小发送间隔(ms)

// 时间同步配置
#define TIME_SYNC_INTERVAL 60000  // 时间同步间隔(1分钟)

// WiFi配置
#define AP_SSID "EDA-Pager"
#define AP_PASSWORD "12345678"
#define WEB_SERVER_PORT 80

#endif
