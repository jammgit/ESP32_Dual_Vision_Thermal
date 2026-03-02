#ifndef SHARED_VAL_H
#define SHARED_VAL_H

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>


// ====================== 引脚定义 =======================

// I2C 引脚定义
#define MLX_VDD  19
#define MLX_SDA  23
#define MLX_SCL  18

// ====================== 引脚定义 =======================

enum  {
    PROB_CONNECTING,
    PROB_INITIALIZING,
    PROB_PREPARING,
    PROB_READY
}; // 探头目前的启动状态码
// 海曼探头需要全局共享的内参
unsigned char nrofdefpix, gradscale, vddscgrad, vddscoff, epsilon, lastepsilon, arraytype;

// 共享变量
 uint8_t brightness = 185;  // 屏幕亮度
 unsigned short T_max = 0, T_min = 0;// 温度
 unsigned long  T_avg = 0; // 温度平均值
 float ft_max = 0, ft_min = 0; // 温度浮点数
 bool flag_in_photo_mode = false;
 bool flag_use_kalman = true; // 是否使用卡尔曼滤波
 bool flag_show_cursor = true; // 是否显示光标
 uint8_t prob_status = PROB_CONNECTING;  // 探头当前状态
 bool flag_sensor_ok = false;
 bool use_upsample = true; 
 bool flag_in_setting = false;
 bool config_loaded = false;

unsigned short data_pixel[32][32];
unsigned short buffer_pixel[32][32];
short cal_pixel[32][32] = {0};
unsigned short draw_pixel[32][32] = {0};

// 锁标志
 volatile bool prob_lock = false;
 volatile bool pix_cp_lock = false;
 volatile bool cmap_loading_lock = false;

uint16_t test_point[2] = {140, 120};  // 测温点的位置
// 电池电量百分比
 int vbat_percent = 100;
bool flag_clear_cursor = false; // 是否拍照模式下清除温度采集指针
float calibrate0_start = 0.;
float calibrate0_end = 100.;
float calibrate0_weight = 1.;
float calibrate0_bias = 0.;
bool camera_calibrated = false;
float calibrate1_start = 0.;
float calibrate1_end = 100.;
float calibrate1_weight = 1.;
float calibrate1_bias = 0.;

 bool color_reverse = false;  // 是否反转色彩
// 展示剩余内存, 总内存，以及使用的百分比情况
// 以kb为单位展示
 void print_heap_usage() {
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);

    Serial.printf("Internal heap: %u / %u bytes (%.2f%% used)\n",
        (unsigned int)(total_internal - free_internal), (unsigned int)total_internal,
        total_internal ?
        (float)(total_internal - free_internal) * 100.0f / total_internal : 0.0f);

    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t total_psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);

    Serial.printf("PSRAM: %u / %u bytes (%.2f%% used)\n",
        (unsigned int)(total_psram - free_psram), (unsigned int)total_psram,
        total_psram ?
        (float)(total_psram - free_psram) * 100.0f / total_psram : 0.0f);

}

// 仿射变换参数
float align_tx = 0.0;
float align_ty = 0.0;
float align_sx = 1.0;
float align_sy = 1.0;
float align_ang = 0.0;
bool camera_vflip = false;
bool camera_hmirror = false;
// 透明度 (0-255, 128 即 alpha=0.5)
uint8_t fusion_alpha = 128; 

// 融合功能开关
bool enable_fusion = true;

#endif
