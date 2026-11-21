# maoshu_Calendar

基于 **ESP32-C3 + 2.4 寸 ST7789 屏** 的横屏桌面时钟（320×240）。一块小板子当桌上摆件：看时间、农历黄历、天气，也能收作业提醒、做番茄钟、定闹钟。

> 本项目为个人 DIY 项目，硬件为安信可 Core-ESP32（ESP32-C3）开发板 + 2.4 寸 SPI 串口屏，焊接与接线说明见下。

---

## 功能

| 功能 | 说明 |
| --- | --- |
| 桌面 | 插画壁纸做背景，右侧深色信息面板：星期、大时钟、农历、今日天气与最高温、黄历宜忌（宜绿忌红） |
| 备忘录 | 通过巴法云 MQTT 推送文字到设备，断电保留最近 10 条，自动翻页 |
| 天气 | Open-Meteo 免费接口（无需密钥），7 天预报页；失败自动退避重试 |
| 番茄钟 | 专注/短休/长休自动循环，时长可在网页配置；阶段结束有蜂鸣提示音 |
| 闹钟 | 最多 4 组，每天或仅一次，到点间歇响 60 秒，任意按键停止 |
| 壁纸 | 内置 5 套 320×240 插画壁纸，壁纸页可启用/停用与切换 |
| 配置页 | 设备内置 Web 配置：换 WiFi、天气地区、番茄钟时长、巴法云推送、闹钟 |

## 硬件清单

| 物料 | 规格 |
| --- | --- |
| 主控 | 安信可 Core-ESP32（ESP32-C3，USB Type-C，4MB Flash） |
| 屏幕 | 2.4 寸 ST7789 240×320 IPS，10PIN SPI 串口屏 + FPC 转接板 |
| 按键 | 轻触开关 1 个（接 GPIO8，代码内部上拉） |
| 蜂鸣器 | 有源蜂鸣器 1 个（正极 → GPIO6，负极 → GND） |
| 其他 | 杜邦线若干、5V USB 供电 |

## 接线（已实测）

屏幕 10PIN 转接板 → ESP32-C3：

| 孔位 | 屏幕功能 | ESP32-C3 |
| --- | --- | --- |
| 1 | GND | GND |
| 2 | RESET | GPIO5 |
| 3 | SCL | GPIO2 |
| 4 | D/C | GPIO4 |
| 5 | CS | GPIO7 |
| 6 | SDA | GPIO3 |
| 7 | BLK（背光） | 3.3V（常亮） |
| 8 | VCC | 3.3V |

- MISO 不接（屏只写）；SPI4W 引脚接 GND
- 按键：一脚 GPIO8，另一脚 GND
- 蜂鸣器：正极 GPIO6，负极 GND
- 注意：**不要按屏幕丝印字面理解 SDA/SCL**，以上表为准；杜邦线 SPI 频率用 27MHz，过高会花屏

## 编译与烧录

Arduino IDE 2.x + esp32 核心包（2.0.17-cn），开发板选 **ESP32C3 Dev Module**。

必选参数：

| 设置 | 值 |
| --- | --- |
| PartitionScheme | Huge APP (3MB No OTA) |
| Flash Frequency | 40MHz |
| Flash Mode | DIO |
| USB CDC On Boot | Enabled |

arduino-cli 一键编译：

```powershell
arduino-cli compile `
  --fqbn "esp32:esp32:esp32c3:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,CDCOnBoot=cdc" `
  --build-path "$env:TEMP\maoshu_build" `
  "./maoshu_Calendar"
```

烧录前需让板子进下载模式（按住 BOOT → 短按 RST → 松开 BOOT），否则报 `Wrong boot mode detected (0xc)`。

依赖库：**TFT_eSPI**（需用本项目的 `User_Setup.h` 覆盖，并打 ESP32-C3 补丁，详见《maoshu_Calendar_开发与使用说明.md》第五节）、**PubSubClient**、**OneButton**。

## 首次使用

1. 上电进入配网模式：手机连热点 `maoshu_Calendar` / `12345678`
2. 浏览器打开 `192.168.4.1`，选择 WiFi 并输入密码，保存重启
3. 联网后桌面会显示设备 IP，用同一局域网浏览器访问该 IP 进入配置页

配置页分五块：网络、天气地区、番茄钟、巴法云推送、闹钟。除网络外都即时生效并存入 NVS。

## 按键手势

| 操作 | 桌面 | 备忘录 | 番茄钟 | 天气 | 壁纸 |
| --- | --- | --- | --- | --- | --- |
| 单击 | 进备忘录 | 下一页（末页进番茄钟） | 开始/暂停 | 返回桌面 | 换下一套 |
| 双击 | 进天气 | 回桌面 | 重置 | — | 启用/停用 |
| 三击 | 进壁纸 | — | — | — | — |
| 长按 3 秒 | — | 清空备忘录 | 回桌面 | 返回桌面 | 回桌面 |

连击判定窗口 300ms；闹钟响铃期间任意按键立即静音。

## 壁纸替换

`maoshu_Calendar/wallpapers/` 放原图，运行电脑端工具重新生成并编译：

```bash
python tools/wallpaper_convert.py --day maoshu_Calendar/wallpapers/1.png --out maoshu_Calendar/wallpaper_data.h
```

每套壁纸 153,600 B（320×240 RGB565，仅白天版）。

## 巴法云推送（可选）

在 [bemfa.com](https://bemfa.com) 注册后取得私钥 UID 与主题，填入配置页即可，无需重新编译。源码 `common.h` 中为占位符 `youUID` / `yourTopic`，**请勿提交真实私钥**。

## 目录结构

```
maoshu_Calendar/          固件源码
├── maoshu_Calendar.ino   主流程与按键
├── common.h              用户配置（引脚/版本/常量）
├── cal.cpp               桌面与天气页绘制、农历黄历
├── weather.cpp           Open-Meteo 拉取与退避重试
├── memo.cpp              巴法云 MQTT 备忘录
├── pomodoro.cpp          番茄钟
├── alarm.cpp             闹钟与蜂鸣器节拍
├── wallpaper.cpp         壁纸绘制
├── wallpaper_data.h      壁纸像素数据（由工具生成）
├── wallpapers/           壁纸原图 1~5.png
├── net.cpp               WiFi/配网/配置网页
├── settings.cpp          NVS 配置读写
├── tftUtil.cpp           主题与屏幕基座
└── font/                 16×16 中文点阵字库
tools/wallpaper_convert.py  壁纸转换工具
firmware/                 预编译固件 bin
```

## 许可

[MIT](LICENSE)
