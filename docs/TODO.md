# SerialEcho - 待办改进项

本文档记录已识别但尚未实现的功能增强和改进项。

---

## 中优先级 - 单元测试

建立 `tests/` 目录，为核心模块添加单元测试。

**测试优先级与模块：**

| 优先级 | 模块 | 测试要点 |
|--------|------|----------|
| 1 | `protocol.c` | 回环逻辑、数据处理、边界条件 |
| 2 | `serial.c` | 端口枚举、打开/关闭、配置修改 |
| 3 | `app_logview.c` | 日志缓冲、刷新逻辑、颜色显示 |

**测试框架：** 复用现有 `tests/` 目录结构，CMake + ctest。

---

## 中优先级 - 协议层分离

将 `protocol.c` 中的 GUI 相关代码提取到 `app_protocol.c`，参考 FakeEsptool 的 `app_protocol.c` 设计。

**提取内容：**

| 函数 | 说明 |
|------|------|
| `ResetSignalState` | 信号状态重置 |
| `OutputBootMessage` | 启动消息输出 |
| `OnEsptoolSignal` | DTR/RTS 信号状态机 |

**收益：**
- 协议逻辑与 GUI 解耦
- 方便移植到其他项目
- 与 FakeEsptool 架构一致

---

## 低优先级 - echo_hal.h

定义协议层平台合同，使协议层可独立测试和移植。

**合同接口：**

```c
// echo_hal.h
void echo_hal_write(const BYTE *data, DWORD len);
void echo_hal_set_baud_rate(DWORD baudRate);
void echo_hal_log(const char *tag, const char *msg);
```

**说明：** 当前 SerialEcho 的串口操作通过 `serial.c` 直接调用，此改进为可选项。

---
