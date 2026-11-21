# maoshu_Calendar 开发日志

> 按日期倒序记录每次改动、遇到的问题与解决办法，方便以后回溯。
> 版本记录与使用说明见《maoshu_Calendar_开发与使用说明.md》。

---

## 2026-11-21

### 一、内置壁纸换新，随后取消夜间模式（Flash 84% → 59%）

**换图（保留 5 套结构）**
- 按画面内容对应替换，而不是按文件名猜：第 1 套古镇灯笼街、第 3 套雪中庙宇、第 4 套红墙秋叶、第 5 套月下书案；第 2 套三只猫没有新图，原样保留。
- 转换统一走 `tools/wallpaper_convert.py`：`ImageOps.fit` 裁到 320×240（不变形）；日间版向白混合 25%，夜间版亮度压到 45%。
- 头文件仍按 `0x%04X` 存 RGB565，运行时由 `wallpaper.cpp` 读 PROGMEM 后 `(c>>8)|(c<<8)` 写进 sprite —— sprite 内部是**字节交换格式**，这样才不会红蓝互换（老坑，见第五节）。
- 该轮每套日+夜 = 2 × 76,800 × 2 B = 307,200 B，5 套 1,536,000 B；Flash 2,648,708 B（84%）。

**改为以 `wallpapers/` 目录为准重生成，并取消夜间**
- 目录 5 张（1.png~5.png），**套号 = 文件名顺序**；每套只生成白天版。
- `currentImage()` 直接返回 `wallpaperDay[i]`；`themeIsDay()` 删除（它只服务壁纸；主题配色的 `themeTick()` 昼夜切换不动）。
- 壁纸页底部状态去掉“日间/夜间”字样；转换工具删 `--night`、`--night-darken`、夜间压暗滑块、夜间选择弹窗。
- 体积：每套 153,600 B，5 套 768,000 B；固件 **1,880,524 B / 3,145,728 B（59%）**，比 84% 降 25 个百分点、约 768 KB。

### 二、天气接口不可达：排查结论与三项改动

**现象**：开机首取 `天气获取: HTTP -11`，桌面停在“天气获取中”。

**定性（环境问题，代码只是放大了影响）**
- `-11` = `HTTPC_ERROR_READ_TIMEOUT`：DNS 已解析、TCP 已建立，8 秒内等不到响应；不是 DNS 失败，也不是拒绝连接。
- 电脑端对照同一接口：`api.open-meteo.com` 的 http 与 https 均 20 秒超时（http_code 000），但 `Test-NetConnection 51.81.167.44 -Port 80` 为 True、DNS 正常、百度 200 —— 同一出口下能握手却收不到 HTTP 响应。
- `WL_CONNECTED` 时 DHCP 与 DNS 已就绪，首取时机不算早；用 `POST /weather` 强制即时重取仍是 -11，可复现。

**三项改动**
1. **失败退避**：`waitGap`（本次等待）与 `backoff`（下次阶梯）分开；失败按 **60 → 120 → 240 → 480 → 960 → 1800 秒封顶**，成功后回 30 分钟；失败时保留旧数据不清空。
2. **首取不白耗周期**：`weatherBegin()` 只在 `WL_CONNECTED` 时取并计时；没连上留给 loop 补首取。
3. **配网更稳且有出口**：首次等 **20 秒**（原 12 秒）+ 失败**重试一次 10 秒**；`netLoop()` 中若 STA 在后台连上就**自动退出配网**（`portalMode=false`、关热点、重设 NTP/时区、重绘桌面），不必重启。

**实测**
- 这块板 WiFi 实际要 **15 秒**才连上：旧 12 秒上限会误判失败进配网；新逻辑出现“连接超时，重试一次...”后连接成功。
- 接口恢复后：启动 3.5 秒首取 `HTTP 200`，之后按 30 分钟周期，退避阶梯未被触发（修复版尚未经历失败场景）。

### 三、宜/忌配色

- 宜（“宜”字头 + 4 项）= **0x07E0**（0,255,0，满绿）；忌（“忌”字头 + 4 项）= **0xF800**（255,0,0，满红通道）。
- 不用主题红 `0xE000`（226,0,0）：实机偏暗发粉；满红通道、不掺蓝才显得“正红”。
- 右侧信息面板底色固定深色（`0x1907`，有壁纸 alpha 170 / 无壁纸 235），昼夜主题下都够暗，所以这两色用固定值，不随主题变。
- 其它文字、分隔线、背景一律未动。

### 四、版本划分与仓库状态

- **V01.5**（提交 `d3a6a7c`，标签已推）：只含字库扩充（1141 → 3778），不含本次改动。
- **V01.6**：壁纸换新/去夜间 + 天气退避重试 + 配网自愈 + 宜忌配色；已烧到 COM12 并分段读回校验。
- HEAD = `origin/main` = `d3a6a7c`；标签 `V01.1`~`V01.5` 齐全，`V01.5` 指向 `d3a6a7c`。
- 未提交清单（`git status`）：`firmware/maoshu_Calendar.ino.bin`、`cal.cpp`、`common.h`、`net.cpp`、`tftUtil.cpp/.h`、`wallpaper.cpp/.h`、`wallpaper_data.h`、`wallpapers/1~4.png`、`weather.cpp`、`开发与使用说明.md`、`tools/wallpaper_convert.py`。
- 提交后记得把 `V01.6` 打上，并把 `V01.5` 保持指向 `d3a6a7c`（不要再移动）。

### 五、踩坑备忘（本轮新增）

- **整包读回会断流**：app 分区 >1 MB 时，单次 `read_flash` 在 1.4–2.2 MB 处报 `Packet content transfer stopped`。改为 1 MB + 剩余分段，每段独立 `--before no_reset` 会话，拼接后先比长度再比 SHA256。
- **RTS 复位对这块板无效**：EN 没接 RTS，`--after hard_reset`/`soft_reset` 都没用（`soft_reset` 只支持 ESP8266），烧完必须手按 RST 或拔插 USB；`--before no_reset` 能连上就说明芯片仍在 bootloader。
- **退避代码顺序坑**：先 `backoff *= 2` 再拿它当等待间隔，首次重试就会变成 120 秒。正确写法是“本次用当前阶梯、下次再翻倍”（`waitGap` / `backoff` 两个变量）。
- **触发天气重取的两种方式**：`POST /weather`（配置页保存地区）会调 `weatherRefreshNow()`；抓串口验证很方便。
- **POST 必须带表单 Content-Type**：`WebClient.UploadString` 默认不带头，服务器 `server->arg()` 解析不到；用 `curl --data-urlencode` 才可靠。诊断时曾把地区名写成乱码，需再 POST 一次纠正。
- **字库重新生成会覆盖大字块**：只保留 CJK 段之前内容，会把 `MEMO_TITLE_COUNT` / `memoTitle`（32×32）冲掉，生成后必须从 HEAD 补回。

### 六、空间规划备忘

- 基准：每套 320×240 **白天**壁纸 = **153,600 B**；16px 中文字模 = **32 B/字**（码表另有 2 B/字）。
- 当前 `1,881,274 B / 3,145,728 B`，剩余 **1,264,454 B（约 1.26 MB，41%）**；分区为 huge_app(3MB)，无需改分区。
- 固件构成（`riscv32-esp-elf-size -A` + `nm -S` 实测）：
  - `.flash.text`（代码）761,648 B；`.flash.rodata`（常量）1,048,984 B；`.eh_frame` 39,688 B；`.flash.rodata_noload` 16,695 B
  - `wallpaperDay` **768,000 B**（占固件 40.8%、占 rodata 73.2%）
  - 字库 `memoFontData` 120,896 B + `memoFontCp` 7,556 B + ASCII 字模 4,560 B + `memoTitle` 384 B ≈ **133,396 B（7.1%）**
  - 配置页 HTML `CONFIG_HTML` **9,283 B**
  - rodata 其余（各种字符串、农历表等）约 138,305 B

**还能加（按当前余量 1,264,454 B）**
- 再来 5 套白天壁纸：5 × 153,600 = 768,000 B → 余约 496,454 B（只改 `wallpaper_data.h`，无库/无分区/无硬件改动）
- 字库扩到 GB2312 二级（约 2700 字 × 32 B ≈ 86,400 B + 码表 5,400 B ≈ 92 KB）→ 余约 1.17 MB（只重生成字库）
- 诗词/每日一句库：200 条 × 14 字 × 32 B ≈ 89,600 B + 解析代码（无库）
- 番茄钟日统计：存 NVS，**0 B**（只加 UI 与 NVS 结构）
- 蜂鸣器提示（番茄钟结束/到点）：**0 B**，需 1 个 GPIO + 有源蜂鸣器（硬件依赖）

**可省回来（按实测占比）**
- 壁纸 768,000 B（40.8%）：5 套→3 套省 307,200 B；改 8 位调色板（每套 76,800 B + 768 B 调色板）5 套共约 387,840 B，省 380,160 B；降到 160×120 再放大每套 38,400 B，省 576,000 B（画质明显下降）；搬进 LittleFS（需自定义分区表 app 2 MB + FS 约 1.9 MB）app 内省 768,000 B（总 Flash 不省）
- 字库 133,396 B：按使用频率裁到 1500 字约 51,000 B，省约 **82,000 B**（有漏字风险，当前只占 7.1%，不建议动）
- `CONFIG_HTML` 9,283 B：压缩/minify 可省 2–3 KB，整块删省 9.3 KB（损失换网与地区配置页）
- 代码 761,648 B：加 `-flto` 一般减 5–8%（约 38,000–61,000 B），需改编译参数，Arduino 下偶有链接问题
- 调试串口日志字符串：全部关掉也就几 KB（`memoTitle` 大字块仅 384 B，可忽略）


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
