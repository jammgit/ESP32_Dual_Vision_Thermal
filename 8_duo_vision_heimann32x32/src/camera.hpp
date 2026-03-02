#pragma once
#include<Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "screen.hpp"
#include "shared_val.hpp"
#include "file_system.hpp"
#include <Wire.h>
#include <MD5Builder.h>
#define logln(...) Serial.println(__VA_ARGS__)

// 摄像头校准矩阵的正确 MD5 哈希值（用于验证校准数据完整性）
const char* CAMERA_CAL_MAT_MD5 = "e5c33efdc7c026f090c92834b51a3b2c";


// 摄像头在这块板子上的引脚定义
#define CAM_WIDTH 320
#define CAM_HEIGHT 240
#define PWDN_GPIO_NUM     17
#define RESET_GPIO_NUM    26
#define XCLK_GPIO_NUM     14
#define PCLK_GPIO_NUM     22
#define SIOD_GPIO_NUM     23
#define SIOC_GPIO_NUM     18
#define VSYNC_GPIO_NUM    21
#define HREF_GPIO_NUM     27
#define D0_GPIO_NUM       34
#define D1_GPIO_NUM       33
#define D2_GPIO_NUM       25
#define D3_GPIO_NUM       35
#define D4_GPIO_NUM       39
#define D5_GPIO_NUM       38
#define D6_GPIO_NUM       37
#define D7_GPIO_NUM       36
#define I2CPULL_UP  19
#define I2CPULL_UP_OPEN  LOW

bool camera_ok = false;
bool is_buf_init = false;

camera_fb_t *fb = NULL;

// 摄像头电平初始化
void camera_hard_reset() {
    pinMode(I2CPULL_UP, OUTPUT);
    digitalWrite(I2CPULL_UP, I2CPULL_UP_OPEN);
    delay(200);
    pinMode(PWDN_GPIO_NUM, OUTPUT);
    digitalWrite(PWDN_GPIO_NUM, HIGH);
    delay(100);
    digitalWrite(PWDN_GPIO_NUM, LOW);
    delay(200);
}

uint8_t *rgb_buffer = nullptr;
void init_rgb_buffer() {
    if (psramFound()) {
        // 在 PSRAM 中申请内存
        rgb_buffer = (uint8_t *)heap_caps_malloc(CAM_WIDTH * CAM_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    } else {
        // 如果没有 PSRAM，这么大的内存可能申请失败
        rgb_buffer = (uint8_t *)malloc(CAM_WIDTH * CAM_HEIGHT * 3);
    }
    
    if (rgb_buffer) {
        is_buf_init = true;
        logln("RGB Buffer allocated in PSRAM/RAM");
    } else {
        is_buf_init = false;
        logln("Failed to allocate RGB Buffer!");
    }
}

// 解码回调函数：将解码后的像素块写入全局 rgb_buffer
bool rgb_decode_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    // 边界检查
    if (y >= CAM_HEIGHT) return 0;

    // 只需要 RGB565，可以直接 memcpy
    // === rgb_buffer 是 uint16_t* (RGB565) ===
    uint16_t *pBuf = (uint16_t*)rgb_buffer + (y * CAM_WIDTH + x);
    for (int j = 0; j < h; j++) {
        memcpy(pBuf, bitmap, w * 2);
        pBuf += CAM_WIDTH;
        bitmap += w;
    }
    return 1; // 返回 1 继续解码
}

void camera_init(){
    // 首先验证摄像头校准矩阵的完整性（MD5校验）
    {
        extern const uint8_t camera_cal_mat[];
        extern size_t get_camera_cal_mat_len();
        
        MD5Builder md5;
        md5.begin();
        md5.add(const_cast<uint8_t*>(camera_cal_mat), get_camera_cal_mat_len());
        md5.calculate();
        String hash = md5.toString();
        
        Serial.print("[Camera] Calibration matrix verification: ");
        Serial.println(hash);
        
        if (hash != CAMERA_CAL_MAT_MD5) {
            Serial.println("[Camera] Calibration data corrupted! Camera initialization aborted.");
            Serial.print("[Camera] Expected: ");
            Serial.println(CAMERA_CAL_MAT_MD5);
            Serial.print("[Camera] Got:      ");
            Serial.println(hash);
            camera_ok = false;
            return;
        }
        Serial.println("[Camera] Calibration data verified.");
    }
    
    // 关闭欠压检测
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
    // 摄像头上电
    camera_hard_reset();
    pinMode(I2CPULL_UP, OUTPUT);
    digitalWrite(I2CPULL_UP, I2CPULL_UP_OPEN);
    // 等待摄像头上电
    delay(800);
    // check_camera_i2c();
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = D0_GPIO_NUM;
    config.pin_d1 = D1_GPIO_NUM;
    config.pin_d2 = D2_GPIO_NUM;
    config.pin_d3 = D3_GPIO_NUM;
    config.pin_d4 = D4_GPIO_NUM;
    config.pin_d5 = D5_GPIO_NUM;
    config.pin_d6 = D6_GPIO_NUM;
    config.pin_d7 = D7_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 10000000;
    config.frame_size = FRAMESIZE_QVGA;  // 320*240
    config.pixel_format = PIXFORMAT_JPEG; // for streaming
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.jpeg_quality = 15;
    // 让摄像头使用psram来存储摄像头的数据
    if (psramFound()){
        Serial.printf("PSRAM found, using it for camera buffers\n");
        config.fb_count = 2;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }
    else{   
        Serial.printf("PSRAM not found\n");
        config.fb_count = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
    }
    Serial.println("Initializing camera...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        Serial.printf("Camera init failed with error: 0x%x\n", err); // 打印错误码
        delay(100);
        camera_ok = false;
        esp_camera_deinit();
        return;
    }
    camera_ok = true;
    Serial.println("Camera init okay");
    sensor_t *s = esp_camera_sensor_get();
    // 针对 OV3660 的设置
    if (s->id.PID == OV3660_PID)
    {
        s->set_vflip(s, 0);       // flip it back
        s->set_hmirror(s, 1); // 开启水平镜像
        s->set_brightness(s, 1);  // up the brightness just a bit
        s->set_saturation(s, -2); // lower the saturation
    }
    // 针对 OV2640 的设置
    if (s->id.PID == OV2640_PID)
    {
        // 旋转 180 度 = 垂直翻转 + 水平镜像
        s->set_vflip(s, 0);   // 开启垂直翻转
        s->set_hmirror(s, 0); // 开启水平镜像
    }
}


void screen_draw_jpeg(const uint8_t* jpg_data, size_t len){
    if (jpg_data == nullptr || len == 0) return;
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);
    tft.startWrite();
    TJpgDec.drawJpg(0, 0, jpg_data, len);
    tft.endWrite();
}

void camera_loop(){ 
    if (!camera_ok) {
        return;
    }
    if (!is_buf_init) {
        init_rgb_buffer();
    }
    
    // 1. 获取 JPEG 帧
    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.printf("Camera capture failed\n");
        esp_camera_fb_return(fb); // 即使失败也要尝试归还
        fb = NULL;
        return;
    }
     if (fb->format == PIXFORMAT_JPEG) {
        // 2. 配置解码器
        if (is_buf_init){
            TJpgDec.setJpgScale(1); // 1:1 解码，不缩放
            TJpgDec.setSwapBytes(true); // 交换字节序 (Big/Little Endian)
            TJpgDec.setCallback(rgb_decode_callback); // 设置刚才写的回调
            TJpgDec.drawJpg(0, 0, fb->buf, fb->len);  // 这一步是阻塞的，耗时约 30-50ms
        }
    } else {
        Serial.println("Non-JPEG frame received!");
    }
    // 6. 释放摄像头帧缓冲
    esp_camera_fb_return(fb);
    fb = NULL;
}

// 应用摄像头翻转
void camera_apply_flip() {
    if (!camera_ok) return;
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        s->set_vflip(s, camera_vflip);
        s->set_hmirror(s, camera_hmirror);
    }
}

// 设置摄像头翻转
void camera_set_flip(bool vflip, bool hmirror) {
    camera_vflip = vflip;
    camera_hmirror = hmirror;
    camera_apply_flip();
    save_align_params();
}

void camera_cli(String command){
    command.trim();
    if (command.startsWith("toggle_vflip")) {
        camera_vflip = !camera_vflip;
        camera_apply_flip();
        save_align_params();
        Serial.printf("VFLIP:%d\n", camera_vflip);

    } else if (command.startsWith("toggle_hflip")) {
        camera_hmirror = !camera_hmirror;
        camera_apply_flip();
        save_align_params();
        Serial.printf("HFLIP:%d\n", camera_hmirror);

    } else if (command.startsWith("set_flip")) {
        int v = 0, h = 0;
        sscanf(command.c_str(), "set_flip %d %d", &v, &h);
        camera_set_flip(v == 1, h == 1);
        Serial.printf("OK: Flip set to V:%d H:%d\n", v, h);

    } else if (command.startsWith("check_camera")) {
        // 工厂测试：检查摄像头连接状态
        Serial.println("=== Camera Connection Test ===");
        if (camera_ok) {
            Serial.println("OK: Camera initialized successfully");
            sensor_t *s = esp_camera_sensor_get();
            if (s) {
                Serial.printf("INFO: Camera PID=0x%04X ", s->id.PID);
                if (s->id.PID == OV2640_PID) Serial.println("(OV2640)");
                else if (s->id.PID == OV3660_PID) Serial.println("(OV3660)");
                else Serial.println("(Unknown)");
            }
        } else {
            Serial.println("FAIL: Camera not initialized");
        }
        Serial.println("==============================");

    } else if (command.startsWith("test_camera")) {
        // 工厂测试：测试摄像头获取画面
        Serial.println("=== Camera Capture Test ===");
        if (!camera_ok) {
            Serial.println("FAIL: Camera not initialized");
            Serial.println("===========================");
            return;
        }
        camera_fb_t *test_fb = esp_camera_fb_get();
        if (test_fb) {
            Serial.println("OK: Frame captured successfully");
            Serial.printf("INFO: Frame size=%dx%d, format=%s, length=%d bytes\n",
                          test_fb->width, test_fb->height,
                          (test_fb->format == PIXFORMAT_JPEG) ? "JPEG" : "Other",
                          test_fb->len);
            esp_camera_fb_return(test_fb);
        } else {
            Serial.println("FAIL: Failed to capture frame");
        }
        Serial.println("===========================");
    }
}