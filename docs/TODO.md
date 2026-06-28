# SerialEcho - 待办改进项

本文档记录已识别但尚未实现的功能增强和改进项。

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
