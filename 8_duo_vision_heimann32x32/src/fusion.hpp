#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "shared_val.hpp"
#include "screen.hpp"
#include "BilinearInterpolation.h"
#include "color_map.hpp"
#include <math.h>

// ================= 融合配置参数 =================
// 融合区域尺寸（不旋转时：224宽 x 240高，对应旋转后的 240x224）
#define FUSION_W 224
#define FUSION_H 240

// 摄像头源分辨率
#define CAM_SRC_W 320
#define CAM_SRC_H 240

// 融合区域在屏幕上的位置
#define FUSION_X 0
#define FUSION_Y 0

// ================= 融合状态 =================
extern bool enable_fusion;       // 是否启用融合（定义在 shared_val.hpp）
bool fusion_initialized = false; // 是否已初始化

// 热成像缓冲区（用于融合）
uint16_t *thermal_buffer = nullptr;

// 融合用 Sprite (240x224)
TFT_eSprite fusion_spr = TFT_eSprite(&tft);

// ================= RGB565 混合算法 =================
// alpha: 0-255, 0=全背景, 255=全前景
inline uint16_t alpha_blend(uint16_t fg, uint16_t bg, uint8_t alpha) {
    // 1. 还原大小端
    fg = (fg << 8) | (fg >> 8);
    bg = (bg << 8) | (bg >> 8);

    // 2. 提取分量
    uint16_t fg_r = (fg >> 11) & 0x1F;
    uint16_t fg_g = (fg >> 5) & 0x3F;
    uint16_t fg_b = fg & 0x1F;

    uint16_t bg_r = (bg >> 11) & 0x1F;
    uint16_t bg_g = (bg >> 5) & 0x3F;
    uint16_t bg_b = bg & 0x1F;

    // 3. 混合
    uint8_t inv_alpha = 255 - alpha;
    uint16_t r = (fg_r * alpha + bg_r * inv_alpha) >> 8;
    uint16_t g = (fg_g * alpha + bg_g * inv_alpha) >> 8;
    uint16_t b = (fg_b * alpha + bg_b * inv_alpha) >> 8;

    // 4. 组合并再次交换
    uint16_t result = (r << 11) | (g << 5) | b;
    return (result << 8) | (result >> 8);
}

// ================= 初始化融合功能 =================
void fusion_init() {
    if (fusion_initialized) return;

    // 确保在默认旋转状态下创建 Sprite
    tft.setRotation(0);
    
    Serial.printf("[Fusion] Initializing... PSRAM found: %d\n", psramFound());

    // 检查 PSRAM 是否可用
    if (psramFound()) {
        thermal_buffer = (uint16_t *)heap_caps_malloc(FUSION_W * FUSION_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    } else {
        thermal_buffer = (uint16_t *)malloc(FUSION_W * FUSION_H * sizeof(uint16_t));
    }

    if (thermal_buffer == nullptr) {
        Serial.println("[Fusion] Failed to allocate thermal buffer!");
        enable_fusion = false;
        fusion_initialized = false;
        return;
    }
    Serial.printf("[Fusion] Thermal buffer allocated: %d bytes (%dx%d)\n", 
                  FUSION_W * FUSION_H * sizeof(uint16_t), FUSION_W, FUSION_H);

    // 创建融合 Sprite (240x224)
    fusion_spr.setColorDepth(16);
    fusion_spr.createSprite(FUSION_W, FUSION_H);
    if (fusion_spr.getPointer() == nullptr) {
        Serial.println("[Fusion] Failed to create sprite!");
        enable_fusion = false;
        fusion_initialized = false;
        return;
    }
    Serial.println("[Fusion] Sprite created successfully");

    // 默认开启融合（如果有 PSRAM）
    if (psramFound()) {
        enable_fusion = true;
        Serial.println("[Fusion] Enabled (PSRAM available)");
    } else {
        enable_fusion = false;
        Serial.println("[Fusion] Disabled (no PSRAM)");
    }

    fusion_initialized = true;
    Serial.println("[Fusion] Initialization complete");
}

// ================= 释放融合资源 =================
void fusion_deinit() {
    if (thermal_buffer != nullptr) {
        free(thermal_buffer);
        thermal_buffer = nullptr;
    }
    fusion_spr.deleteSprite();
    fusion_initialized = false;
    enable_fusion = false;
}

// ================= 将热成像数据渲染到缓冲区 =================
// 注意：由于不使用 setRotation(2)，sprite 是 224x240
// 热成像传感器 30x32 -> 放大 8 倍 -> 240x256
// 但只取 28 行 -> 240x224
// 现在要把 240x224 "竖着"塞进 224x240 的 sprite 里（即行列互换）
void render_thermal_to_buffer() {
    if (thermal_buffer == nullptr) return;

    int value = 0;
    uint16_t color = 0;
    uint16_t* pBuf = thermal_buffer;

    // sprite 是 224x240
    // x: 0-223 (对应热成像的 y，即 28行*8=224)
    // y: 0-239 (对应热成像的 x，即 30列*8=240)
    for (int sprite_y = 0; sprite_y < FUSION_H; sprite_y++) {
        for (int sprite_x = 0; sprite_x < FUSION_W; sprite_x++) {
            // 行列互换：sprite 的 x 对应热成像的 y，sprite 的 y 对应热成像的 x
            // 翻转240方向（therm_x）
            int therm_x = sprite_y;  // 翻转240方向
            // 翻转224方向（therm_y）
            int therm_y = 223 - sprite_x;  // 翻转224方向
            
            // 使用双线性插值
            value = bio_linear_interpolation(therm_x, therm_y, draw_pixel);
            
            if (value < 0) value = 0;
            if (value > 179) value = 179;

            color = colormap[value];
            color = (color >> 8) | (color << 8);
            *pBuf++ = color;
        }
    }
}

// ================= 仿射变换融合函数 =================
// 使用全局参数: align_tx, align_ty, align_sx, align_sy, align_ang
// 注意：sprite 是 224x240，摄像头 320x240 需要适配
void apply_affine_fusion(uint16_t* cam_data) {
    if (!enable_fusion || cam_data == nullptr) return;
    if (thermal_buffer == nullptr) return;

    uint16_t* pSpriteBuffer = (uint16_t*)fusion_spr.getPointer();
    if (!pSpriteBuffer) return;

    // --- 1. 预计算逆变换矩阵 ---
    float rad = -align_ang * PI / 180.0f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);
    
    float inv_sx = (align_sx != 0) ? (1.0f / align_sx) : 1.0f;
    float inv_sy = (align_sy != 0) ? (1.0f / align_sy) : 1.0f;

    // sprite 中心 (224x240)
    float dst_cx = FUSION_W / 2.0f;  // 112
    float dst_cy = FUSION_H / 2.0f;  // 120
    
    // 摄像头中心 (320x240)
    float src_cx = CAM_SRC_W / 2.0f;  // 160
    float src_cy = CAM_SRC_H / 2.0f;  // 120

    float m00 = cos_a * inv_sx;
    float m01 = sin_a * inv_sx;
    float m10 = -sin_a * inv_sy;
    float m11 = cos_a * inv_sy;

    float dx0 = 0.0f - dst_cx - align_tx;
    float dy0 = 0.0f - dst_cy - align_ty;
    
    float start_u = dx0 * m00 + dy0 * m01 + src_cx;
    float start_v = dx0 * m10 + dy0 * m11 + src_cy;

    // --- 2. 逐行扫描渲染 ---
    for (int y = 0; y < FUSION_H; y++) {
        float u_f = start_u + y * m01;
        float v_f = start_v + y * m11;
        
        uint16_t* pSpriteLine = pSpriteBuffer + y * FUSION_W;
        uint16_t* pThermLine = thermal_buffer + y * FUSION_W;

        for (int x = 0; x < FUSION_W; x++) {
            int u = (int)u_f;
            int v = (int)v_f;

            if (u >= 0 && u < CAM_SRC_W && v >= 0 && v < CAM_SRC_H) {
                uint16_t cam_pixel = cam_data[v * CAM_SRC_W + u];
                uint16_t therm_pixel = *pThermLine;
                *pSpriteLine = alpha_blend(cam_pixel, therm_pixel, fusion_alpha);
            } else {
                *pSpriteLine = *pThermLine;
            }

            u_f += m00;
            v_f += m10;
            pSpriteLine++;
            pThermLine++;
        }
    }
}

// ================= 合成并推送到屏幕 =================
// 注意：避免使用 setRotation(2)，像参考代码那样直接 pushSprite
void composite_and_push_fusion(uint16_t* cam_rgb_data) {
    if (!fusion_initialized) {
        return;
    }

    // 1. 将热成像渲染到缓冲区（已包含180度旋转）
    render_thermal_to_buffer();

    // 2. 将热成像推送到 sprite
    fusion_spr.pushImage(0, 0, FUSION_W, FUSION_H, thermal_buffer);

    // 3. 如果有摄像头数据，执行融合（在 sprite 上进行）
    if (cam_rgb_data != nullptr && enable_fusion) {
        apply_affine_fusion(cam_rgb_data);
    }

    // 4. 直接推送到屏幕（不使用 setRotation，因为旋转已在渲染时处理）
    // 注意：fusion_spr 在创建时是在 rotation 0 下，所以直接 pushSprite 即可
    fusion_spr.pushSprite(FUSION_X, FUSION_Y);
}

// ================= 简单的融合开关 =================
void fusion_set_enabled(bool enabled) {
    enable_fusion = enabled;
    Serial.printf("Fusion %s\n", enabled ? "enabled" : "disabled");
}

bool fusion_is_enabled() {
    return enable_fusion;
}

// ================= 切换融合状态 =================
void fusion_toggle() {
    fusion_set_enabled(!enable_fusion);
}
