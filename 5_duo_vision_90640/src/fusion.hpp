#pragma once

#include <Arduino.h>
#include "screen.hpp"
#include "shared_val.hpp"
#include "color_map.hpp"
#include "BilinearInterpolation.hpp"
#include "camera.hpp"
#include "mlx_drivers/mlx_probe.hpp"
#include <TJpg_Decoder.h>

// ================= 融合模式渲染器 =================

// JPEG 解码回调 - 用于摄像头图像
static uint16_t* camera_frame_buffer = nullptr;  // PSRAM 中的摄像头帧缓冲区
static bool camera_frame_ready = false;

// 临时缓冲区用于画中画渲染
static uint16_t pip_thermal_buffer[PIP_WIDTH * PIP_HEIGHT];

// 全屏 Sprite（在 PSRAM 中创建）用于 alpha 融合
static TFT_eSprite* fusion_sprite = nullptr;

// 画中画模式用的全屏 Sprite（在 PSRAM 中创建）
static TFT_eSprite* pip_sprite = nullptr;

// 边缘提取融合：灰度与边缘强度缓冲区（PSRAM，仅 Thermal Overlay 模式使用）
static uint8_t* edge_gray_buffer = nullptr;   // 320*240 亮度
static uint8_t* edge_mag_buffer = nullptr;    // 320*240 Sobel 幅值

// 边缘融合开关与阈值：1=仅边缘叠加（细节增强），0=全图透明叠加（原逻辑）
#define FUSION_EDGE_ONLY 1
#define EDGE_THRESHOLD   24   // Sobel 幅值低于此不叠加可见光，避免噪声

// JPEG 解码回调函数 - 将解码后的图像存入缓冲区
// 使用静态指针，这样可以在不修改回调签名的情况下访问 buffer
static uint16_t* decode_target_buffer = nullptr;
static int decode_buffer_width = 320;
static int decode_buffer_height = 240;

bool camera_decode_callback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (decode_target_buffer == nullptr) return 0;
    if (y >= decode_buffer_height) return 0;

    // 将解码的图像块复制到帧缓冲区
    for (int16_t row = 0; row < h; row++) {
        int16_t target_y = y + row;
        if (target_y >= decode_buffer_height) break;
        for (int16_t col = 0; col < w; col++) {
            int16_t target_x = x + col;
            if (target_x >= decode_buffer_width) break;
            decode_target_buffer[target_y * decode_buffer_width + target_x] = bitmap[row * w + col];
        }
    }
    return 1;
}

// 分配摄像头帧缓冲区及边缘融合用灰度/边缘缓冲
void init_fusion_buffers() {
    if (camera_frame_buffer == nullptr) {
        camera_frame_buffer = (uint16_t*)ps_malloc(320 * 240 * sizeof(uint16_t));
        if (camera_frame_buffer) {
            Serial.println("[Fusion] Camera frame buffer allocated in PSRAM");
        } else {
            Serial.println("[Fusion] Failed to allocate camera frame buffer!");
        }
    }
#if FUSION_EDGE_ONLY
    if (edge_gray_buffer == nullptr) {
        edge_gray_buffer = (uint8_t*)ps_malloc(320 * 240);
        if (edge_gray_buffer) Serial.println("[Fusion] Edge gray buffer allocated");
    }
    if (edge_mag_buffer == nullptr) {
        edge_mag_buffer = (uint8_t*)ps_malloc(320 * 240);
        if (edge_mag_buffer) Serial.println("[Fusion] Edge magnitude buffer allocated");
    }
#endif
}

// 初始化全屏 Sprite（PSRAM），用于 alpha 融合
void init_fusion_sprite() {
    if (fusion_sprite == nullptr) {
        fusion_sprite = new TFT_eSprite(&tft);
        // 在 PSRAM 中创建 320x240 的 Sprite
        if (fusion_sprite->createSprite(320, 240)) {
            Serial.println("[Fusion] Fusion sprite created in PSRAM: 320x240");
        } else {
            Serial.println("[Fusion] Failed to create fusion sprite!");
            delete fusion_sprite;
            fusion_sprite = nullptr;
        }
    }
}

// 初始化画中画模式用的全屏 Sprite
void init_pip_sprite() {
    if (pip_sprite == nullptr) {
        pip_sprite = new TFT_eSprite(&tft);
        // 在 PSRAM 中创建 320x240 的 Sprite
        if (pip_sprite->createSprite(320, 240)) {
            Serial.println("[Fusion] PIP sprite created successfully: 320x240");
        } else {
            Serial.println("[Fusion] Failed to create PIP sprite!");
            delete pip_sprite;
            pip_sprite = nullptr;
        }
    }
}

// 释放摄像头帧缓冲区及边缘缓冲
void free_fusion_buffers() {
    if (camera_frame_buffer != nullptr) {
        free(camera_frame_buffer);
        camera_frame_buffer = nullptr;
        Serial.println("[Fusion] Camera frame buffer freed");
    }
#if FUSION_EDGE_ONLY
    if (edge_gray_buffer != nullptr) {
        free(edge_gray_buffer);
        edge_gray_buffer = nullptr;
    }
    if (edge_mag_buffer != nullptr) {
        free(edge_mag_buffer);
        edge_mag_buffer = nullptr;
    }
#endif
    if (fusion_sprite != nullptr) {
        fusion_sprite->deleteSprite();
        delete fusion_sprite;
        fusion_sprite = nullptr;
        Serial.println("[Fusion] Fusion sprite deleted");
    }
    if (pip_sprite != nullptr) {
        pip_sprite->deleteSprite();
        delete pip_sprite;
        pip_sprite = nullptr;
        Serial.println("[Fusion] PIP sprite deleted");
    }
}

// 捕获并解码摄像头帧到缓冲区
bool capture_camera_frame() {
    if (!camera_ok) return false;
    if (camera_frame_buffer == nullptr) {
        init_fusion_buffers();
    }

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[Fusion] Camera capture failed");
        return false;
    }

    if (fb->format == PIXFORMAT_JPEG) {
        // 设置目标缓冲区
        decode_target_buffer = camera_frame_buffer;
        decode_buffer_width = 320;
        decode_buffer_height = 240;

        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(false);  // 与原始代码保持一致
        TJpgDec.setCallback(camera_decode_callback);

        // 清空头几行，确保解码从干净状态开始
        memset(camera_frame_buffer, 0, 320 * 240 * sizeof(uint16_t));

        // 解码 JPEG 到缓冲区
        TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
        camera_frame_ready = true;
    }

    esp_camera_fb_return(fb);
    fb = NULL;
    return camera_frame_ready;
}

// RGB565 颜色字节序交换（大端 <-> 小端）
inline uint16_t swap_bytes(uint16_t color) {
    return (color >> 8) | (color << 8);
}

// Alpha 混合函数 - 混合两个 RGB565 颜色
// alpha: 0-255，0 表示完全使用背景色，255 表示完全使用前景色
inline uint16_t alpha_blend(uint16_t bg_color, uint16_t fg_color, uint8_t alpha) {
    if (alpha == 0) return bg_color;
    if (alpha == 255) return fg_color;

    // 1. 还原大小端 (因为 TFT_eSprite 存储的是字节交换后的 RGB565)
    bg_color = (bg_color << 8) | (bg_color >> 8);
    fg_color = (fg_color << 8) | (fg_color >> 8);

    // 提取 RGB565 的各分量
    uint8_t r1 = (bg_color >> 11) & 0x1F;
    uint8_t g1 = (bg_color >> 5) & 0x3F;
    uint8_t b1 = bg_color & 0x1F;

    uint8_t r2 = (fg_color >> 11) & 0x1F;
    uint8_t g2 = (fg_color >> 5) & 0x3F;
    uint8_t b2 = fg_color & 0x1F;

    // Alpha 混合
    uint8_t r = (r1 * (255 - alpha) + r2 * alpha) / 255;
    uint8_t g = (g1 * (255 - alpha) + g2 * alpha) / 255;
    uint8_t b = (b1 * (255 - alpha) + b2 * alpha) / 255;

    // 重组颜色
    uint16_t result = (r << 11) | (g << 5) | b;

    // 4. 再次交换字节序，以便存储到 TFT_eSprite 缓冲区
    return (result << 8) | (result >> 8);
}

// 从 RGB565 提取亮度值（用于判断是否为高温区域）
// 若 color 为 TFT 字节交换后的 RGB565，先 swap_bytes 再传入
inline uint8_t get_luminance(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    return (r + g + b) / 3;
}

#if FUSION_EDGE_ONLY
// 将 RGB565 帧转为灰度（亮度），cam 为 TFT 字节序，需先还原再取 Y
inline void rgb565_to_gray(uint16_t* cam, uint8_t* gray, int w, int h) {
    const int n = w * h;
    for (int i = 0; i < n; i++) {
        uint16_t c = (cam[i] << 8) | (cam[i] >> 8);
        gray[i] = get_luminance(c);
    }
}

// Sobel 边缘提取，整数运算，幅值 = |Gx|+|Gy| 截断到 0~255
// gray/edge 均为 w*h，边界 1 像素不计算（置 0）
inline void sobel_edge(uint8_t* gray, uint8_t* edge, int w, int h) {
    const int stride = w;
    memset(edge, 0, (size_t)(w * h));
    for (int y = 1; y < h - 1; y++) {
        uint8_t* row_m1 = gray + (y - 1) * stride;
        uint8_t* row_0  = gray + y * stride;
        uint8_t* row_p1 = gray + (y + 1) * stride;
        uint8_t* out   = edge + y * stride;
        for (int x = 1; x < w - 1; x++) {
            int gx = (int)row_m1[x + 1] - (int)row_m1[x - 1]
                   + 2 * ((int)row_0[x + 1] - (int)row_0[x - 1])
                   + (int)row_p1[x + 1] - (int)row_p1[x - 1];
            int gy = (int)row_p1[x - 1] - (int)row_m1[x - 1]
                   + 2 * ((int)row_p1[x] - (int)row_m1[x])
                   + (int)row_p1[x + 1] - (int)row_m1[x + 1];
            int mag = (gx < 0 ? -gx : gx) + (gy < 0 ? -gy : gy);
            out[x] = (uint8_t)(mag > 255 ? 255 : mag);
        }
    }
}
#endif

// 渲染热成像到 Sprite（解决闪烁问题）
void render_thermal_to_sprite(TFT_eSprite* sprite, int width, int height) {
    Serial.println("[DEBUG] render_thermal_to_sprite start");
    // 安全检查
    if (sprite == nullptr) {
        Serial.println("[Fusion] Error: sprite is null");
        return;
    }
    Serial.println("[DEBUG] sprite ok");
    if (mlx90640To_buffer == nullptr) {
        Serial.println("[Fusion] Error: mlx90640To_buffer is null");
        return;
    }
    Serial.println("[DEBUG] mlx90640To_buffer ok");

    int cols = mlx_cols();
    int rows = mlx_rows();
    int scale = (int)mlx_scale();
    Serial.printf("[DEBUG] cols=%d, rows=%d, scale=%d\n", cols, rows, scale);

    // 限制渲染范围
    if (scale > 10) scale = 10;  // 防止过大的缩放
    Serial.printf("[DEBUG] after clamp scale=%d\n", scale);

    // 清空 Sprite
    Serial.println("[DEBUG] fillSprite start");
    sprite->fillSprite(TFT_BLACK);
    Serial.println("[DEBUG] fillSprite done");

    // 计算居中偏移
    int render_w = cols * scale;
    int render_h = rows * scale;
    int offset_x = (width - render_w) / 2;
    int offset_y = (height - render_h) / 2;
    Serial.printf("[DEBUG] render_w=%d, render_h=%d, offset_x=%d, offset_y=%d\n", render_w, render_h, offset_x, offset_y);

    // 边界检查
    if (offset_x < 0) offset_x = 0;
    if (offset_y < 0) offset_y = 0;

    // 渲染热成像到 Sprite（使用简单方法，更稳定）
    Serial.println("[DEBUG] render loop start");
    for (int i = 0; i < rows; i++) {
        int draw_y = i * scale + offset_y;
        if (draw_y >= height) break;

        for (int j = 0; j < cols; j++) {
            int draw_x = j * scale + offset_x;
            if (draw_x >= width) break;

            // 安全地访问缓冲区
            int buffer_idx = (rows - 1 - i) * cols + j;
            if (buffer_idx < 0 || buffer_idx >= cols * rows) {
                Serial.printf("[DEBUG] idx out of range: %d\n", buffer_idx);
                continue;
            }

            uint16_t color = colormap[mlx90640To_buffer[buffer_idx]];
            sprite->fillRect(draw_x, draw_y, scale, scale, color);
        }
    }
    Serial.println("[DEBUG] render loop done");

    // 注意：文本在主屏幕上绘制，避免 Sprite 字体问题
    Serial.println("[DEBUG] render_thermal_to_sprite end");
}

// ================= 模式 1: 仅摄像头 =================
void draw_camera_only() {
    if (!camera_ok) {
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(80, 110);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.println("Camera Error");
        return;
    }

    // 使用原始 camera_loop 的显示方式
    fb = esp_camera_fb_get();
    if (fb && fb->format == PIXFORMAT_JPEG) {
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(false);
        TJpgDec.setCallback(tft_output);
        tft.startWrite();
        TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
        tft.endWrite();
    }
    if (fb) {
        esp_camera_fb_return(fb);
        fb = NULL;
    }
}

// ================= 模式 2: 画中画 (摄像头主画面 + 热成像小窗) =================
// 使用 TFT_eSprite 作为全页缓冲，替代直接数组操作
void draw_pip_thermal() {
    // 安全检查
    if (mlx90640To_buffer == nullptr || !flag_sensor_ok) {
        draw_camera_only();
        return;
    }

    if (!camera_ok) {
        extern void draw();
        draw();
        return;
    }

    // 初始化 PIP Sprite（用于全页缓冲）
    if (pip_sprite == nullptr) {
        init_pip_sprite();
    }
    if (pip_sprite == nullptr) {
        // Sprite 分配失败，回退到直接渲染
        draw_camera_only();
        return;
    }

    // 清空 Sprite 为黑色背景
    pip_sprite->fillSprite(TFT_BLACK);

    // 1. 获取摄像头帧并直接解码到 Sprite 内存
    fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_JPEG) {
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        extern void draw();
        draw();
        return;
    }

    // 直接解码到 Sprite 的缓冲区，省去中间拷贝
    uint16_t* sprite_buffer = (uint16_t*)pip_sprite->getPointer();
    if (sprite_buffer == nullptr) {
        esp_camera_fb_return(fb);
        fb = NULL;
        draw_camera_only();
        return;
    }

    decode_target_buffer = sprite_buffer;
    decode_buffer_width = 320;
    decode_buffer_height = 240;

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);  // TFT_eSprite 使用交换后的字节序
    TJpgDec.setCallback(camera_decode_callback);
    TJpgDec.drawJpg(0, 0, fb->buf, fb->len);

    esp_camera_fb_return(fb);
    fb = NULL;

    // 2. 在 Sprite 中绘制热成像小窗
    int cols = mlx_cols();
    int rows = mlx_rows();
    // 动态计算 scale，让热成像尽可能填满小窗
    int scale_x = PIP_WIDTH / cols;   // 128 / 32 = 4
    int scale_y = PIP_HEIGHT / rows;  // 96 / 24 = 4
    int scale = (scale_x < scale_y) ? scale_x : scale_y;  // 取较小值，确保不超出
    if (scale < 1) scale = 1;

    // 计算热成像偏移（在小窗中居中）
    int thermal_w = cols * scale;
    int thermal_h = rows * scale;
    int inner_offset_x = (PIP_WIDTH - thermal_w) / 2;
    int inner_offset_y = (PIP_HEIGHT - thermal_h) / 2;

    // 在小窗区域绘制黑色背景和白色边框
    pip_sprite->fillRect(PIP_POS_X, PIP_POS_Y, PIP_WIDTH, PIP_HEIGHT, TFT_BLACK);
    pip_sprite->drawRect(PIP_POS_X - 2, PIP_POS_Y - 2, PIP_WIDTH + 4, PIP_HEIGHT + 4, TFT_WHITE);

    // 渲染热成像到小窗区域
    for (int i = 0; i < rows; i++) {
        int draw_y = PIP_POS_Y + inner_offset_y + i * scale;
        if (draw_y >= 240) break;

        for (int j = 0; j < cols; j++) {
            int draw_x = PIP_POS_X + inner_offset_x + j * scale;
            if (draw_x >= 320) break;

            int buffer_idx = (rows - 1 - i) * cols + j;
            if (buffer_idx < 0 || buffer_idx >= cols * rows) continue;

            uint16_t color = colormap[mlx90640To_buffer[buffer_idx]];
            pip_sprite->fillRect(draw_x, draw_y, scale, scale, color);
        }
    }

    // 3. 一次性推送整个 Sprite 到屏幕（无闪烁！）
    pip_sprite->pushSprite(0, 0);
}

// ================= 模式 3: 画中画 (热成像主画面 + 摄像头小窗) =================
// 使用 TFT_eSprite 作为全页缓冲
void draw_pip_camera() {
    // 安全检查：确保 MLX 数据已准备好
    if (mlx90640To_buffer == nullptr || !flag_sensor_ok) {
        return;
    }

    // 初始化 PIP Sprite
    if (pip_sprite == nullptr) {
        init_pip_sprite();
    }
    if (pip_sprite == nullptr) {
        return;
    }

    // 清空 Sprite 为黑色背景
    pip_sprite->fillSprite(TFT_BLACK);

    // 1. 渲染热成像主画面到 Sprite
    int cols = mlx_cols();
    int rows = mlx_rows();
    int scale = (int)mlx_scale();

    // 渲染热成像（居中）
    int thermal_w = cols * scale;
    int thermal_h = rows * scale;
    int offset_x = (320 - thermal_w) / 2;
    int offset_y = (240 - thermal_h) / 2;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            int buffer_idx = (rows - 1 - i) * cols + j;
            if (buffer_idx < 0 || buffer_idx >= cols * rows) continue;
            uint16_t color = colormap[mlx90640To_buffer[buffer_idx]];

            int start_x = offset_x + j * scale;
            int start_y = offset_y + i * scale;

            pip_sprite->fillRect(start_x, start_y, scale, scale, color);
        }
    }

    // 2. 如果有摄像头，在小窗位置绘制摄像头画面
    if (camera_ok && capture_camera_frame()) {
        // 缩放摄像头图像到小窗
        static uint16_t camera_small[PIP_WIDTH * PIP_HEIGHT];
        const int batch_size = 16;

        for (int y_batch = 0; y_batch < PIP_HEIGHT; y_batch += batch_size) {
            yield();
            delay(1);

            for (int y = y_batch; y < y_batch + batch_size && y < PIP_HEIGHT; y++) {
                for (int x = 0; x < PIP_WIDTH; x++) {
                    int src_x = x * 320 / PIP_WIDTH;
                    int src_y = y * 240 / PIP_HEIGHT;
                    if (src_x >= 320) src_x = 319;
                    if (src_y >= 240) src_y = 239;
                    camera_small[y * PIP_WIDTH + x] = camera_frame_buffer[src_y * 320 + src_x];
                }
            }
        }

        // 将小窗摄像头图像绘制到 Sprite（带白色边框）
        pip_sprite->drawRect(PIP_POS_X - 2, PIP_POS_Y - 2, PIP_WIDTH + 4, PIP_HEIGHT + 4, TFT_WHITE);

        // 小窗图像 - 使用 pushImage 更高效，并自动处理字节序
        // 注意：camera_small 中的数据需要字节序转换
        static uint16_t camera_swapped[PIP_WIDTH * PIP_HEIGHT];
        for (int i = 0; i < PIP_WIDTH * PIP_HEIGHT; i++) {
            uint16_t color = camera_small[i];
            camera_swapped[i] = (color << 8) | (color >> 8);
        }
        pip_sprite->pushImage(PIP_POS_X, PIP_POS_Y, PIP_WIDTH, PIP_HEIGHT, camera_swapped);
    }

    // 3. 一次性推送整个 Sprite 到屏幕（无闪烁！）
    pip_sprite->pushSprite(0, 0);
}


// ================= 模式 4: Alpha融合 (热成像叠加在摄像头上) =================
void draw_thermal_overlay() {
    // 帧率计时
    static unsigned long dt = 0;
    static unsigned long last_time = 0;
    last_time = millis();

    // 安全检查：确保 MLX 数据已准备好
    if (mlx90640To_buffer == nullptr || !flag_sensor_ok) {
        draw_camera_only();
        return;
    }

    if (!camera_ok) {
        extern void draw();
        draw();
        return;
    }

    // 初始化融合 Sprite
    if (fusion_sprite == nullptr) {
        init_fusion_sprite();
    }
    if (fusion_sprite == nullptr) {
        draw_camera_only();
        return;
    }

    // 1. 获取摄像头帧并解码到临时缓冲区
    fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_JPEG) {
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
        }
        extern void draw();
        draw();
        return;
    }

    static uint16_t* cam_buffer = nullptr;
    if (cam_buffer == nullptr) {
        cam_buffer = (uint16_t*)ps_malloc(320 * 240 * sizeof(uint16_t));
        if (cam_buffer == nullptr) {
            Serial.println("[Fusion] Failed to allocate cam buffer!");
            esp_camera_fb_return(fb);
            fb = NULL;
            draw_camera_only();
            return;
        }
    }

    decode_target_buffer = cam_buffer;
    decode_buffer_width = 320;
    decode_buffer_height = 240;

    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);  // 摄像头解码时需要交换字节序
    TJpgDec.setCallback(camera_decode_callback);
    TJpgDec.drawJpg(0, 0, fb->buf, fb->len);

    esp_camera_fb_return(fb);
    fb = NULL;

#if FUSION_EDGE_ONLY
    // 1.5 可见光边缘提取：灰度 -> Sobel，用于仅在边缘处叠加轮廓
    if (edge_gray_buffer && edge_mag_buffer) {
        rgb565_to_gray(cam_buffer, edge_gray_buffer, 320, 240);
        sobel_edge(edge_gray_buffer, edge_mag_buffer, 320, 240);
    }
#endif

    // 2. 渲染热成像到 Sprite 缓冲区（固定位置）
    uint16_t* sprite_buffer = (uint16_t*)fusion_sprite->getPointer();
    if (sprite_buffer == nullptr) {
        draw_camera_only();
        return;
    }

    fusion_sprite->fillSprite(TFT_BLACK);
    
    int cols = mlx_cols();
    int rows = mlx_rows();
    int scale = 10;

    // 渲染热成像到 sprite_buffer（参考 draw.hpp 实现：逐行扫描 + 双线性插值）
    int render_w = cols * scale;
    int render_h = rows * scale;

    // 计算居中偏移
    int offset_x = (320 - render_w) / 2;
    int offset_y = (240 - render_h) / 2;

    // 初始化插值表
    init_interp_tables(cols, rows, scale);

    // 使用双线性插值渲染整帧（参考 draw.hpp 的逐行扫描方式）
    for (int y = 0; y < render_h; y++) {
        int draw_y = offset_y + y;
        if (draw_y < 0 || draw_y >= 240) continue;

        for (int x = 0; x < render_w; x++) {
            int draw_x = offset_x + x;
            if (draw_x < 0 || draw_x >= 320) continue;

            // 使用双线性插值获取温度索引（与 draw.hpp 一致）
            int value = bio_linear_interpolation(x, render_h - 1 - y, mlx90640To_buffer, cols, rows);

            // 边界检查，防止 colormap 越界
            if (value < 0) value = 0;
            if (value > 179) value = 179;

            // 获取颜色并转换字节序（解决大小端问题）
            uint16_t thermal_color = colormap[value];
            thermal_color = (thermal_color << 8) | (thermal_color >> 8);

            // 写入缓冲区
            int pixel_idx = draw_y * 320 + draw_x;
            sprite_buffer[pixel_idx] = thermal_color;
        }
    }

    // 3. 执行仿射变换融合 - 只变换摄像头画面
    // 使用仿射变换参数 align_tx, align_ty, align_sx, align_sy, align_ang

    // --- 1. 预计算逆变换矩阵 ---
    // 角度转弧度 (逆向旋转所以取负)
    float rad = -align_ang * PI / 180.0f;
    float cos_a = cos(rad);
    float sin_a = sin(rad);

    // 缩放系数倒数
    float inv_sx = (align_sx != 0) ? (1.0f / align_sx) : 1.0f;
    float inv_sy = (align_sy != 0) ? (1.0f / align_sy) : 1.0f;

    // 屏幕中心 (目标)
    float dst_cx = 160.0f; // 320/2
    float dst_cy = 120.0f; // 240/2
    // 摄像头中心 (源, 320x240 的中心)
    float src_cx = 160.0f;
    float src_cy = 120.0f;

    // 矩阵系数 (Mapping Steps)
    // u = x * m00 + y * m01 + offset_u
    // v = x * m10 + y * m11 + offset_v
    float m00 = cos_a * inv_sx;
    float m01 = sin_a * inv_sx;
    float m10 = -sin_a * inv_sy;
    float m11 = cos_a * inv_sy;

    // 计算起始偏移量 (当 x=0, y=0 时的 u, v)
    // 变换公式推导: P_src = M_inv * (P_dst - Center_dst - Translate) + Center_src
    float dx0 = 0.0f - dst_cx - align_tx;
    float dy0 = 0.0f - dst_cy - align_ty;

    float start_u = dx0 * m00 + dy0 * m01 + src_cx;
    float start_v = dx0 * m10 + dy0 * m11 + src_cy;

    // --- 2. 逐行扫描渲染 (增量算法优化) ---
    for (int y = 0; y < 240; y++) {
        // 当前行的 u, v 起始值
        float u_f = start_u + y * m01;
        float v_f = start_v + y * m11;

        // 目标 Sprite 当前行的指针
        uint16_t* pSpriteLine = sprite_buffer + y * 320;

        for (int x = 0; x < 320; x++) {
            // 取整数坐标 (最近邻插值)
            int u = (int)u_f;
            int v = (int)v_f;

            // 边界检查: 只有在摄像头范围内的点才进行融合
            if (u >= 0 && u < 320 && v >= 0 && v < 240) {
                // 获取摄像头像素
                uint16_t cam_pixel = cam_buffer[v * 320 + u];
                // 获取热成像像素 (背景)
                uint16_t therm_pixel = *pSpriteLine;

                int therm_x = x / scale;
                int therm_y = y / scale;
                int therm_idx = therm_y * cols + therm_x;

                if (therm_x < cols && therm_y < rows && therm_idx < cols * rows) {
                    uint8_t alpha;
#if FUSION_EDGE_ONLY
                    // 细节增强：仅在可见光边缘处叠加轮廓，避免全图虚影
                    if (edge_mag_buffer) {
                        uint8_t edge_mag = edge_mag_buffer[v * 320 + u];
                        alpha = (edge_mag > EDGE_THRESHOLD)
                            ? (uint8_t)((uint32_t)edge_mag * fusion_alpha / 255)
                            : 0;
                    } else {
                        alpha = fusion_alpha;
                    }
#else
                    alpha = fusion_alpha;
                    if (alpha < 50) alpha = 50;
                    if (alpha > 255) alpha = 255;
#endif
                    uint16_t blended_color = alpha_blend(therm_pixel, cam_pixel, alpha);
                    *pSpriteLine = blended_color;
                }
            }
            // else: 超出范围的地方保持热成像原样

            // 增量步进 X
            u_f += m00;
            v_f += m10;
            pSpriteLine++;
        }
    } 
    // 4. 推送 Sprite 到屏幕
    tft.startWrite();
    fusion_sprite->pushSprite(0, 0);
    tft.endWrite();

    // 5. 在屏幕上直接绘制温度信息（与纯热成像模式一致）
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(25, 220);
    tft.printf("max: %.2f  ", T_max_fp);
    tft.setCursor(25, 230);
    tft.printf("min: %.2f  ", T_min_fp);
    tft.setCursor(105, 220);
    tft.printf("avg: %.2f  ", T_avg_fp);
    tft.setCursor(105, 230);
    tft.printf("bat: %d %% ", vbat_percent);
    tft.setCursor(180, 220);
    tft.printf("bright: %d  ", brightness);
    tft.setCursor(180, 230);
    tft.printf("%d ms", dt);

    // 计算帧率
    dt = millis() - last_time;
}

// ================= 主渲染函数 =================
void draw_fusion();

// 获取当前模式名称 (提前声明，供 save_display_mode_to_eeprom 使用)
const char* get_display_mode_name() {
    switch (current_display_mode) {
        case MODE_THERMAL_ONLY:    return "Thermal Only";
        case MODE_CAMERA_ONLY:     return "Camera Only";
        case MODE_PIP_THERMAL:     return "PiP Thermal";
        case MODE_PIP_CAMERA:      return "PiP Camera";
        case MODE_THERMAL_OVERLAY: return "Thermal Overlay";
        default:                   return "Unknown";
    }
}

// 保存显示模式到 EEPROM
void save_display_mode_to_eeprom() {
    uint8_t mode = (uint8_t)current_display_mode;
    EEPROM.write(EEPROM_ADDR_DISPLAY_MODE, mode);
    EEPROM.commit();
    Serial.printf("[Fusion] Display mode saved to EEPROM: %d (%s)\n",
                  current_display_mode, get_display_mode_name());
}

// 从 EEPROM 读取显示模式
DisplayMode load_display_mode_from_eeprom() {
    uint8_t mode = EEPROM.read(EEPROM_ADDR_DISPLAY_MODE);
    // 验证模式值是否有效
    if (mode >= 0 && mode <= 4) {
        return (DisplayMode)mode;
    }
    // 无效值返回默认值
    return DEFAULT_DISPLAY_MODE;
}

// ================= 模式切换函数 =================
void set_display_mode(DisplayMode mode) {
    if (mode != current_display_mode) {
        current_display_mode = mode;
        Serial.printf("[Fusion] Display mode changed to: %d\n", mode);

        // 保存到 EEPROM
        save_display_mode_to_eeprom();

        // 根据模式分配/释放资源
        if (mode == MODE_PIP_CAMERA) {
            init_fusion_buffers();
        }

        // Alpha 融合模式需要全屏 Sprite
        if (mode == MODE_THERMAL_OVERLAY) {
            init_fusion_sprite();
        }

        // 如果切换到纯热成像，可以释放资源
        if (mode == MODE_THERMAL_ONLY) {
            // free_fusion_buffers();  // 可选
        }
    }
}

// 切换到下一个显示模式
void next_display_mode() {
    int next = (current_display_mode + 1) % 5;
    set_display_mode((DisplayMode)next);
}

// ================= 主渲染函数实现 =================
void draw_fusion() {
    switch (current_display_mode) {
        case MODE_THERMAL_ONLY:
            extern void draw();
            draw();
            break;

        case MODE_CAMERA_ONLY:
            draw_camera_only();
            break;

        case MODE_PIP_THERMAL:
            draw_pip_thermal();
            break;

        case MODE_PIP_CAMERA:
            draw_pip_camera();
            break;

        case MODE_THERMAL_OVERLAY:
            draw_thermal_overlay();
            break;

        default:
            extern void draw();
            draw();
            break;
    }
}
