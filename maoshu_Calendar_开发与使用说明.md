# maoshu_Calendar 开发与使用说明

> 横屏 320×240 日历设备：月历 + 农历/黄历（宜忌）+ 大时钟 + 7 天天气 + 巴法云备忘录推送
> 硬件平台：ESP32-C3 + 2.4 寸 ST7789 IPS TFT

---

## 一、硬件清单

| 物料 | 规格/说明 |
|---|---|
| 主控板 | 安信可 ESP32-C3 Core 板（USB Type-C，USB-Serial-JTAG，4MB Flash） |
| 屏幕 | 2.4 寸 ST7789 240×320 IPS，**10PIN SPI 串口屏**（FPC 插接款） |
| FPC 转接板 | 10PIN FPC → 2×5 排针（随屏购买，板上有 0R0 电阻与跳线为出厂配置，勿动） |
| 按键 | 轻触开关 1 个（复用板载 BOOT 键亦可，本机外接至 GPIO8） |
| 连接线 | 杜邦线若干 |
| 供电 | 任意 5V USB 电源头（正常工作时约 100mA） |

**可用 GPIO**：0–10、18–21（GPIO11–17 为内置 Flash 占用，禁用；GPIO2/8/9 为 Strapping 引脚，烧录时注意）。

---

## 二、接线表（已实测验证）

### 屏幕 10PIN → ESP32-C3

| 转接板孔位 | 屏幕功能 | 线色（本机） | ESP32-C3 引脚 |
|---|---|---|---|
| 1 | GND | 白 | GND |
| 2 | RESET | 蓝 | GPIO5 |
| 3 | SCL（SPI 时钟） | 黄 | GPIO2 |
| 4 | D/C（命令/数据） | 紫 | GPIO4 |
| 5 | CS（片选） | 绿 | GPIO7 |
| 6 | SDA（SPI 数据） | 橙 | GPIO3 |
| 8 | VCC | 红 | 3.3V |
| 7 | BLK（背光） | — | 3.3V（常亮） |

- **背光**：LED+ → 3.3V，LED− → GND（转接板自带限流电阻；接反会导致 USB 报"设备无法识别"）
- **MISO 不接**（屏为只写，`TFT_MISO = -1`）
- **SPI4W 引脚 → GND**（固定 4 线 SPI 模式，转接板出厂已配置）
- ⚠️ 接线核心教训：**不要按屏幕丝印 SDA/SCL 字面理解**，以本表实测验证的孔位为准；杜邦线 SPI 频率保守用 27MHz，过高会白屏/花屏

### 按键

| 按键引脚 | ESP32-C3 |
|---|---|
| 一脚 | GPIO8（代码内部上拉） |
| 另一脚 | GND |

---

## 三、开发环境与烧录设置

### 1. Arduino 环境

- Arduino IDE 2.x + esp32 核心包 **2.0.17-cn**（含 C3 支持）
- 开发板选择：**ESP32C3 Dev Module**

### 2. 关键烧录参数（全部必选，实测结论）

| 设置项 | 值 | 原因 |
|---|---|---|
| PartitionScheme | **Huge APP (3MB No OTA)** | 固件约 990KB，默认分区放不下 |
| Flash Frequency | **40MHz** | ⚠️ 此模组 80MHz 会无限重启（rst:0x3 循环），40MHz 实测稳定 |
| Flash Mode | **DIO** | 同上，QIO 不稳定 |
| USB CDC On Boot | **Enabled** | 串口日志走 USB，无需外接串口工具 |
| Upload Speed | 460800 | 稳定速度 |

**arduino-cli 一键编译命令**：

```powershell
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile `
  --fqbn "esp32:esp32:esp32c3:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,CDCOnBoot=cdc" `
  --build-path "$env:TEMP\maoshu_build" `
  "c:\Users\Administrator\Desktop\my-demo2\DuduClock_2.0\maoshu_Calendar"
```

### 3. 烧录步骤

1. USB 连接电脑，确认出现 COM 口
2. **进下载模式**：按住 BOOT 不放 → 短按一下 RST → 松开 BOOT（不进下载模式会报 `Wrong boot mode detected (0xc)`）
3. IDE 点上传，或用 arduino-cli upload（同 FQBN）
4. 烧完按一下 **RST** 复位运行

### 4. 串口日志

波特率 115200；用 PowerShell SerialPort 抓取时需 `DtrEnable=true, RtsEnable=false`。

---

## 四、固件依赖库清单

| 库 | 用途 | 备注 |
|---|---|---|
| **TFT_eSPI** | 屏幕驱动 | ① 必须用本项目的 `User_Setup.h` 覆盖库目录同名文件（关键参数见下）；② 必须打 **C3 补丁**（见第五节） |
| **PubSubClient** | MQTT 连接巴法云 | 缓冲区设 512 |
| **OneButton** | 按键单击/双击/长按识别 | |
| WiFi / WebServer / HTTPClient / Preferences | ESP32 核心自带 | 配网、天气 HTTP、NVS 存储 |

TFT_eSPI `User_Setup.h` 关键配置（已含在本仓库备注）：

```cpp
#define ST7789_2_DRIVER        // 该屏需用 2 号初始化序列
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#define TFT_RGB_ORDER 1        // 必须写数值1；写 TFT_BGR 宏会红蓝互换
#define TFT_INVERSION_OFF      // 此屏玻璃不反色，需发 INVOFF 抵消库的强制 INVON
#define TFT_ROTATION 1         // 横屏；装反改 3
#define TFT_MOSI   3
#define TFT_SCLK   2
#define TFT_CS     7
#define TFT_DC     4
#define TFT_RST    5
#define TFT_MISO  -1
#define SPI_FREQUENCY 27000000 // 杜邦线 27MHz 安全
```

---

## 五、必打补丁：TFT_eSPI 的 ESP32-C3 崩溃修复

**现象**：不改此补丁，固件一启动即 `Guru Meditation Error: Store access fault`（MTVAL=0x10）。

**根因**：`Processors/TFT_eSPI_ESP32_C3.h` 中用 `#ifndef REG_SPI_BASE` 打的 IDF 修复块是死代码（IDF 头文件早已定义过该宏），导致 C3 上 SPI 寄存器基址求值为 0，全局寄存器指针 `_spi_user` 等初始化成非法地址。

**修法**：在库文件 `Processors/TFT_eSPI_ESP32_C3.h` 中，把原 `#ifndef REG_SPI_BASE ... #endif` 块改为无条件定义：

```cpp
#undef  REG_SPI_BASE
#define REG_SPI_BASE(i) DR_REG_SPI2_BASE   // C3 上无论 SPI_HOST 是几都用 SPI2 基址
```

---

## 六、软件结构与配置

```
maoshu_Calendar/
├── maoshu_Calendar.ino   主流程：初始化、按键、主循环、日/夜主题切换
├── common.h           ★ 全部用户配置（按键脚/NTP/巴法云/天气坐标/热点名）
├── cal.cpp/.h         日历页：月历、农历、干支黄历(建除十二神宜忌)、大时钟
├── weather.cpp/.h     天气：Open-Meteo 免费API(无需密钥)，30分钟刷新
├── memo.cpp/.h        备忘录：巴法云MQTT推送、NVS持久化(断电保留10条)
├── net.cpp/.h         WiFi连接 + AP配网(192.168.4.1) + NTP校时
├── tftUtil.cpp/.h     屏幕基座 + 日/夜主题引擎(7:00-19:00自动切换)
└── font/memo_font_16.h  16×16中文点阵字库(1131字，Unicode排序二分查找)
```

**常用配置都在 `common.h`**：

- `WEATHER_LAT / WEATHER_LON`：天气坐标（当前为云南大理 25.61, 100.27）
- `BEMFA_UID / BEMFA_TOPIC`：巴法云私钥与主题（当前 `yourTopic`）
- `AP_SSID / AP_PASS`：配网热点名（当前 maoshu_Calendar / 12345678）
- `BUTTON`：按键引脚（当前 GPIO8）

**扩充字库**：屏上出现 □ 方框即缺字。用 Python + PIL 读取 `C:\Windows\Fonts\simsun.ttc`，将新字渲染成 16×16 点阵（行优先、每行 2 字节、高位在前），插入 `font/memo_font_16.h` 并保持 `memoFontCp` 数组按 Unicode 升序排列，同时更新 `memo.h` 里的 `MEMO_CJK_COUNT`。

---

## 七、使用说明

### 首次使用（配网）

1. 上电后屏幕显示配网页，手机连接热点 **maoshu_Calendar**（密码 **12345678**）
2. 浏览器打开 **192.168.4.1**，页面自动搜索周边 WiFi（按信号强度排序，显示 强/中/弱 + dBm），点击名称自动填入，只需输入密码，点"保存并重启"
3. 重启后自动连 WiFi + 阿里云 NTP 校时，进入日历主界面

> 已配网设备开机直连；WiFi 断线每 30 秒自动重连。

### 更换 WiFi（手动配网页面）

设备联网后配置服务常驻，**日历页顶栏会显示设备 IP**（如 192.168.1.23）：

1. 手机/电脑浏览器访问该 IP（需与设备同一局域网）
2. 页面顶部显示"当前网络：XXX (IP)"，下方自动列出周边 WiFi
3. 点击选择新网络（或手动输入）→ 输密码 → "保存并重启"即完成换网

> 换新网络后若连接失败（如密码错误），设备会自动回到配网热点模式，重新配网即可。彻底清空 WiFi 凭证可用 esptool 擦除 NVS 分区：`esptool --chip esp32c3 --port COMxx erase_region 0x9000 0x6000`。

### 界面布局（横屏）

- **顶栏**：完整日期（橙）+ 星期 + 设备 IP（联网时显示，即配置页地址）+ 右上角 WiFi 状态点（绿=在线）
- **左侧**：月历（周日橙/周六青/今日红块高亮）+ 底部 7 天天气条（今 + 周一~周六）
- **右侧**：大时钟（每分钟刷新）+ 干支黄历（年干支/农历日，绿）+ 宜（绿）+ 忌（红）

### 按键操作

| 操作 | 功能 |
|---|---|
| 单击 | 日历页 ↔ 备忘录页 切换（备忘录页内单击=翻页） |
| 双击 | 回到日历页 |
| 长按 3 秒 | 清空全部备忘录 |

### 备忘录推送（巴法云）详细步骤

**第 1 步：注册账号**
1. 手机微信搜索小程序"**巴法云**"（或电脑浏览器打开 [bemfa.com](https://bemfa.com)）
2. 用手机号注册并登录（小程序和网页端账号通用）

**第 2 步：获取私钥 UID**
1. 登录后进入**控制台**首页
2. 页面上"**私钥**"一栏就是你的 UID（32 位十六进制字符串）
3. 把这个 UID 填入固件 `common.h` 的 `BEMFA_UID`（设备就是用它作为 MQTT 的 clientId 登录，无需用户名密码）

**第 3 步：创建/确认主题**
1. 在控制台点击"**添加主题**"（或直接在发送框输入主题名）
2. 主题名设为 **yourTopic**（须与固件 `common.h` 的 `BEMFA_TOPIC` 完全一致，区分大小写）
3. 主题类型选 **TCP设备云**（默认即可）

**第 4 步：重新编译烧录**
- 首次配置或更换过 UID/主题后，改完 `common.h` 需重新编译烧录一次（按第三节步骤）

**第 5 步：推送消息**
- **小程序**：巴法云控制台 → 选择主题 yourTopic → 输入文字 → 发送
- **网页端**：bemfa.com 控制台 → 主题右侧输入框 → 发送
- **其他方式**（可选）：巴法云还支持微信服务号、QQ 机器人、HTTP API 推送，见其官方文档
- 设备收到消息后**自动跳到备忘录页**显示；最多保留 10 条，超出自动丢弃最旧一条；**断电不丢**（存于 NVS）

**注意事项**：
- 巴法云会在主题里**保留最后一条消息**，设备重启重连后会收到重推——固件已做去重过滤（相同内容不重复入库），无需担心重启后重复
- 想让设备忽略某条已显示的消息，清空备忘录（长按 3 秒）即可，云端保留消息也不会回灌
- 若设备始终收不到：检查 UID 是否填对、主题名是否一致、屏幕右上角是否绿点（在线）

### 日/夜主题

每天 **7:00–19:00** 自动切换为日间浅色主题，其余时间为夜间深色主题，无需手动操作。主题色集中定义在 `tftUtil.cpp` 的 `DAY` / `NIGHT` 结构体中，可自行调整。

### 指示与异常

| 现象 | 含义/处理 |
|---|---|
| 右上角红点 | WiFi 离线，检查路由器；会自动重连 |
| 天气条显示"天气获取中" | 开机后最多半分钟获取；一直不出现则检查外网 |
| 时间的农历黄历区空白 | 尚未 NTP 校时成功（需外网） |
| 白屏 | 先查接线表和 27MHz 频率；再查 User_Setup.h 是否为仓库版本 |
| 启动即崩溃重启 | TFT_eSPI 未打第五节 C3 补丁 |

---

*文档生成：2026-11-04，对应固件版本：横屏日/夜主题版（989KB / Huge APP 31%）*
