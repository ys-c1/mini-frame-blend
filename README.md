# mini-frame-blend

一个很小的 C++17 RGBA8 帧混合 / 合成练习项目，带一个不依赖第三方库的自测程序。

## 文件布局

```
mini-frame-blend/
├── Makefile
├── README.md
├── src/
│   ├── blend.h
│   └── blend.cpp
└── tests/
    └── test_blend.cpp
```

## 构建与测试

要求：Linux/WSL + g++（支持 C++17）+ make。

```bash
make test
```

等价于构建 `build/test_blend` 并运行。测试程序最后输出：

```
test_blend: checks=38 failures=0
```

退出码为 0 表示全部通过，非 0 表示有失败。

## API

```cpp
void blend_over(const uint8_t* src, const uint8_t* dst,
                int w, int h, int x, int y, int mode);
void premultiply(const uint8_t* rgba, uint8_t* out, int npixels);
void unpremultiply(const uint8_t* rgba, uint8_t* out, int npixels);
```

- 所有像素都是紧密排列的 RGBA8，使用直通（straight / non-premultiplied）alpha。
- `src` 和 `dst` 都是 `w × h` 帧，`dst` 在函数内原地更新；API 保留了题目给定
  的 `const uint8_t* dst` 签名，因此实现内部用 `const_cast` 获取可写指针。
- `x, y` 是 `src` 在目的帧中的左上角偏移。完全落在目的帧外的部分会被裁剪，
  不会写到逻辑缓冲区 `w × h` 之外。
- `mode`：
  - `0`：直通，复制 `src` 像素；
  - `1`：source-over，直通 alpha 的 over 合成；
  - `2`：add，RGB 逐通道饱和相加，alpha 取两者较大值；
  - `3`：multiply，RGB 按 `(a*b + 127) / 255` 归一化相乘，alpha 取较大值。

## 测试覆盖

`tests/test_blend.cpp` 使用自行累计的 `CHECK` 统计断言，共 38 项：

1. 50% 红叠到不透明蓝：结果介于两者之间，alpha = 255；
2. 完全不透明 source 覆盖后与 source 逐字节相同；
3. 全透明 source 覆盖后与 destination 逐字节相同；
4. `premultiply` → `unpremultiply` 往返误差 ≤ 1（覆盖 alpha 128..255）；
5. 正偏移越界区域的裁剪，以及 destination 帧后哨兵字节未被改写；
6. mode 0 直通；
7. add 模式 RGB 饱和到 255；
8. multiply 模式归一化结果以及 255×255 饱和到 255。

> 说明：8 位预乘在 alpha 很小时会丢失颜色信息（例如 alpha = 1 时 256 个颜色
> 值只剩 2 个可表示值），因此往返测试覆盖 alpha ≥ 128，此时标准四舍五入实现
> 的最大误差为 1。

## 仓库状态

最终 `HEAD` 是**故意带 bug 的版本**，用于测试定位能力。正确版本在
`correct-baseline` 提交中；最终版本在其后注入了 B1、B2、B3 三处 bug。测试文件
未在注入 bug 时改动。
