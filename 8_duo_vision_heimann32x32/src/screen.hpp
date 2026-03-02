
#pragma once
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <Arduino.h>
#include "miku_jpg.hpp"
#include "cam_cal_mat.hpp"  // 摄像头校准矩阵数据

#define PIN_BLK 4
#define PWM_CHANNEL 0     // LEDC PWM 通道
#define PWM_FREQ 20000    // PWM 频率 20kHz (超出人耳范围，减少 I2C 干扰)
#define PWM_RESOLUTION 8  // 8位分辨率 (0-255)
#include "shared_val.hpp"  // 使用 shared_val.h 中定义的 brightness

static const uint16_t screenWidth  = 320;
static const uint16_t screenHeight = 240;

TFT_eSPI tft = TFT_eSPI(screenHeight, screenWidth); /* TFT instance */

// ================= 亮度控制 =================
inline void set_brightness(int _brightness, bool remenber=true){
   // 限制范围在 0-255
   if (_brightness > 255) _brightness = 255;
   if (_brightness < 0) _brightness = 0;

   // 使用 LEDC 高频 PWM 替代 analogWrite
   ledcWrite(PWM_CHANNEL, _brightness);

   if (remenber) {brightness = _brightness;}
}

// ================= 亮起屏幕 =================
void smooth_on(){
   ledcWrite(PWM_CHANNEL, 0);  // 先关闭
   for(int i=0; i<brightness; i++){
      set_brightness(i, false);
      delay(2);
   }
}

// ================= 熄灭屏幕 =================
void smooth_off(){
   for(int i=brightness; i>=0; i--){
      ledcWrite(PWM_CHANNEL, i);
      delay(2);
   }
}

// ================= JPEG 解码回调 =================
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap){
    // 用途就是把解码后的图像数据绘制到屏幕上
   if ( y >= tft.height() ) return 0;
   tft.pushImage(x, y, w, h, bitmap);
   return 1;
}

// 显示启动画面
void render_boot_logo(){
   TJpgDec.setCallback(tft_output);  // 设置jpeg解码器回调函数
   tft.fillScreen(TFT_BLACK);
   TJpgDec.setJpgScale(1);
   uint16_t w = 0, h = 0;
   TJpgDec.getJpgSize(&w, &h, miku_jpg, sizeof(miku_jpg));
   tft.startWrite();
   TJpgDec.drawJpg(0, 0, miku_jpg, sizeof(miku_jpg));
   tft.endWrite();
}


// ================= 屏幕初始化主函数 =================
void screen_init(){
    Serial.println("Initializing screen...");
    // 初始化 LEDC 高频 PWM (20kHz，减少 I2C 干扰)
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(PIN_BLK, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);  // 初始关闭
    
    tft.init();
    tft.setRotation(1); 
    tft.setSwapBytes(true);
    tft.invertDisplay(false); 
    if(!color_reverse){
        tft.invertDisplay(true);
        Serial.print("[Screen] Color reversed.");
    }else{
        tft.invertDisplay(false);
        Serial.print("[Screen] Color not reversed.");
    }
    tft.initDMA();
    // 打开屏幕之后要做的事情：慢慢打开屏幕，然后绘制一个可爱的miku图片
    render_boot_logo();  // 显示启动画面
    smooth_on();
    delay(500);
}

// 支持的命令：screen off, screen on, screen brightness <value>
void screen_cli(String cmd){
    cmd.trim();
    if (cmd.equalsIgnoreCase("screen off")){
        smooth_off();
        // 增加熄屏回显
        Serial.println("[Screen] Turned OFF smoothly.");
        
    }else if(cmd.equalsIgnoreCase("screen on")){
        smooth_on();
        // 增加亮屏回显
        Serial.println("[Screen] Turned ON smoothly.");
        
    }else if(cmd.startsWith("screen brightness ")){
        String valueStr = cmd.substring(String("screen brightness ").length());
        int value = valueStr.toInt();
        set_brightness(value);
        // 增加亮度设置回显，把实际设置的值打印出来
        Serial.print("[Screen] Brightness set to: ");
        Serial.println(value);
        
    }else if (cmd.startsWith("color_reverse ")){
        String param = cmd.substring(14);
        param.trim();  // 去除参数两端的空白字符
        if (param == "1") {
            color_reverse = true; // 设置为反转
            Serial.println("Color reverse enabled.");
            delay(100);
            tft.invertDisplay(false); // 立即应用反转设置
            EEPROM.write(0, color_reverse);
            EEPROM.commit(); // 确保数据写入 EEPROM
        } else if (param == "0") {
            color_reverse = false; // 设置为不反转
            Serial.println("Color reverse disabled.");
            delay(100);
            tft.invertDisplay(true); // 立即应用反转设置
            EEPROM.write(0, color_reverse);
            EEPROM.commit(); // 确保数据写入 EEPROM
        } else if (param == "-q") {
            Serial.println("Color reverse status: " + String(color_reverse ? "enabled" : "disabled")); // 查询当前状态
        }else {
            Serial.println("Invalid parameter for color_reverse. Use '1' to enable, '0' to disable, or '-q' to query status.");
        }
    }else{
        // 未知指令提示
        Serial.println("[Error] Unknown screen command: " + cmd);
    }
}

// 执行摄像头校准流程
void camera_calibrate(){
   TJpgDec.setCallback(tft_output);
   tft.fillScreen(TFT_BLACK);
   TJpgDec.setJpgScale(1);
   TJpgDec.setSwapBytes(false);
   camera_calibrated = true;
   uint16_t w = 0, h = 0;
   TJpgDec.getJpgSize(&w, &h, camera_cal_mat, sizeof(camera_cal_mat));
   tft.startWrite();
   TJpgDec.drawJpg(0, 0, camera_cal_mat, sizeof(camera_cal_mat));
   tft.endWrite();
}