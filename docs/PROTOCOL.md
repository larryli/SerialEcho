# SerialEcho 协议层开发规范

本文档说明如何基于 SerialEcho 框架开发自定义串口协议。

## 协议层架构

```
┌──────────────────────────────────────────────────────┐
│  GUI 层                                               │
│  main.c  app_logview.c  serial.c                     │
└──────────┬───────────────────────────┬───────────────┘
           │                           │
           ▼                           ▼
┌──────────────────────┐  ┌────────────────────────────┐
│  app_protocol.c/h    │  │  echo_hal.c/h              │
│  — 信号/配置处理     │  │  — 平台合同               │
└──────────────────────┘  └────────────┬─────────────┘
                                       │
                                       ▼
                              ┌────────────────────┐
                              │  protocol.c/h      │
                              │  — 协议逻辑        │
                              └────────────────────┘
```

**设计原则**：协议层通过 `echo_hal.h` 合同操作串口，不依赖 `serial.c`。

## 回调接口

### SERIAL_RX_CB（数据接收）

```c
typedef void (*SERIAL_RX_CB)(void *ctx, const BYTE *data, DWORD len, HWND hNotify);
```

| 参数 | 说明 |
|------|------|
| `ctx` | `SERIAL_CTX*`，串口上下文 |
| `data` | 接收到的数据 |
| `len` | 数据长度 |
| `hNotify` | 主窗口句柄，用于 PostMessage |

**注册方式**：
```c
Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_OnData);
```

### SERIAL_SIGNAL_CB（信号变化）

```c
typedef void (*SERIAL_SIGNAL_CB)(void *ctx, DWORD modemStatus, HWND hNotify);
```

| 参数 | 说明 |
|------|------|
| `ctx` | `SERIAL_CTX*`，串口上下文 |
| `modemStatus` | `GetCommModemStatus()` 结果 |
| `hNotify` | 主窗口句柄 |

**信号标志**：
- `MS_DSR_ON` — DSR 为 ON（主机 DTR 已断言）
- `MS_CTS_ON` — CTS 为 ON（主机 RTS 已断言）
- `MS_RING_ON` — 振铃指示
- `MS_RLSD_ON` — 载波检测

**注册方式**：
```c
Serial_SetSignalCallback(&g_serial, (SERIAL_SIGNAL_CB)MyProtocol_OnSignal);
```

## 开发自定义协议

### 步骤 1：创建协议文件

```c
// src/my_protocol.h
#ifndef MY_PROTOCOL_H
#define MY_PROTOCOL_H

#include <windows.h>

void MyProtocol_Init(void);
void MyProtocol_OnData(void *ctx, const BYTE *data, DWORD len, HWND hNotify);
void MyProtocol_OnSignal(void *ctx, DWORD modemStatus, HWND hNotify);

#endif
```

### 步骤 2：实现协议逻辑

```c
// src/my_protocol.c
#include "my_protocol.h"
#include "echo_hal.h"
#include "app_logview.h"
#include "utils/trace.h"

static const char *TAG = "MY";

void MyProtocol_Init(void)
{
    TRACE_PROTO(TAG, "Protocol initialized");
}

void MyProtocol_OnData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    if (!data || len == 0) return;

    TRACE_PROTO(TAG, "Received %lu bytes", len);

    // 示例：回环
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (buf) {
        CopyMemory(buf, data, len);
        echo_hal_write(buf, len);
        HeapFree(GetProcessHeap(), 0, buf);
    }

    // 记录日志
    Main_AppendLog(hNotify, data, len, DIR_RX);
}

void MyProtocol_OnSignal(void *ctx, DWORD modemStatus, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    BOOL dsr = (modemStatus & MS_DSR_ON) != 0;
    BOOL cts = (modemStatus & MS_CTS_ON) != 0;
    TRACE_PROTO(TAG, "Signal: DSR=%s CTS=%s", dsr ? "ON" : "OFF", cts ? "ON" : "OFF");
}
```

### 步骤 3：修改 main.c

1. 替换 `#include "protocol.h"` 为 `#include "my_protocol.h"`
2. 在连接时注册回调：
```c
Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_OnData);
Serial_SetSignalCallback(&g_serial, (SERIAL_SIGNAL_CB)MyProtocol_OnSignal);
```

### 步骤 4：更新 CMakeLists.txt

```cmake
add_executable(SerialEcho WIN32
    src/main.c
    src/serial.c
    src/my_protocol.c  # 替换 protocol.c
    src/app_logview.c
    ...
)
```

## 信号处理

### DTR/RTS 状态机

SerialEcho 已内置 DTR/RTS 信号监听（`EV_DSR | EV_CTS`），通过 `SERIAL_SIGNAL_CB` 回调通知协议层。

**典型用途**：
- 检测下载模式（ClassicReset 序列）
- 串口设备复位控制
- 流控制

### 信号控制函数

```c
BOOL Serial_SetDtr(SERIAL_CTX *ctx, BOOL state);  // 设置 DTR
BOOL Serial_SetRts(SERIAL_CTX *ctx, BOOL state);  // 设置 RTS
```

## echo_hal.h 平台合同

协议层通过 `echo_hal.h` 操作串口，不依赖 `serial.c`：

| 函数 | 说明 |
|------|------|
| `echo_hal_write(data, len)` | 发送数据 |
| `echo_hal_log(tag, fmt, ...)` | 日志投递 |

**实现**：`echo_hal.c` 将调用转发到 `serial.c` 的 `Serial_WriteData` 和 `Serial_PostLog`。

**优势**：协议层可独立编译测试，换平台只改 `echo_hal.c`。

## 日志接口

协议层可通过以下函数记录日志：

```c
// 数据日志（带颜色区分 RX/TX）
Main_AppendLog(hNotify, data, len, DIR_RX);
Main_AppendLog(hNotify, data, len, DIR_TX);

// 自定义文本日志
Main_AppendCustomLog(hNotify, L"TAG", L"message");

// 信号/配置日志（带颜色）
Main_AppendSignalLog(L"SIG", L"DSR:ON CTS:OFF", COLOR_SIGNAL);
```

## 调试

启用协议调试日志：

```powershell
cmake -DENABLE_TRACE_PROTO=ON -B build
```

使用 `TRACE_PROTO` 宏输出调试信息：
```c
TRACE_PROTO(TAG, "Processing command: 0x%02X", cmd);
```
