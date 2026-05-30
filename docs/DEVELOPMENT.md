# 二次开发指南

本文档介绍如何基于 SerialEcho 框架开发自定义串口设备模拟器。

## 项目架构

```
┌─────────────┐     ┌─────────────┐          ┌─────────────┐
│   main.c    │────▶│  serial.c   │──回调──▶│ protocol.c  │
│  (UI层)     │     │ (通信层)    │          │ (协议层)    │
└─────────────┘     └─────────────┘          └─────────────┘
```

- **main.c**: 用户界面，注册协议回调，处理菜单/工具栏命令，显示日志
- **serial.c**: 串口通信，管理端口开关、数据收发，通过回调通知协议层
- **protocol.c**: 协议处理，实现回调函数，决定如何响应接收的数据

## 协议回调机制

serial.c 通过函数指针回调通知协议层接收数据，解耦通信层和协议层。

### 回调类型

```c
typedef void (*SERIAL_RX_CB)(void *ctx, const BYTE *data, DWORD len, HWND hNotify);
```

### 注册回调

```c
// 在 gui.c GUI_OnConnect 中注册
Serial_SetReceiveCallback(&g_serial, Protocol_ProcessData);
```

## 协议处理扩展

### 当前实现：ECHO 回环

`protocol.c` 中的 `Protocol_ProcessData()` 实现了简单的回环测试：

```c
void Protocol_ProcessData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    SERIAL_CTX *serial = (SERIAL_CTX *)ctx;
    if (!serial || !data || len == 0) return;

    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    if (!buf) return;

    CopyMemory(buf, data, len);
    Serial_WriteData(serial, buf, len, hNotify);
    HeapFree(GetProcessHeap(), 0, buf);
}
```

### 自定义协议步骤

1. 创建 `src/my_protocol.c` 和 `src/my_protocol.h`
2. 实现回调函数（匹配 `SERIAL_RX_CB` 签名）
3. 在 `gui.c` 中注册你的回调
4. 在 `CMakeLists.txt` 中添加源文件

### 自定义协议示例

```c
// my_protocol.c
#include "serial.h"

static int g_temperature = 25;

void MyProtocol_ProcessData(void *ctx, const BYTE *data, DWORD len, HWND hNotify)
{
    SERIAL_CTX *serial = (SERIAL_CTX *)ctx;
    if (!serial || !data || len == 0) return;

    switch (data[0]) {
    case 0x01:  // Query temperature
    {
        BYTE *resp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 2);
        if (resp) {
            resp[0] = 0x01;
            resp[1] = (BYTE)g_temperature;
            Serial_WriteData(serial, resp, 2, hNotify);
            HeapFree(GetProcessHeap(), 0, resp);
        }
        break;
    }
    case 0x02:  // Set temperature
        if (len >= 2) g_temperature = data[1];
        break;
    }
}
```

### 注册自定义协议

```c
// gui.c - GUI_OnConnect()
Serial_SetReceiveCallback(&g_serial, MyProtocol_ProcessData);
```

## 主动发送数据

### 使用 Serial_WriteData()

```c
BYTE data[] = {0xAA, 0x55, 0x01, 0x02};
Serial_WriteData(&g_serial, data, sizeof(data), hWnd);
```

### 定时发送（心跳包）

```c
#define IDT_HEARTBEAT 1001

// WM_CREATE
SetTimer(hWnd, IDT_HEARTBEAT, 5000, NULL);

// WM_TIMER
if (wParam == IDT_HEARTBEAT && Serial_IsOpen(&g_serial)) {
    BYTE hb[] = {0xAA, 0x55};
    Serial_WriteData(&g_serial, hb, sizeof(hb), hWnd);
}

// WM_DESTROY
KillTimer(hWnd, IDT_HEARTBEAT);
```

## 添加新功能模块

1. 创建 `src/mymodule.c` 和 `src/mymodule.h`
2. 在 `CMakeLists.txt` 中添加源文件
3. 在 `gui.c` 中集成你的模块

## 消息定义

| 消息 | 用途 | wParam | lParam |
|------|------|--------|--------|
| WM_SERIAL_RX | RX 数据到达 | 数据长度 | 数据指针 (HeapAlloc) |
| WM_SERIAL_TX | TX 数据已发送 | 数据长度 | 数据指针 (HeapAlloc) |
| WM_SERIAL_ERROR | 连接错误 | 错误代码 | 0 |
| WM_SERIAL_LOG | 自定义日志 | WCHAR* tag | WCHAR* text |
| WM_SERIAL_SIGNAL | 信号变化 | modemStatus | 0 |
| WM_SERIAL_CONFIG | 配置变更 | 保留 | 0 |

接收 WM_SERIAL_RX/TX 的数据指针需要 HeapFree 释放。
接收 WM_SERIAL_LOG 的 tag/text 指针需要 HeapFree 释放。

## 内存管理规则

1. **回调函数**: 使用 `HeapAlloc` 分配响应缓冲区，发送后 `HeapFree`
2. **Serial_WriteData**: 内部复制数据用于 TX 通知，不影响传入的缓冲区
3. **PostMessage**: 接收端负责释放 lParam 指向的内存

## 定时器工具

协议层可使用 `timer.h` 提供的定时器接口实现超时处理：

```c
#include "utils/timer.h"

// 创建定时器
TIMER_CTX *timer = Timer_Create();

// 启动一次性定时器（5秒超时）
Timer_Start(timer, 5000, MyTimeoutCallback, userData);

// 取消定时器
Timer_Cancel(timer);

// 销毁定时器
Timer_Destroy(timer);
```

### 定时器回调

```c
void MyTimeoutCallback(void *userData)
{
    // 注意：回调在工作线程中执行，不是 UI 线程
    // 如需操作 UI，请使用 PostMessage
    SERIAL_CTX *ctx = (SERIAL_CTX *)userData;
    Serial_PostLog(ctx->hNotify, L"TIMEOUT", L"Response timeout");
}
```

## 编译调试

```powershell
# Debug build with trace logging
cmake .. -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TRACE=ON
cmake --build .

# Check trace.log for debug output
```

## API 参考

### serial.h

| 函数 | 说明 |
|------|------|
| `Serial_EnumPorts()` | 枚举可用串口 |
| `Serial_Open()` | 打开串口 |
| `Serial_Close()` | 关闭串口 |
| `Serial_IsOpen()` | 检查连接状态 |
| `Serial_WriteData()` | 写入数据 |
| `Serial_SetReceiveCallback()` | 设置接收回调 |
| `Serial_SetSignalCallback()` | 设置信号变化回调 |
| `Serial_SetDtr()` | 设置/清除 DTR |
| `Serial_SetRts()` | 设置/清除 RTS |
| `Serial_SetBaudRate()` | 修改波特率 |
| `Serial_GetRxBytes()` | 获取接收字节数 |
| `Serial_GetTxBytes()` | 获取发送字节数 |
| `Serial_PostLog()` | 发送日志到 UI |

### timer.h

| 函数 | 说明 |
|------|------|
| `Timer_Create()` | 创建定时器上下文 |
| `Timer_Destroy()` | 销毁定时器 |
| `Timer_Start()` | 启动一次性定时器 |
| `Timer_Cancel()` | 取消运行中的定时器 |
| `Timer_IsRunning()` | 检查定时器是否运行中 |

### protocol.h

| 函数 | 说明 |
|------|------|
| `Protocol_Init()` | 初始化协议模块 |
| `Protocol_ProcessData()` | ECHO 回调实现 |
| `Protocol_SendPing()` | 发送随机测试数据 |
