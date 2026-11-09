# maoshu_Calendar 开发日志

> 按日期倒序记录每次改动、遇到的问题与解决办法，方便以后回溯。
> 版本记录与使用说明见《maoshu_Calendar_开发与使用说明.md》。

---

## 2026-11-09

### 字库扩充：1141 → 3778 字（GB2312 一级常用字）

**起因**：巴法云推送的作业文本在屏上出现 □ 方框（原文含「宇宙」「海棠」「提纲」）。

**处理**
- 项目没有独立字符集清单，唯一来源是 `font/memo_font_16.h` 的 `memoFontCp` 表；以它为基准，并入 **GB2312 一级常用字（3755）** 及本次文本用字，重排生成新表。
- 新字用宋体 `simsun.ttc` 16px、偏移 (0,0)、阈值 80 渲染；抽样 60 个原有字逐位比对 **差异 0**，确认与旧字模同一套参数。
- 原有 1141 字模原样保留，仅新增 2637 字（共 3778）。

**本次新增的缺字**：宇(U+5B87)、宙(U+5B99)、棠(U+68E0)、纲(U+7EB2)；`档`、`译` 已在表内。

**重生成时的坑**：重新生成头文件只保留了 CJK 段之前的内容，`MEMO_TITLE_COUNT` / `memoTitle`（32×32 备忘录大字）被覆盖，编译报未定义；已从 HEAD 原样补回。

**编译 / 固件**
- Flash：2,648,708 B / 3,145,728 B（84%），剩余约 497 KB（V01.4 时为 2,559,050 B，本次 +89.7 KB）
- RAM 全局变量：41,484 B（12%）
- `firmware/` 下 3 个 bin 已同步为新产物（ino.bin 2,688,640 B）

**烧录与校验（第二块板 COM12）**
- `arduino-cli upload` 写入 2,688,640 B、12.1 s，stub 报 `Hash of data verified`
- 整包读回：单次读取在 1.4–2.2 MB 处报 `Packet content transfer stopped`，改为 1MB+1MB+0x90680 分段读回拼接；与本地 bin **逐字节 0 差异**，SHA256 双方一致
- 串口日志：`maoshu_Calendar V01.4 [设备 xxxxxx] 启动`、WiFi 已连、天气 HTTP 200，无异常重启；`/status` 返回 `ver:V01.4`
- 屏上显示已确认正常（无方框）

**注意事项**
- 这块板是芯片原生 USB-Serial-JTAG：`--before usb_reset` 才能连上，进的是**下载模式**；`--after hard_reset` 无效，烧完必须手动按 RST / 拔插 USB 才会跑应用。
- 排查时 `--before no_reset` 可直接判断芯片是否还停在 bootloader。

---

## 2026-11-08

### 一、当日主线：可换壁纸（V01.2 → V01.3）

| 提交 | 时间 | 内容 |
| --- | --- | --- |
| `20a6e40` | 15:05 | feat: 新增可换壁纸 + 电脑端图片转换工具（V01.2） |
| `8d2986d` | 15:30 | fix: 补齐壁纸页缺失字模（壁/套/返） |
| `7e07518` | 15:44 | chore: 固件版本升至 V01.3，壁纸扩充为 2 套 |
| `1feda52` | 16:00 | fix: 修正壁纸颜色错位（红蓝互换） |
| `c2ae4c8` | 16:12 | feat: 日历页壁纸叠加主题蒙层，提升文字可读性 |
| `dea651c` | 晚间 | feat: V01.4 插画桌面改版，壁纸扩至 5 套并清理无用代码 |

标签：`V01.1`（5c74816）、`V01.2`（20a6e40）、`V01.3`（7e07518，后强制移动到该提交）、`V01.4`（dea651c，已推）

#### 1. 壁纸功能（V01.2）
- 日/夜两张壁纸随主题（7:00–19:00 为日间）自动切换，作为**日历页背景**。
- 图片以 RGB565 C 数组**编译进固件**（`wallpaper_data.h`），不占文件系统。
- 新增第 4 个页面 `PAGE_WALL`（壁纸页）：
  - 日历页**双击** → 壁纸页
  - 单击 = 下一套；双击 = 启用/停用；长按 = 回日历
  - 开关与套编号存 NVS（`cfg` 命名空间：`wpOn` / `wpSet`），掉电保留
- 电脑端工具 `tools/wallpaper_convert.py`（Python3 + PIL + tkinter）：
  - 选日间/夜间图 → 生成 `maoshu_Calendar/wallpaper_data.h`
  - 自动裁剪缩放 320×240，可调暗化程度；只选一张时自动生成另一时段版本
  - `python wallpaper_convert.py --demo` 恢复内置渐变壁纸

#### 2. 问题：壁纸页出现方框（缺字）
- 现象：`壁`、`套`、`返` 显示为方框。
- 原因：16×16 字库 `font/memo_font_16.h` 里没有这 3 个字。
- 处理：按宋体补入 `壁`(U+58C1)、`套`(U+5957)、`返`(U+8FD4)，字库 1136 → 1139 字，保持 Unicode 升序。

#### 3. 问题：壁纸颜色错乱（偏紫/偏绿）
- 现象：照片壁纸显示成蓝紫/绿色块，文字颜色正常。
- 原因：`TFT_eSprite` 写入显存时按**字节交换格式**存储颜色（`fillRect` 等绘图函数内部会 `color = (color>>8)|(color<<8)`），
  而 `pushImage` 默认 `_swapBytes = false`，直接 memcpy 原始数据，少了一次交换 → RGB565 高低字节颠倒 → 红蓝/亮度全乱。
- 处理（`wallpaper.cpp`）：`pushImage` 前临时 `setSwapBytes(true)`，之后还原。
- 备选方案（未采用）：在转换工具里直接输出字节交换后的数据。

#### 4. 问题：启用壁纸后时间/文字看不清
- 原因：照片明暗对比强，与日历文字抢视觉。
- 处理（`wallpaper.cpp`）：逐像素把壁纸向主题底色混合（直接写 sprite 缓冲区，同时保持字节交换格式）。
  - `WP_SCRIM_DAY 150`：日间向浅色底混合（数值越大越淡）
  - `WP_SCRIM_NIGHT 135`：夜间向深色底混合（数值越大越暗）
- 壁纸页预览仍显示**原图**，方便判断图片本身效果。

#### 5. 第二块板子烧录
- 端口：第一块 `COM11`，第二块 `COM12`（`VID_303A` Espressif）。
- 已用同一份 V01.3 固件烧录 COM12；新板 NVS 为空，首次开机会进入配网模式（热点 `maoshu_Calendar` / 密码 `12345678`）。

---

### 二、关键参数与命令（复用备忘）

**编译**（PowerShell）
```powershell
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile `
  --fqbn "esp32:esp32:esp32c3:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,CDCOnBoot=cdc" `
  --build-path "$env:TEMP\dudu_pomo_build" `
  "c:\Users\Administrator\Desktop\my-demo2\DuduClock_2.0\maoshu_Calendar"
```

**烧录**（同上参数，`upload -p COMxx` + `--input-dir "$env:TEMP\dudu_pomo_build"`）
```powershell
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" upload `
  -p COM12 `
  --fqbn "esp32:esp32:esp32c3:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,CDCOnBoot=cdc" `
  --input-dir "$env:TEMP\dudu_pomo_build" `
  "c:\Users\Administrator\Desktop\my-demo2\DuduClock_2.0\maoshu_Calendar"
```
- 报 `Wrong boot mode detected (0xc)` 时：按住 **BOOT** → 短按 **RST** → 松开 **BOOT**，再执行烧录。
- 每次编译后把 `%TEMP%\dudu_pomo_build` 下的 `maoshu_Calendar.ino.bin` / `.bootloader.bin` / `.partitions.bin` 覆盖到 `firmware/`。

**当前固件体量（V01.4，5 套壁纸）**
- Flash：2,559,050 B / 3,145,728 B（约 81%）
- RAM 全局变量：41,388 B / 327,680 B
- 每套壁纸 = 2 张 320×240 RGB565 ≈ 300 KB（日 + 夜各 153,600 B），最多再加 1 套

**git 提交身份**
```powershell
git -c user.name="DuduClock" -c user.email="duduclock@local" commit -m "..."
```

### 三、当日遗留

- 无。设备标识、V01.4 均已提交推送并打标签。

### 四、V01.4：桌面改版（当日后续）

**界面**
- 桌面改为「插画背景 + 右侧深色信息面板」：星期行（今日高亮）→ 大时钟 → 农历 → 今日天气+最高温 → 宜4项 → 忌4项（两行两列对齐）
- 删除月历网格、底部天气条、底部操作提示行、左上"本周日程"卡
- 新增独立 **7 天天气页**（星期/天气/最高℃/最低℃），底部仅"单击 返回桌面"

**按键（OneButton 三击支持）**
- 桌面：单击→备忘录，双击→天气，三击→壁纸；连击窗口 `setClickTicks(300)`
- 其余页长按 3 秒回桌面；备忘录长按清空、番茄钟单击启停/双击重置不变

**壁纸**
- 内置 5 套（原插画 + 4 张插画），原图存 `maoshu_Calendar/wallpapers/1.png~5.png`
- 去掉主题蒙层（`wallpaperDrawBg(s, scrim)` → `wallpaperDrawBg(s)`），夜间图由转换工具压暗
- 固件体量：5 套约 2.56MB / 3MB（81%），每套 +307KB

**问题与修复（当日）**
| 现象 | 原因 | 解决 |
| --- | --- | --- |
| 屏上出现 □ | 字库缺字 | 补 壁/套/返/桌/℃（共 1141 字） |
| 壁纸红蓝互换 | sprite 内部字节交换格式 | 写像素时做 `(c>>8)\|(c<<8)` |
| 天气页温度显示成日期 | JSON 先匹配到 `daily_units` 同名字段 | 改为匹配 `"temperature_2m_max":[` |
| 农历与天气重叠 | 行距不足 | 农历上移 8px、天气下移 2px |
| 按键无任何反应 | 只注册了 click/double/long，三击未注册且判定冲突 | 三击另注册 `attachTripleClick` + 300ms 窗口 |

**清理**
- 删除 `drawNum`、`SHEN12`、`calYearGZ`、`calDaysInMonth`、`calDow`、节日判断（`solarFestivalName`/`lunarFestivalName`/`LunarInfo.festival`）、`blend565` 与 `WP_SCRIM_*`、`tools/__pycache__`
- 编译体积由 2,560,644B 降至 2,559,050B
