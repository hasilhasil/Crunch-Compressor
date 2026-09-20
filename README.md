# Crunch Compressor

界面与交互对标本机 Crunch EQ（Pro-Q 风格）外壳、显示区对标 FabFilter Pro-C 2 的压缩器插件
（VST3 / Standalone），基于 C++20 / JUCE 8。完整设计见 [DESIGN.md](DESIGN.md)。

## 功能

- 基本压缩：Threshold（−60…0 dB）、Ratio（1…20:1）、Attack（0.1…200 ms）、
  Release（10…1000 ms），Hard / Soft 拐点，Peak / RMS 双检测模式，
  0 / 1 ms / 3 ms 预前瞻（自动上报宿主时延），Auto Gain 自动补偿。
- MIX 干湿比：0–100 %（默认 100 % = 全湿，保持旧行为）。湿信号为整条
  处理链（输入增益 → 染色 → 压缩 → 输出增益）输出，干信号取**原始输入**
  （输入增益前），即经典并行压缩；10 ms 平滑防自动化时产生拉链噪声。
- 底部控制面板参照数字混响插件 UI 重制：
  - 三套主题 Blue / Red / **Cream**（奶油暖色底 + 珊瑚粉强调色），默认
    **Blue**，可在显示区齿轮菜单切换；
  - 旋钮重绘：暖灰圆环轨道 + 强调色 value arc + 近白色旋钮帽（带柔和
    投影）+ 深色指针；标签在旋钮上方、数值读数在下方；
  - 主旋钮行（Threshold / Ratio / Attack / Release）尺寸最大（类比参考
    界面 Close/Mid/Far 一排），底栏 In / Mix / Out 与 Amount 次之；
  - 布局：左列为 **STYLE 模块**（模块名在控件上方）+ Color Mode / Position
    下拉 + Amount 旋钮，其下为 **KNEE / DETECTOR / LOOKAHEAD** 三个带
    文字标签的下拉选择器；右侧上排为主旋钮、下排为 In / Mix / Out；
  - Bypass 与 AUTO GAIN 为圆角胶囊按钮。**Bypass 未开启时为主题“关闭”
    色（track），开启后切换为主题强调色**（其状态取 `enabled` 参数的反相）；
  - 嵌入 Poppins 字体（SIL OFL 1.1，`src/ui/Fonts/`，经
    `juce_add_binary_data` 打包进插件二进制），不依赖宿主机系统字体；
  - 数值读数格式统一：dB 两位小数（`-0.00 dB`）、ms 取整
    （`10 ms` / `0.1 ms`）、百分比整数（`100 %`）；
  - 下拉选择器与 Bypass / Auto Gain / KNEE / 设置齿轮等改为圆角胶囊样式。
- 染色模块（STYLE）：Warm / Cold / Clip 三种模式，Pre / Post 位置，Amount 干湿混合，
  与 EQ 插件中的实现完全一致。
- Pro-C 2 风格主显示：
  - **红色增益衰减（GR）曲线**：与左侧电平刻度**共用同一套 dB 刻度**
    （0 dB 在刻度顶边），每 6 dB 一条网格线；压缩 X dB 的曲线位置正好落在
    "-X dB" 线上，大量压缩时曲线向图内延伸，不再有此前 24 dB 触顶截断的
    问题；
  - 主图滚动电平历史（约 5 s 窗口）：灰色输入电平填充区 + 浅色输出电平线；
  - 双层**拐点传输曲线**：白色静态层 + 绿色动态层（绿段从曲线底部原点到
    当前输出音量位置，像电平表填充一样在白曲线内上下跳动），
    右侧带 `KNEE` 标签的 `< CURVE >` 按钮控制显隐；
  - 右上角 GR 读数（`GR x.x dB`）；
  - 右侧 IN / OUT 峰值电平表（峰值保持 + 1 dB/tick 衰减，量程 −60…+6 dB）。
- 三套主题（Blue / Red / Cream），默认 Blue；窗口默认 920×620，缩放范围
  700×500–1840×1240，底部面板固定 280px，纯相对布局。

## 构建（Windows / Visual Studio）

仓库只包含插件源码，JUCE 8.0.4 由 CMake 首次配置时通过 FetchContent 从
GitHub 自动拉取（需要联网）：

```powershell
cmake -B build -A x64
cmake --build build --config Release
```

生成产物位于 `build/CrunchCompressor_artefacts/Release/VST3/Crunch Compressor.vst3`。

如果本机已有 JUCE 源码（例如本地缓存），可传 `-DJUCE_DIR` 离线构建：

```powershell
cmake -B build -A x64 -DJUCE_DIR="JUCE-8.0.4"
cmake --build build --config Release
```

## 运行

- VST3：将 `Crunch Compressor.vst3` 复制到 `C:\Program Files\Common Files\VST3\`，
  然后在 DAW 里重新扫描即可加载 "Crunch Compressor"。
- Standalone：直接运行构建出的独立可执行文件。

## 安装到宿主（每次重新构建后都要重新复制）

```powershell
Copy-Item -Recurse -Force `
  "build\CrunchCompressor_artefacts\Release\VST3\Crunch Compressor.vst3" `
  "C:\Program Files\Common Files\VST3\"
```

注意：本工程未启用 `COPY_PLUGIN_AFTER_BUILD`（自动复制到 Program Files 需要
管理员权限，且 DAW 占用插件时会文件锁失败）。**手动复制是唯一安装路径——重新
构建后若忘记复制，DAW 里调试的仍是旧二进制。** 复制前建议先关闭占用该插件的
DAW。

## 结构

```
src/
├── PluginProcessor.h/.cpp     DSP 主处理 + 电平历史环形缓冲 + 传输曲线 API
├── PluginEditor.h/.cpp        顶层编辑器
├── Parameters.h/.cpp          参数 ID / 枚举 / 布局工厂（含读数格式化）
├── dsp/
│   ├── BiquadFilter.h         RBJ 双二阶（与 EQ 完全相同）
│   ├── Compressor.h/.cpp      压缩器核心（检测 + 增益计算 + 拐点 + 预前瞻 + 自动增益）
│   └── ColorProcessor.h       染色模块（与 EQ 完全相同）
└── ui/
    ├── Fonts/                 Poppins TTF（SIL OFL 1.1，构建时嵌入）
    ├── LookAndFeel.h          主题 + CrunchPalette 调色板 + 旋钮/组合框/按钮/字体
    ├── DisplayView.h/.cpp     Pro-C 2 风格显示（电平历史 + 拐点曲线 + 电平表 + GR）
    └── BandControlPanel.h/.cpp 底部功能区（主旋钮行 + 染色行 + In/Mix/Out 全局行）
```
