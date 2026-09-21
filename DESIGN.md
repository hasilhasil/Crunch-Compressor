# Crunch Compressor — 设计文档

界面外壳沿用 Crunch EQ（同主题、同旋钮、同面板结构），显示区对标 FabFilter Pro-C 2：
滚动电平历史 + 拐点传输曲线 + 电平表。

---

## 1. 控件映射（EQ → Compressor）

UI 结构：上半部 DisplayView + 下半部 280px 控制面板（左列 STYLE 模块与
带标签的选择器、右列主旋钮排 + In/Mix/Out 排）。控件外观已参照数字
混响插件 UI 重制（主题、旋钮、字体、布局见 §6）；主题切换、窗口缩放行为与
APVTS 持久化方式同 EQ。

| EQ 控件 | Compressor 控件 | 参数 ID | 范围 / 默认 |
|---------|-----------------|---------|-------------|
| Type 下拉 | Knee：Hard / Soft | `knee` | 拐点宽 0 / 12 dB，默认 Soft |
| Slope 下拉 | Detector：Peak / RMS | `detector` | 默认 Peak |
| Freq 旋钮 | **Threshold** | `threshold` | −60…0 dB，默认 −20 dB |
| Gain 旋钮 | **Ratio** | `ratio` | 1…20 : 1，默认 4 : 1 |
| Q 旋钮 | **Attack** | `attack` | 0.1…200 ms（对数，中心 10 ms），默认 10 ms |
| （第 4 旋钮） | **Release** | `release` | 10…1000 ms（对数，中心 100 ms），默认 100 ms |
| Bypass 按钮 | Bypass（压缩器旁路） | `enabled` | Bool，默认 true |
| COLOUR 行 | **STYLE** 行（完全相同结构） | `colorMode` / `colorPosition` / `colorAmount` | Off/Warm/Cold/Clip，Pre/Post，0–100 % |
| Phase Mode 下拉 | Auto Gain：Off / On | `autoGain` | 实测压缩量快速平均（~100 ms）补偿，输出平均电平对齐干信号 |
| Quality 下拉 | Lookahead：Off / 1 ms / 3 ms | `lookahead` | 经 `setLatencySamples` 上报宿主 |
| Gain Scale / Analyzer 下拉 | （随频谱显示一并移除） | — | — |
| In / Out 旋钮 | 相同 | `inputGain` / `outputGain` | ±36 dB，默认 0 |
| （新增） | **Mix** | `mix` | 0–100 %，默认 100 %（全湿） |

EQ 的"频点拖拽 / 新增 / 删除 / 滚轮调 Q"交互随频段概念一并移除；显示区新增
KNEE 曲线开关。保留：旋钮双击复位（JUCE 内建）、Shift 微调（JUCE 内建）、
滚轮调旋钮值（JUCE 内建）、齿轮主题菜单、tooltip。

## 2. 主显示（Pro-C 2 风格）

```
┌───────────────────────────────────────────────┬────┐
│  ⚙  红色 GR 曲线（与左刻度同尺度）   GR 6.3 dB │ IN │
│ 0  ────────────────────────────────────────   │ OUT│
│ -6   ▁▂▃灰输入区▃▂▁▂▃▅▇█输出电平线▇▅▃▂▁      │    │
│ -12    KNEE                                  │    │
│       < CURVE >  白色拐点曲线 + 绿色动态层     │    │
└───────────────────────────────────────────────┴────┘
```

### 2.1 红色 GR 曲线（压缩量）

- GR 曲线与左侧电平刻度**共用同一套 dB 刻度**：主图区纵轴为 **0 dB（顶边）
  到 −60 dB（底边）**（`kGraphTopDb = 0`），GR 值 g dB 映射到 `dbToY(−g)`，
  即压缩量可以直接按左轴刻度读数；每 6 dB 一条网格线与标签（`kAxisStepDb`）。
- 大量压缩（旧版 24 dB 触顶之后）曲线只是沿同一刻度向图内延伸，不会被
  条带边界截断；最深可读到 −60 dB。
- 顶部留出 `topInset()`（`max(28px, 10%高)`）给 GR 读数与设置按钮，
  GR = 0 时红线贴在该内缩顶边。
- 数据来自电平历史的 `histGr_`（压缩器实际施加的增益衰减，旁路时为 0，
  曲线贴顶）。GR 曲线画在电平历史**之后**，避免被半透明输入填充遮住。
- 右上角另有 `GR x.x dB` 数字读数。

### 2.2 主图：滚动电平历史（约 5 秒窗口）

- **记录**：`processBlock` 内单一逐样本主循环（输入增益 → 染色(Pre) → 压缩 →
  染色(Post) → 输出增益）中，每 `historyStride_`（≈0.5 ms）记录一个点：
  `inDb` = 检测器看到的电平（后输入增益、前压缩；旁路时取峰值 dB）、
  `outDb` = 压缩+染色后、输出增益前的电平、`grDb` = 当前增益衰减。
  电平点先经 **4 ms 一阶包络**平滑（`displayInEnvDb_`/`displayOutEnvDb_`）再入环——
  原始 2 kHz 采样按列抽稀时窗口滑动会导致“沸腾”抖动，包络化后滚动稳定；
  电平表仍使用块峰值（`getInputPeakDb`），不受影响。
- **环形缓冲**：16384 点容量（≈8 s），预分配，音频线程只写、UI 线程只读，
  靠 `std::atomic<int> historyCounter_`（写后 release / 读前 acquire，无符号回绕安全）
  同步，无锁。
- **读取**：UI 60 Hz Timer 调 `processor_.readHistory(10000, ...)` 取最近 ≈5 s，
  并维护**平滑滚动游标** `scrollCursor_`（对最新计数做 0.25 一阶平滑，
  ≈67 ms 滞后，**上下界都钳制**——宿主在播放/停止、改采样率/缓冲时会重跑
  `prepareToPlay` 把计数清零，上界钳制保证旧游标不会残留到数据之前导致
  图形消失）。宿主消息循环 tick 间隔不规则会让滚动步长在 2–4 px/帧间
  跳动（电平表因运动缓慢+自带衰减不敏感），游标平滑吸收该抖动后波形
  匀速滑行。
- **绘制**（`DisplayView::drawHistoryGraph`）：x 采用**固定比例的时间映射**
  （`pxPerPoint = plotW/(kHistoryPoints-1)`，1 个历史点恒定占 pxPerPoint
  像素，最新绘制点在右边缘）——滚动是**纯平移**（恒定 px/ms），历史填充
  期间图形从右侧向左生长，**不做任何横向压缩/缩放**，稳态后也不再
  逐帧微缩放。按**每像素列抽稀**取列内极值（≤ ~860 点，开销降一个数量
  级），但每个点画在**极值样本的精确亚像素 x** 上（而非列的整数网格
  位置）——峰值 x 量化到 1px 列网格会在滚动时“爬行”，表现为峰值错位
  抖动；Y 同为亚像素。
- **绘制**（`DisplayView::drawHistoryGraph`）：两层，**颜色与样式照搬 Crunch EQ
  的频谱两层**，但信号源与层的对应关系不变：
  - 第 1 层（下）= EQ 灰层样式：**输入电平**（`histIn_`）→ `CrunchPalette::inputFill`
    渐变填充（顶 α0.42 → 底 α0.08）+ 1px 同色描边（α0.55）。
  - 第 2 层（上）= EQ 主题色层样式：**输出电平**（`histOut_`）→ 主题 `accent`
    渐变填充（顶 α0.65 → 底 α0.10）+ **2px 白色描边**（α0.95）。
  - 视觉语义与 EQ 一致：增益（makeup）处主题色层露出在灰层之外，压缩处灰层
    露出在主题色层之外。
  - 稳态时因 ~67ms 滚动滞后，图形左缘距面板左缘约 11px。

### 2.3 双层拐点传输曲线

- `processor_.getTransferCurve(−60, +6, 128, ...)`：静态传输函数
  （`out = in + getAppliedGainDb(in)`，含 Auto Gain makeup），严格单调递增。
- **底层（白色静态层）**：完整静态传输曲线，α0.55，始终不动（亮色主题自动换深色）。
- **顶层（绿色动态层）**：同一条曲线**从底部原点到当前输出电平位置的一段**
  （`drawTransferCurve` 内在曲线上扫描输出电平的插值位置）。端点位置取
  **最近 50ms 窗口内的输出电平峰值**（`kGrHoldMs`，峰值保持），随压缩后的
  音量沿白曲线平滑滑动——安静时绿段短、音量大时绿段长、压缩越强绿段越短，
  效果等同峰值保持电平表在曲线内跳动，且不会随波形涟漪抖动。
- x 轴用 `dbToX`（与 y 轴同一 dB→像素映射，1:1 线呈左下-右上对角线）。
- **开关**：显示区右侧 `< CURVE >` 按钮（toggle，on 时强调色药丸填充），
  上方带 `KNEE` 小标签；`paint` 中据 `getToggleState()` 决定是否绘制。

### 2.4 电平表与读数
- 右侧 60px 条带，样式与 Crunch EQ 完全一致：
  - IN：`CrunchPalette::inputFill` 平色填充（α 0.35），原始输入峰值。
  - OUT：整条全高竖向渐变，顶端 = 主题 accent，底端 = 该主题的"当前颜色"
    （暗色蓝/红主题用 `CrunchPalette::outputLevel`，Cream 主题用
    `accent.darker(0.5)`）。
  - **顶部描边（峰值保持）**：描边跟随的是**保持的峰值**而非当前电平——
    新峰值瞬时跟上并重置 hold 计时；hold `kPeakHoldMs` = 2000 ms（1~3 s 区间取
    中间值）；到期后按 `kPeakFallDbPerSec` = 30 dB/s 回落，且
    `jmax(levelDb, …)` 保证永不低于当前电平，因此描边始终在柱条上方。
    描边颜色：暗色蓝/红主题为纯白（`juce::Colours::white`），Cream 主题为
    `CrunchPalette::inputFill` 灰。
  - 柱条本身弹道不变：瞬时上升、60 Hz 每 tick 衰减 1 dB；量程固定 −60…+6 dB
    （`kMeterTopDb/kMeterBottomDb`，与主图 0 dB 顶边的刻度无关，保留余量）。
- 峰值保持算法（`DisplayView::updatePeakHold`）与 EQ 插件逐行一致，dt 按真实
  tick 间隔测量，回落速率不受定时器频率影响；已用离线脚本验证：0 dB→−20 dB
  时柱 0.4 s 内掉到 −20 dB，描边保持 0.00 dB 整整 2.0 s，之后 30 dB/s 回落，
  3 s 后收敛到 −20 dB（= 当前电平）。
- 主图纵轴 0…−60 dB（每 6 dB 一线，0 dB 加粗，左侧每 6 dB 标注），
  纵向每 1/10 宽度一条浅色时间网格；主图顶部为 `topInset()` 内缩。

### 2.5 主题切换与按钮配色

- 主题切换时编辑器 `onThemeChanged` 显式 repaint 显示区、控制面板与自身
  （JUCE 的 Button 不保证响应 `lookAndFeedChanged`，否则按钮会保留旧主题
  的黑色底色）。
- TextButton 底色用 `track` 令牌（与面板 `panel` 区分），白色主题下按钮
  不再是“黑底黑字”。

## 3. 信号流

```
Input → 输入增益 → [染色(Pre)] → 压缩器 → [染色(Post)] → 输出增益 → Mix 干湿混合 → Output
                 ↑ colorPosition = Pre              ↑ colorPosition = Post
```

- 电平表 IN = 块入口原始峰值（前输入增益），OUT = 输出增益后峰值。
- MIX：`out = 湿×(m) + 干×(1−m)`，m = 0…1（参数 0–100 %，默认 1 = 全湿，
  行为与加 MIX 前完全一致）。干侧取**原始输入**（前输入增益），湿侧为
  整链输出；m 逐样本走 ~10 ms 一阶平滑（`mixSmoothed_` / `cachedMix_`），
  宿主自动化或拖动旋钮时不会在交叉点产生拉链噪声。OUT 电平表与输出峰值
  统计在混合**之后**，反映真实输出音量。
- 预前瞻开启时 `setLatencySamples(lookaheadSamples)`，宿主自动延迟补偿。
- 音频线程每块只做原子参数读取（`updateParameters()` 缓存差分），无锁无分配。

## 4. 压缩器内核（dsp/Compressor.h）

前馈式（feed-forward）结构，逐样本处理：

1. **检测**：立体声联动（取 |L|、|R| 较大者）；Peak 直取绝对值，RMS 走 10 ms
   一阶积分窗。电平转 dB（`gainToDecibels(x + 1e-9)`），记入 `lastDetectorDb_`。
2. **增益计算**（`computeGainDb`，dB 域）：
   - 硬拐点：`level > T` 时 `slope·(level − T)`，`slope = 1/R − 1`；
   - 软拐点：拐点内二次插值 `slope·over²/(2·W)`，两端与硬拐点公式连续。
3. **增益平滑**：目标增益比当前更负（GR 增大）走 Attack 系数，否则走 Release
   系数；一阶低通 `1 − exp(−1/(sr·τ))`。
4. **自动增益**（Auto Gain 开启）：`makeup = −avgGR`，其中 `avgGR` 为压缩器
   实际施加增益衰减的**快速平均（~100 ms，kAutoGainAverageSec）**，开环测量
   （检测器读输入，不会自激）。平均值窗口必须短：多秒级窗口会让开启 Auto Gain
   后（以及每次传输停止/重启 `reset()` 清零后）输出音量在数秒内渐变爬升，
   听感像淡入。当前 100 ms 下补偿在 ~0.3 s 内收敛，且平均值始终跟踪，
   音频已在播放时开启几乎立即到位。施加前再经 20 ms 去爆音平滑。
5. **预前瞻**：检测用当前样本、增益施加于延迟后的样本（环形缓冲）。

`getAppliedGainDb(levelDb)` = `computeGainDb + makeup`，即显示用的静态传输函数。

## 5. 从 EQ 复用而未改动的部分

- `dsp/BiquadFilter.h`（RBJ 双二阶 + `responseAt`）：**逐字节复用**。
- `dsp/ColorProcessor.h`：染色行为不变（Warm/Cold/Clip + 干湿混合 + Pre/Post）。
- 编辑器：默认 920×620、限制 700×500–1840×1240、底栏 280px、APVTS 状态
  （XML 二进制）持久化方式全部一致。

## 6. 底部面板 UI（280px，参照数字混响插件 UI 重制）

- **主题**：三套主题 Blue / Red / Cream，默认 **Blue**。Cream = 奶油暖色底
  （#F6F0E7）+ 珊瑚粉强调（#E07856）；Blue / Red 保持深色底，复用同一套
  控件绘制（白旋钮帽在深色底上同样成立）。
- **旋钮**：暖灰圆环轨道（刻度弧 270°，1.25π–2.75π）+ 强调色 value arc
  （3–4px 圆头）+ 近白旋钮帽（多层椭圆近似柔和投影）+ 深色圆头指针；
  弧长随归一化值顺时针增长。三档尺寸：主旋钮（≈115px）> 底栏/Amount
  （≈75–85px），主旋钮类比参考界面 Close/Mid/Far 一排。
- **排版**：嵌入 Poppins（Regular/Medium/SemiBold/Bold，`src/ui/Fonts/`，
`juce_add_binary_data` 打包）。knob 标签 SemiBold 16.5px（主行）/14.5px（底栏），
选择器文字标签（KNEE / DETECTOR / LOOKAHEAD）SemiBold 12.5px 弱化色，
STYLE 模块名 Bold 15.5px；数值读数在旋钮下方，Poppins 13.5px（经
`getLabelFont` 统一覆写，同时作用于下拉框文字）。按钮文字 14px
（`getTextButtonFont`），显示区网格标签 10.5px、GR 读数 12px、
电平表标签/读数 11/10px。
- **读数格式**（`Parameters.cpp` 的 `withStringFromValueFunction`）：
  dB 两位小数、ms 在 ≥1 时取整、百分比整数；单位后缀由 slider 追加。
- **底部面板布局**（`BandControlPanel::resized`，两列）：
  - **左列**（宽 190–280px）：顶部 **STYLE 模块**——模块名（SemiBold 14px
    强调色）在控件上方，下面是 Color Mode / Position 两个下拉（26px 高），
    再下面是 Amount 旋钮（标签上方、读数下方）；其后是两个带文字标签的
    选择器行：KNEE | DETECTOR、LOOKAHEAD，每个下拉上方有 13px 高的
    文字标签说明功能。
  - **右列上排**：Threshold / Ratio / Attack / Release 四个主旋钮
    （等分，强调色标签），右侧 Bypass 胶囊。
  - **右列下排**：In / Mix / Out 三个旋钮（等分），右侧 AUTO GAIN 胶囊
    （on 时强调色填充 + 柔光文字）。
- **Bypass 逻辑**：参数为 `enabled`（true = 压缩器工作），胶囊显示其**反相**
  ——未 bypass（enabled=true）时为主题“关闭”色（track 令牌 + 暗字），
  bypass 后填充主题强调色 + 近白文字。点击时 `setValueNotifyingHost`
  写入反相值，参数变化经 `AudioProcessorValueTreeState::Listener` 回同步
  胶囊状态（宿主自动化同样可驱动）。
- **其他控件**：下拉选择器为 8px 圆角浅色框 + 强调色三角箭头；TextButton
  （Bypass / KNEE / 设置齿轮）统一圆角胶囊，toggle on 时强调色填充、
  近白文字；设置（主题切换）按钮与 Crunch EQ 完全一致——一个
  `juce::TextButton`，文字为齿轮字符 U+2699（⚙），由
  `CrunchLookAndFeel::drawButtonBackground` 绘制成同款圆角胶囊。显示区网格
  标签、GR 读数、电平表文字同步切换为 Poppins。字体大小经
  `getLabelFont` / `getTextButtonFont` 统一覆写。

## 7. 已知边界

- 无预设系统（与 EQ 一致，宿主会话保存/恢复）。
- 主题与窗口尺寸仅存于进程内 atomic，不写入状态数据（与 EQ 一致）。
- 无 undo/redo、无 A/B、无键盘快捷键（与 EQ 一致）。
- 动态 EQ、Mid/Side、L/R 分声道压缩不在范围内。
