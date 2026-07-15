# SerialEcho

串口设备模拟器 —— Win32 串口应用的最小可运行示例。

## 功能概述

- 串口回环测试（接收数据原样返回）
- 实时 HEX 日志显示（带时间戳和颜色）
- Ping 测试（发送随机数据）
- 可扩展的协议处理框架
- 字体配置与 INI 持久化

## 系统要求

- Windows 10/11 (x64)
- CMake 3.20+ / MSVC 编译器

## 编译

```powershell
mkdir build && cd build
cmake .. -G "NMake Makefiles"
cmake --build .
```

选项：
- `-DENABLE_TRACE_FW=ON` 启用框架调试日志
- `-DENABLE_TRACE_PROTO=ON` 启用协议调试日志
- `-DCMAKE_BUILD_TYPE=Debug` 调试构建

## 使用

1. 运行 `build\SerialEcho.exe`
2. Serial > Connect 选择串口
3. 用串口工具连接另一端，发送数据即可看到回显

> 如需本地测试，可使用 [com0com](https://sourceforge.net/projects/com0com/) 创建虚拟串口对。

## 项目结构

```
SerialEcho/
├── src/                    # 源代码
│   ├── main.c / main.h     # 程序入口和 GUI 实现
│   ├── serial.c / serial.h # 串口通信模块
│   ├── example_echo_hal.c / example_echo_hal.h # Echo 示例 HAL（替换为你自己的）
│   ├── example_echo.c / example_echo.h         # Echo 协议示例（替换为你自己的）
│   ├── app_protocol.c / app_protocol.h # 信号/配置处理
│   ├── app_logview.c / app_logview.h # 日志显示
│   ├── resource.h          # 资源 ID
│   ├── resource.rc         # 资源文件（菜单、对话框、字符串）
│   └── utils/              # 辅助模块
│       ├── config.c / config.h   # 配置持久化
│       ├── lang.c / lang.h       # 国际化辅助
│       ├── timer.c / timer.h     # 定时器工具
│       └── trace.c / trace.h     # 调试日志
├── tests/                  # 单元测试
│   └── test_echo.c         # Echo 协议测试
├── res/                    # 资源文件（图标、位图、清单）
├── docs/                   # 文档
│   ├── REQUIREMENTS.md     # 需求规格
│   ├── DEVELOPMENT.md      # 二次开发指南
│   └── PROTOCOL.md         # 协议层开发规范
├── LICENSE                 # MIT 许可证
└── README.md
```

## 实际应用

- [FakeEsptool](https://github.com/larryli/FakeEsptool) — 基于 SerialEcho 框架构建的 ESP 芯片模拟器，是二次开发的完整参考。

## 文档

- [需求规格说明](docs/REQUIREMENTS.md)
- [二次开发指南](docs/DEVELOPMENT.md)
- [协议层开发规范](docs/PROTOCOL.md)

## 许可证

本项目采用 [MIT 许可证](LICENSE)。

Copyright (c) 2026 Larry Li
