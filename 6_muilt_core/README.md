# ESP32 PlatformIO 实战教程 第六课：热成像的高性能编程

本节进阶课，我们将彻底突破单核性能天花板，教你如何解锁 ESP32 的 **双核计算能力** 构建高帧率热成像渲染管线！ 

[视频](https://www.bilibili.com/video/BV1dyZsBrEsV/)展示了双核下的渲染刷新率

---

## 🎯 本课核心知识点

* **FreeRTOS 双核任务调度**：将 MLX 传感器的数据采集与屏幕渲染彻底物理隔离。
* **内存屏障 (Memory Barrier) 与指针双缓冲**：解决多线程并发下的数据脏读与竞态条件。
* **消除棋盘格撕裂**：利用静态合成区 (Composition Buffer) 解决 MLX90640 子页 (Subpage) 交替刷新导致的画面闪烁。

---

## 🧠 核心技术深度解密

让我们深入拆解这份高水准代码，看看帧率是如何被一步步压榨出来的。

### 1. 物理隔离：FreeRTOS 双核调度 (`main.cpp`)

在传统的 `setup()` 和 `loop()` 架构中，ESP32 默认只使用了 Core 1。如果在这个核上同时运行 I2C 读取和 SPI 刷屏，两者的耗时会相互叠加，导致帧率断崖式下跌。

在 `main.cpp` 中，我们通过 `xTaskCreatePinnedToCore` 显式地唤醒了被闲置的 Core 0。

```cpp
// 在 Core 0 上创建独立的数据采集任务
xTaskCreatePinnedToCore(
    vTaskCore0,   /* 任务函数 */
    "vTaskCore0", /* 任务名称 */
    20000,        /* 堆栈大小 - 增加到 20KB 防止栈溢出 */
    NULL,         /* 参数 */
    1,            /* 优先级 */
    &Task1,       /* 任务句柄 */
    0);           /* 指定核心: 0 */
```

**架构优势：**
* **Core 0**：化身为无情的“抽水机”，在一个死循环 `for(;;){ loop1(); }` 中全力执行 `probe_loop_mlx()`。
* **Core 1**：专注于 UI 交互与屏幕渲染 (`screen_loop`, `button_loop`)，不再被缓慢的外设总线拖累。

### 2. 消除交错闪烁：静态合成区 (`mlx_probe.hpp`)

MLX90640 的硬件特性决定了它每一帧只能输出一半的像素（即所谓的 Subpage 0 和 Subpage 1，呈国际象棋的棋盘格交错分布）。如果你直接把读出来的数据推给屏幕，画面边缘会疯狂跳动。

在 `mlx_probe.hpp` 中，我们巧妙地使用了一个 `static float mlx_composition_buffer[834];` 静态数组。

```cpp
// 静态数组：用于保存完整的画面（合成缓冲区）
static float mlx_composition_buffer[834];

// MLX API 自动将当前 Subpage 填入合成区，与上一帧的另一半 Subpage 互补
MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx_composition_buffer);

// 将合成好的完整无闪烁画面，一次性拷贝给当前的写缓冲区
memcpy(currentWriteTarget, mlx_composition_buffer, sizeof(float) * 834);
```
**原理解析：** 这个静态区就像一个拼图板。无论 I2C 读回来的是黑格还是白格，都填入这个拼图板的对应位置。最终提交给渲染管线的，永远是一幅完整的复合画面。

### 3. 多核并发的护城河：无锁双缓冲与内存屏障 (`draw.hpp`)

既然两个核心在同时读写内存，如何保证 Core 1 渲染到一半时，数据不会被 Core 0 突然覆盖

代码中采用了一种策略：

1.  **准备两个内存区**：`pWriteBuffer` (供 Core 0 写入) 和 `pReadBuffer` (供 Core 1 读取)。
2.  **瞬间交换**：当 Core 0 写完一整帧后，只进行指针的交换，这个操作在 CPU 指令级极快。

```cpp
// Core 0 侧：瞬间交换指针
prob_lock = true;
volatile float* temp = pWriteBuffer;
pWriteBuffer = pReadBuffer;
pReadBuffer = temp;
prob_lock = false;
```

同时，在 `draw.hpp` 的 `screen_loop` 中：
```cpp
// 等待 Core 0 释放数据 - 使用内存屏障确保可见性
while (prob_lock != false) { delay(1); }
prob_lock = true;  // 防止指针交换的瞬间读取数组导致错误
float local_T_min = T_min_fp;
float local_T_max = T_max_fp;
```

### 4. 极致渲染：DMA 乒乓缓冲 (`draw.hpp`)

即使 Core 1 负责渲染，将 320x240 的像素推向 SPI 总线依然费时。在这里，我们启用了 **DMA (直接内存访问)** 技术。 

在 `draw.hpp` 的 `draw()` 函数中，定义了两个块级缓冲区：`dmaBuffer1` 和 `dmaBuffer2`。

```cpp
dmaBufferPtr = dmaBufferSel ? dmaBuffer2 : dmaBuffer1; // 切换指针
dmaBufferSel = !dmaBufferSel;

// pushImageDMA 是非阻塞的！
tft.pushImageDMA(0, y - now_y + 1, render_w, lines, lineBuffer, dmaBufferPtr);
```

**乒乓机制：**
当 DMA 控制器正在后台苦哈哈地把 `dmaBuffer1` 里的像素搬运到屏幕时，CPU 根本不需要等待！CPU 会立刻掉头，把下一行的双线性插值计算结果写入 `dmaBuffer2`。当 CPU 算完时，DMA 正好搬完第一块。两者完美交替，将 SPI 总线的利用率压榨到了接近 100%。

---

## 🚀 下一步计划

掌握了双核异构与 DMA 乒乓缓冲，你的底层渲染引擎已经具备了极高的吞吐量。之前的代码中对可见光摄像头 (OV2640) 的 JPEG 解码依然在使用同步等待的回调函数。你可以尝试在这个架构基础上，把 JPEG 解码也挂载到闲置的算力片段上，彻底打通双光数据的全并行流水线！