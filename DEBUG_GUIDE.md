# Gemini-S1 AI Radio Console 调试指南

## 固件版本
- **v16**: gemini-s1_ai_radio_v16.img (25MB) - 基于官方defconfig，最小修改

## 启动流程

```
Boot ROM → U-Boot → NuttX Kernel → board_late_initialize() → NSH → rcS.nsh → luncher_mini
```

## 刷机步骤

### Windows (PhoenixSuit)
1. 安装PhoenixSuit
2. 连接开发板USB
3. 选择 gemini-s1_ai_radio_v16.img
4. 点击"升级"

### Linux (LiveSuit)
1. cd sunxi-livesuite-master
2. sudo ./LiveSuit.sh
3. 选择 gemini-s1_ai_radio_v16.img

## 调试方法

### 1. 串口调试 (推荐)
- **接口**: UART2 (1500000 baud)
- **连接**: USB转TTL连接开发板UART2引脚
- **工具**: minicom, screen, or PuTTY
```bash
# Linux
sudo minicom -D /dev/ttyUSB0 -b 1500000
# 或
screen /dev/ttyUSB0 1500000
```

### 2. ADB调试
```bash
adb devices          # 查看设备
adb shell            # 进入shell
adb logcat           # 查看日志
```

### 3. 检查启动是否成功
启动成功标志:
- 串口看到 "You're running an nsh image."
- 串口看到 "Boot nsh ok"
- 屏幕显示luncher_mini界面（5个窗口）
- ADB可以连接

## 常见问题

### 问题1: 完全无输出
- 检查电源是否正常
- 检查USB连接
- 检查固件是否正确刷入

### 问题2: 串口有输出但卡住
- 可能是文件系统挂载失败
- 检查 /dev/res 和 /dev/usrdata 是否存在
- 检查KVDB是否正常启动

### 问题3: ADB无法连接
- 检查CONFIG_SYSTEM_ADBD是否启用
- 检查USB连接
- 尝试重启设备

### 问题4: 屏幕无显示
- 检查LCD_ILI9341配置
- 检查SPI连接
- 检查背光

### 问题5: AI Radio无法启动
- 在串口执行 `ai_radio &`
- 检查ai_radio_main符号是否存在
- 检查luncher_mini是否正确集成

## 关键配置检查清单

```
CONFIG_GEMINI_S1_NSH=y          # NSH启动脚本
CONFIG_KVDB=y                   # KVDB数据库
CONFIG_BOARDCTL_RESET_CAUSE=y   # 重置原因检测
CONFIG_SYSTEM_ADBD=y            # ADB支持
CONFIG_LUNCHER_MINI_APP=y       # luncher_mini
CONFIG_LVX_USE_DEMO_CONTEST2026_095_AI_RADIO_CONSOLE=y  # AI Radio
CONFIG_LCD_ILI9341=y            # ILI9341 LCD驱动
CONFIG_AUDIO=y                  # 音频支持
CONFIG_SENSORS_SHTC3=y          # 温湿度传感器
CONFIG_SENSORS_LTR553=y         # 光感传感器
CONFIG_SCHED_LPWORK=y           # 低优先级工作队列
CONFIG_WIRELESS_WAPI=y          # WiFi支持
```

## 构建命令

```bash
# 清理
cd /home/bi4mib/openvela-build
rm -rf cmake_out/ nuttx/.config

# 构建
export PATH=/home/bi4mib/openvela-build/prebuilts/build-tools/linux-x86_64/bin:/home/bi4mib/openvela-build/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1

# 修复archive order bug
bash /run/media/bi4mib/新加卷/ontest2026_095_hehaohanBI4MIB/fix_archive_order.sh

# 重新链接
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay -j1

# 打包
cd vendor/allwinnertech/lichee && bash -c 'source envsetup.sh && lunch_nuttx <<< "2" && pack'
```
