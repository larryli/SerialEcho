# SerialEcho

串口设备模拟器 —— 基于 com0com 虚拟串口驱动的数据回环工具。

## 功能概述

- **串口回环**: 接收数据原样返回，用于测试串口通信
- **数据日志**: 实时显示 RX/TX 数据，HEX 格式，带时间戳
- **标准 SDI 界面**: 工具栏 + 状态栏 + 菜单

## 系统要求

- Windows 10/11 (x64)
- [com0com](https://sourceforge.net/projects/com0com/) 虚拟串口驱动
- Visual Studio Build Tools 或 Visual Studio（用于编译）

## 编译方法

```powershell
# 打开 VS 开发者命令行
# 具体路径根据安装位置调整

# 编译
mkdir build
cd build
cmake .. -G "NMake Makefiles"
cmake --build .
```

编译产物: `build\SerialEcho.exe`

## 使用方法

1. 安装 com0com 驱动，创建虚拟串口对（如 COM10 和 COM11）
2. 运行 SerialEcho.exe
3. 点击 **Serial > Connect** 或工具栏连接按钮，选择端口（如 COM10）
4. 使用串口工具连接另一端（如 COM11），发送数据
5. SerialEcho 会自动回环数据并显示日志

## 界面说明

### 菜单

| 菜单 | 项目 | 功能 |
|------|------|------|
| File | Exit | 退出程序（已连接时需确认） |
| Serial | Connect | 连接串口 |
| Serial | Disconnect | 断开串口 |
| Log | Clear | 清空日志 |
| Log | Save as... | 保存日志到文件 |
| Help | About | 关于对话框 |

### 工具栏

| 按钮 | 功能 |
|------|------|
| 连接 | 连接串口（已连接时禁用） |
| 断开 | 断开串口（未连接时禁用） |
| 清除 | 清空日志 |
| 保存 | 保存日志到文件 |

### 状态栏

| 栏位 | 内容 |
|------|------|
| 第1栏 | 保留 |
| 第2栏 | 当前端口名 或 "Disconnected" |
| 第3栏 | 串口参数 "115200,8N1" |

### 日志格式

```
2026-05-29 14:30:25.123 [RX] 48 65 6C 6C 6F 20 57 6F  72 6C 64 21 0D 0A
2026-05-29 14:30:25.124 [TX] 4F 4B 0D 0A
```

- 时间戳: `YYYY-MM-DD HH:MM:SS.mmm`
- 方向: `[RX]` 接收 / `[TX]` 发送
- 每字节2位HEX，空格分隔
- 每8字节额外空格分组
- 每16字节换行，续行对齐

## 项目结构

```
SerialEcho/
├── CMakeLists.txt      # CMake 构建配置
├── main.c              # 程序入口
├── gui.c / gui.h       # GUI 模块
├── serial.c / serial.h # 串口通信模块
├── trace.c / trace.h   # 跟踪日志模块
├── resource.h          # 资源 ID 定义
├── resource.rc         # 资源文件（菜单、对话框、位图）
├── app.ico             # 应用图标
├── toolbar.bmp         # 工具栏位图（4个16x16图标）
├── res1.xml            # 应用清单（Common Controls v6、DPI 感知）
├── .clang-format       # 代码格式配置
├── .editorconfig       # 编辑器配置
├── REQUIREMENTS.md     # 需求规格说明
└── README.md           # 本文件
```

## 串口配置

参数固定，不可修改：

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验 | 无 |
| 停止位 | 1 |

## 技术细节

- **语言**: C11
- **GUI**: Win32 API (RichEdit, Toolbar, Statusbar) - Wide (Unicode) 版本
- **串口**: WaitCommEvent 事件驱动 + Overlapped I/O
- **内存**: HeapAlloc/HeapFree (Windows 堆管理)
- **内部编码**: UTF-16 (Windows Wide API)
- **文件编码**: UTF-8 (无 BOM)
- **清单**: Common Controls v6、PerMonitorV2 DPI 感知

## 调试跟踪

启用跟踪日志记录到 `SerialEcho.log` 文件：

```powershell
cmake .. -G "NMake Makefiles" -DENABLE_TRACE=ON
cmake --build .
```

日志格式：`HH:MM:SS.mmm [ThreadID] [TAG] Message`

TAG 说明：
- `MAIN` - 程序入口
- `GUI` - GUI 模块
- `SER` - 串口模块

## 许可证

Copyright (c) 2026. All rights reserved.
