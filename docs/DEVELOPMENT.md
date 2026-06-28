# 二次开发指南

本文档介绍如何基于 SerialEcho 框架开发自定义串口设备模拟器。

## 架构概览

```
┌─────────────────┐     ┌─────────────────┐
│     main.c      │────▶│   serial.c      │
│    (GUI层)      │     │  (通信层)       │
└────────┬────────┘     └────────┬────────┘
         │                       │
         ▼                       ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│ app_protocol.c  │  │  echo_hal.c     │  │ app_logview.c   │
│ (信号/配置处理) │  │ (平台合同)     │  │ (日志显示)      │
└─────────────────┘  └────────┬────────┘  └─────────────────┘
                              │
                              ▼
                     ┌─────────────────┐
                     │   protocol.c    │
                     │  (协议逻辑)     │
                     └─────────────────┘
```

| 模块 | 职责 |
|------|------|
| `main.c` | 用户界面，消息分发 |
| `serial.c/h` | 串口通信，数据收发，信号控制 |
| `echo_hal.c/h` | 协议层平台合同（send, log） |
| `protocol.c/h` | 协议处理逻辑（回环） |
| `app_protocol.c/h` | 信号/配置变化处理 |
| `app_logview.c/h` | 带缓冲的彩色日志显示 |
| `utils/*` | 配置、国际化、调试日志 |

## 快速开始

### 1. 创建协议文件

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

```c
// src/my_protocol.c
#include "my_protocol.h"
#include "echo_hal.h"
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

    echo_hal_log(L"MY", L"Received %lu bytes", len);

    // ECHO: send received data back
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (buf) {
        CopyMemory(buf, data, len);
        echo_hal_write(buf, len);
        HeapFree(GetProcessHeap(), 0, buf);
    }
}

void MyProtocol_OnSignal(void *ctx, DWORD modemStatus, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    BOOL dsr = (modemStatus & MS_DSR_ON) != 0;
    BOOL cts = (modemStatus & MS_CTS_ON) != 0;
    TRACE_PROTO(TAG, "Signal: DSR=%d CTS=%d", dsr, cts);
}
```

### 2. 注册回调

```c
// main.c - Main_OnConnect()
#include "my_protocol.h"

Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_OnData);
Serial_SetSignalCallback(&g_serial, (SERIAL_SIGNAL_CB)MyProtocol_OnSignal);
```

### 3. 更新构建

```cmake
# CMakeLists.txt
add_executable(SerialEcho WIN32
    src/main.c
    src/serial.c
    src/my_protocol.c  # 添加
    ...
)
```

## 回调机制

### 数据接收回调

```c
// 类型定义
typedef void (*SERIAL_RX_CB)(void *ctx, const BYTE *data, DWORD len, HWND hNotify);

// 注册（main.c 中）
Serial_SetReceiveCallback(&g_serial, (SERIAL_RX_CB)MyProtocol_OnData);

// 实现（协议层）
void MyProtocol_OnData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    // ctx: 串口上下文（通过 echo_hal 操作）
    // data: 接收数据
    // len: 数据长度
    // hNotify: UI 窗口句柄
}
```

### 信号变化回调

```c
// 类型定义
typedef void (*SERIAL_SIGNAL_CB)(void *ctx, DWORD modemStatus, HWND hNotify);

// 注册
Serial_SetSignalCallback(&g_serial, (SERIAL_SIGNAL_CB)MyProtocol_OnSignal);

// 实现
void MyProtocol_OnSignal(void *ctx, DWORD modemStatus, HWND hNotify)
{
    BOOL dsr = (modemStatus & MS_DSR_ON) != 0;  // 主机 DTR 状态
    BOOL cts = (modemStatus & MS_CTS_ON) != 0;  // 主机 RTS 状态
}
```

**注意：** 协议层使用 `void *ctx` 和 `echo_hal.h` 操作串口，不直接依赖 `serial.h`。

## 数据发送

### 通过 echo_hal 发送（推荐）

```c
#include "echo_hal.h"

BYTE data[] = {0xAA, 0x55, 0x01, 0x02};
echo_hal_write(data, sizeof(data));
```

### 定时发送

```c
#include "utils/timer.h"
#include "echo_hal.h"

static TIMER_CTX *g_timer = NULL;

void StartHeartbeat(void)
{
    g_timer = Timer_Create();
    Timer_Start(g_timer, 5000, OnHeartbeat, NULL);
}

void OnHeartbeat(void *userData)
{
    (void)userData;
    BYTE hb[] = {0xAA, 0x55};
    echo_hal_write(hb, sizeof(hb));
}

void StopHeartbeat(void)
{
    Timer_Destroy(g_timer);
    g_timer = NULL;
}
```

## 信号控制

```c
// 读取当前信号状态
DWORD baudRate;
BYTE dataBits, parity, stopBits;
Serial_GetConfig(ctx, &baudRate, &dataBits, &parity, &stopBits);

// 控制输出信号
Serial_SetDtr(ctx, TRUE);   // DTR 置高
Serial_SetRts(ctx, FALSE);  // RTS 置低

// 修改串口参数
Serial_SetBaudRate(ctx, CBR_921600);
```

## 配置持久化

使用 `config.h` 接口保存/加载配置，存储在 `SerialEcho.ini`：

```c
#include "utils/config.h"

// 保存
Config_SetString(L"MyProtocol", L"Mode", L"Normal");
Config_SetInt(L"MyProtocol", L"Timeout", 5000);
Config_SetBool(L"MyProtocol", L"AutoReply", TRUE);

// 加载（第三个参数为默认值）
WCHAR mode[32];
Config_GetString(L"MyProtocol", L"Mode", mode, 32, L"Normal");
int timeout = Config_GetInt(L"MyProtocol", L"Timeout", 3000);
BOOL autoReply = Config_GetBool(L"MyProtocol", L"AutoReply", FALSE);
```

## 调试日志

### 启用日志

```powershell
cmake .. -DENABLE_TRACE_PROTO=ON   # 协议日志
cmake .. -DENABLE_TRACE_FW=ON      # 框架日志
```

### 使用日志宏

```c
#include "utils/trace.h"

static const char *TAG = "MY";

TRACE_PROTO(TAG, "Received %lu bytes", len);
TRACE_PROTO(TAG, "Command: 0x%02X", data[0]);
```

### 自定义日志输出到主窗口

```c
// 基础版本
Serial_PostLog(hNotify, L"MODBUS", L"Frame received");

// 格式化版本（更方便）
Serial_PostLogF(hNotify, L"MODBUS", L"Received %lu bytes, CRC: 0x%04X", len, crc);
Serial_PostLogF(hNotify, L"MODBUS", L"Register %d = %d", addr, value);
```

日志以橙色显示，格式：`HH:MM:SS.mmm [TAG] Message`

## 完整示例

```c
// my_protocol.c - 自定义协议示例
#include "my_protocol.h"
#include "echo_hal.h"
#include "utils/trace.h"
#include "utils/config.h"

static const char *TAG = "MY";

static int g_mode = 0;

void MyProtocol_Init(void)
{
    g_mode = Config_GetInt(L"MyProtocol", L"Mode", 0);
    TRACE_PROTO(TAG, "Protocol initialized, mode=%d", g_mode);
}

void MyProtocol_OnData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    if (!data || len == 0) return;

    TRACE_PROTO(TAG, "Received %lu bytes", len);

    switch (data[0]) {
    case 0x01:  // 查询命令
    {
        BYTE resp[4] = {0x01, 0x00, (BYTE)g_mode, 0x00};
        echo_hal_write(resp, sizeof(resp));
        break;
    }
    case 0x02:  // 设置命令
        if (len >= 2) {
            g_mode = data[1];
            Config_SetInt(L"MyProtocol", L"Mode", g_mode);
            BYTE ack[] = {0x02, 0x01};
            echo_hal_write(ack, sizeof(ack));
        }
        break;
    default:    // ECHO
        echo_hal_write(data, len);
        break;
    }
}

void MyProtocol_OnSignal(void *ctx, DWORD modemStatus, HWND hNotify)
{
    (void)ctx; (void)hNotify;
    BOOL dsr = (modemStatus & MS_DSR_ON) != 0;
    BOOL cts = (modemStatus & MS_CTS_ON) != 0;
    TRACE_PROTO(TAG, "Signal: DSR=%d CTS=%d", dsr, cts);
}
```

## API 参考

### serial.h - 串口通信

| 函数 | 说明 |
|------|------|
| `Serial_Open(ctx, portName, hNotify)` | 打开串口 |
| `Serial_Close(ctx)` | 关闭串口 |
| `Serial_IsOpen(ctx)` | 检查连接状态 |
| `Serial_WriteData(ctx, data, len, hNotify)` | 写入数据 |
| `Serial_SetReceiveCallback(ctx, cb)` | 设置数据接收回调 |
| `Serial_SetSignalCallback(ctx, cb)` | 设置信号变化回调 |
| `Serial_SetDtr(ctx, state)` | 设置 DTR |
| `Serial_SetRts(ctx, state)` | 设置 RTS |
| `Serial_SetBaudRate(ctx, baudRate)` | 修改波特率 |
| `Serial_SetDataBits(ctx, bits)` | 修改数据位 |
| `Serial_SetParity(ctx, parity)` | 修改校验 |
| `Serial_SetStopBits(ctx, bits)` | 修改停止位 |
| `Serial_GetConfig(ctx, ...)` | 读取当前配置 |
| `Serial_PostLog(hNotify, tag, text)` | 发送自定义日志 |
| `Serial_PostLogF(hNotify, tag, fmt, ...)` | 发送格式化日志 |

### config.h - 配置持久化

| 函数 | 说明 |
|------|------|
| `Config_GetString/SetString` | 字符串读写 |
| `Config_GetInt/SetInt` | 整数读写 |
| `Config_GetBool/SetBool` | 布尔读写 |
| `Config_GetFont/SetFont` | 字体设置 |
| `Config_GetLastPort/SetLastPort` | 最后连接端口 |

### timer.h - 定时器

| 函数 | 说明 |
|------|------|
| `Timer_Create()` | 创建定时器 |
| `Timer_Destroy(ctx)` | 销毁定时器 |
| `Timer_Start(ctx, ms, cb, data)` | 启动一次性定时器 |
| `Timer_Cancel(ctx)` | 取消定时器 |

### trace.h - 调试日志

| 宏 | 说明 |
|-----|------|
| `TRACE_FW(tag, ...)` | 框架日志 |
| `TRACE_PROTO(tag, ...)` | 协议日志 |
