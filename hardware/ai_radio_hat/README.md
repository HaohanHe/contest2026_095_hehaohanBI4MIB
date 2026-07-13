# AI Radio HAT for Gemini-S1

> 嘉立创 EDA / KiCad 双兼容硬件工程

## 设计目标

为 Gemini-S1 (Allwinner R528) 开发板设计一块 **AI 业余无线电扩展板**，通过 2.54mm 杜邦线/排针连接，实现：

1. **电台音频输入** → R528 LINEIN（给 ASR 语音识别）
2. **R528 音频输出** → 电台 MIC（用于 TTS/数字语音通联）
3. **PTT 控制**（R528 GPADC0 → 光耦隔离 → 电台 PTT）
4. **电台 CAT 串口**（R528 R_TX/R_RX → 电台 CAT）
5. **I2C 扩展接口**（R528 TWI2 PB8/PB9，接温湿度/光感等传感器）

## 文件说明

| 文件 | 说明 |
|---|---|
| `BOM.md` | 物料清单 + 嘉立创搜索关键词 + 杜邦线连接表 |
| `schematic.md` | ASCII 原理图 + 各模块详细说明 |
| `ai_radio_hat_v1.json` | 嘉立创 EDA 标准版导入文件 |
| `ai_radio_hat_v1.kicad_pro` | KiCad 7 工程文件 |
| `ai_radio_hat_v1.kicad_sch` | KiCad 7 原理图 |
| `ai_radio_hat_v1.kicad_pcb` | KiCad 7 PCB（仅板框，需自行布局） |

## 使用方法

### 方式一：嘉立创 EDA 标准版（在线）

1. 打开 [嘉立创 EDA 标准版](https://lceda.cn/editor)
2. 文件 → 打开 → 选择 `ai_radio_hat_v1.json`
3. 系统会自动生成原理图符号，根据 `schematic.md` 补全连线
4. 转换到 PCB，按 80mm×50mm 板框布局
5. 一键下单打样 + SMT

### 方式二：KiCad 7

1. 安装 KiCad 7.0+
2. 双击 `ai_radio_hat_v1.kicad_pro`
3. 打开原理图，检查连线
4. 打开 PCB 进行布局布线
5. 导出 Gerber 上传到嘉立创下单

### 方式三：手工在嘉立创 EDA 里画（最稳）

如果 JSON 导入有问题，直接照着 `schematic.md` 和 `BOM.md` 在嘉立创 EDA 里画：
- 新建原理图
- 从标准库放元件（3.5mm 耳机座、PC817、TXS0102、2N7002、电阻电容等）
- 按 `schematic.md` 连接
- 生成 PCB，80mm×50mm

## 与 Gemini-S1 的接线

| 扩展板 | Gemini-S1 |
|---|---|
| LINEIN_L | 顶部粉色 `LINEIN` |
| HPOUT_L | 顶部绿色 `HPOUT` |
| CAT-TX | 底部排针 `R_TX` |
| CAT-RX | 底部排针 `R_RX` |
| PTT_IN | 右侧排针 `GPADC0` |
| TWI2_SDA | 左侧 `TWI2_SDA (PB9)` |
| TWI2_SCK | 左侧 `TWI2_SCK (PB8)` |
| 3.3V / GND | 对应 3.3V / GND |

## 板子尺寸

- **80mm × 50mm**
- **2 层板**
- **1.6mm 厚度**
- 四角 M3 安装孔

## 后续软件配合

硬件接上后，软件端需要确认：

1. R528 音频输入设备为 `/dev/audio/pcm0c`
2. ASR 采样率配置为 16kHz（通过 AUDIOIOC_CONFIGURE 设置）
3. PTT GPIO 对应 `GPADC0`（需在代码里配置）
4. CAT 串口对应 `/dev/ttyS?`（通常是 R_TX/R_RX 对应的 UART）

详见 `/workspace/app/ai_radio_console/` 软件工程。
