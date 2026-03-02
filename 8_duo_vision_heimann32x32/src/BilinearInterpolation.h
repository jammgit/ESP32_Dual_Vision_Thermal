// 双线性插值 - LUT 优化版本
// 预计算 X/Y 轴映射表，运行时只需查表

#ifndef BIO_LINEAR_INTERPOLATION_H
#define BIO_LINEAR_INTERPOLATION_H

#include <Arduino.h>

// ================= 配置区域 =================
// 适配项目：源数据 30x32，目标 240x256 (8x 放大)
#define SRC_W 30           // 源数据宽度
#define SRC_H 32           // 源数据高度
#define SCALE 8            // 放大倍数
#define SCALE_SHIFT 3      // log2(8)

// ================= 查找表结构 =================
// 每个目标坐标对应：两个源索引 + 两个权重
struct InterpLUT {
    int8_t idx0;    // 左/上索引
    int8_t idx1;    // 右/下索引
    uint8_t w0;     // 权重0 (256 - frac)
    uint8_t w1;     // 权重1 (frac)
};

// X/Y 轴查找表（静态存储，只需初始化一次）
static InterpLUT lut_x[SRC_W * SCALE];  // 240 entries
static InterpLUT lut_y[SRC_H * SCALE];  // 256 entries
static bool _lut_initialized = false;

// ================= 初始化函数 =================
// 预计算每个目标坐标的源索引和权重
inline void bilinear_init() {
    if (_lut_initialized) return;

    // 1. 预计算 X 轴映射表 (0..239 -> 0..29)
    for (int x = 0; x < SRC_W * SCALE; x++) {
        // 源坐标：(x + 0.5) / 8 - 0.5
        int32_t src_pos_256 = ((x << 8) + 128) >> SCALE_SHIFT;  // *256 / 8 = *32
        src_pos_256 -= 128;  // -0.5 * 256
        
        int8_t idx = src_pos_256 >> 8;       // 整数部分
        uint8_t frac = src_pos_256 & 0xFF;   // 小数部分

        // 边界处理
        if (idx < 0) {
            lut_x[x].idx0 = 0; 
            lut_x[x].idx1 = 0;
            lut_x[x].w0 = 255; 
            lut_x[x].w1 = 0;
        } else if (idx >= SRC_W - 1) {
            lut_x[x].idx0 = SRC_W - 1; 
            lut_x[x].idx1 = SRC_W - 1;
            lut_x[x].w0 = 255; 
            lut_x[x].w1 = 0;
        } else {
            lut_x[x].idx0 = idx;
            lut_x[x].idx1 = idx + 1;
            lut_x[x].w1 = frac;
            lut_x[x].w0 = 255 - frac;
        }
    }

    // 2. 预计算 Y 轴映射表 (0..255 -> 0..31)
    for (int y = 0; y < SRC_H * SCALE; y++) {
        int32_t src_pos_256 = ((y << 8) + 128) >> SCALE_SHIFT;
        src_pos_256 -= 128;
        
        int8_t idx = src_pos_256 >> 8;
        uint8_t frac = src_pos_256 & 0xFF;

        if (idx < 0) {
            lut_y[y].idx0 = 0; 
            lut_y[y].idx1 = 0;
            lut_y[y].w0 = 255; 
            lut_y[y].w1 = 0;
        } else if (idx >= SRC_H - 1) {
            lut_y[y].idx0 = SRC_H - 1; 
            lut_y[y].idx1 = SRC_H - 1;
            lut_y[y].w0 = 255; 
            lut_y[y].w1 = 0;
        } else {
            lut_y[y].idx0 = idx;
            lut_y[y].idx1 = idx + 1;
            lut_y[y].w1 = frac;
            lut_y[y].w0 = 255 - frac;
        }
    }

    _lut_initialized = true;
}

// ================= 核心插值函数 =================
inline int bio_linear_interpolation(int dst_x, int dst_y, unsigned short src_data[32][32]) {
    // 查表获取 X/Y 轴的索引和权重
    const InterpLUT* lx = &lut_x[dst_x];
    const InterpLUT* ly = &lut_y[dst_y];

    // 获取四个角的像素值
    uint16_t v00 = src_data[lx->idx0][ly->idx0];
    uint16_t v01 = src_data[lx->idx0][ly->idx1];
    uint16_t v10 = src_data[lx->idx1][ly->idx0];
    uint16_t v11 = src_data[lx->idx1][ly->idx1];

    // 双线性插值：先水平，后垂直
    // result = (v00*w0x + v10*w1x) * w0y + (v01*w0x + v11*w1x) * w1y
    //        = v00*w0x*w0y + v10*w1x*w0y + v01*w0x*w1y + v11*w1x*w1y
    
    uint32_t result = (uint32_t)v00 * lx->w0 * ly->w0
                    + (uint32_t)v10 * lx->w1 * ly->w0
                    + (uint32_t)v01 * lx->w0 * ly->w1
                    + (uint32_t)v11 * lx->w1 * ly->w1;

    return result >> 16;  // 除以 256*256 = 65536
}

#endif
