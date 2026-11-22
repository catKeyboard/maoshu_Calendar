# maoshu_Calendar 开发日志

> 按日期倒序记录每次改动、遇到的问题与解决办法，方便以后回溯。
> 版本记录与使用说明见《maoshu_Calendar_开发与使用说明.md》。

---

## 2026-11-23

### 备忘录排序置顶、双向翻页与长按菜单（V01.17）

**排序改为新内容置顶（存储层）**
- `memoAddText()` 把新内容插到 `memoList[0]`，原条目整体后移；满 10 条时 `m9`（最旧）被挤出。
- NVS 键位不变，但 `m0` 语义改为“最新、`m9` 最旧”；`memoSaveToNVS()` 仍全量写 `m0`~`m9`，写入量没有增加，NVS 磨损不变。
- 推送后 `memoPageIdx = 0` 天然就是最新第 1 页，不再需要翻到末页找新内容。

**双向翻页（单键）**
- 单击下一页、末页回第 1 页；双击上一页、第 1 页回末页；三击回第 1 页。全部循环。
- 删除了 `memoAtLastPage()` 与“末页单击进番茄钟”的分支，退出备忘录只走长按菜单。
- 0 条或只有 1 页时，三种翻页手势均无动作。

**长按菜单（三段式清空）**
- 长按 3 秒只弹菜单，不改变数据：单击退出回桌面、双击进入清空确认、三击或再长按取消，8 秒无操作自动关闭。
- 清空确认内再长按才真正执行，单击/双击取消，8 秒超时；菜单期间收到新推送会自动关闭菜单。
- `drawMemoPage()` 在菜单/确认激活时一并重绘，避免 10 秒主题重绘把覆盖层盖掉；桌面页四类手势保持不变。

**体积**
- Flash 1,898,776 B / 3,145,728 B（60%），较 V01.16 +918 B。
- RAM 41,572 B / 327,680 B（12%），+8 B。
- 两块板（COM11、COM12）均已烧 V01.17，分段读回 SHA256 一致、逐字节差异 0。

**踩点**
- 双击“上一页”需等 300ms 连击判定，体感有约 0.3~0.6 秒延迟，属单键方案的固有代价。
- 置顶后不要只写变化键：顺序依赖 `memoSaveToNVS()` 全量重写，改增量写会把顺序弄乱。
- `boot_app0.bin` 只在 `firmware/` 下，不在 arduino-cli build 目录；烧录命令别指向 build path。

---

## 交接状态（截至 2026-11-22）

> 给接手的人：先看这一节，再按日期往下翻历史。本节记录的是 2026-11-22 的 V01.16 状态，最新改动见上方 2026-11-23 一节。仓库 `catKeyboard/maoshu_Calendar`。

### 1. 能力清单与源码地图

**现在能做什么**
- 桌面页：月历 + 农历/黄历（宜/忌）+ 大时钟 + 今日天气 + 底部 IP（小号 ASCII）。
- 天气页：本周 7 天预报（Open-Meteo，失败自动退避重试）。
- 备忘录页：巴法云 MQTT 推送 + 局域网网页推送，最近 10 条、单条 600 字节，NVS 断电保留；按键翻页、长按两次确认清空。
- 番茄钟页：专注/短休/长休循环，网页可改时长，阶段结束有蜂鸣提示音。
- 闹钟：最多 4 组，每天/仅一次，响 60 秒，间歇 300 ms/250 ms，任意键停止。
- 壁纸页：5 套内置壁纸（仅白天版），单击换套、双击启停。
- 配置网页两页：`/` 常用设置（番茄钟→闹钟→局域网备忘录），`/sys` 系统设置（网络→天气→巴法云）；启动信息页显示版本与 IP 约 10 秒。
- 字库：16×16 宋体 3,834 字（GB2312 一级 + 常用补充）+ 常用符号。

**改 X 去哪个文件**

| 文件 | 职责 |
|---|---|
| `maoshu_Calendar.ino` | 页面状态机、单按键（OneButton）单击/双击/长按/三击、主循环顺序、启动初始化 |
| `common.h` | 版本号 `FW_VERSION`、引脚（屏/按键 GPIO8/蜂鸣器 GPIO6）、页面枚举与公共常量 |
| `cal.cpp/.h` | 月历、农历/宜忌、桌面布局、启动信息页、桌面底部 IP 小字 |
| `weather.cpp/.h` | Open-Meteo 请求、8 秒超时、失败退避（60→…→1800 秒）、天气页绘制 |
| `memo.cpp/.h` | 备忘录 NVS、巴法云 MQTT、局域网入库同一路径、分页/清空确认、点阵文字绘制接口 |
| `pomodoro.cpp/.h` | 番茄钟计时与阶段切换、结束提示音（`beepFocusDone`/`beepRestDone`） |
| `alarm.cpp/.h` | 4 组闹钟、uint32 打包存 NVS、响铃排队、间歇蜂鸣 |
| `wallpaper.cpp/.h` | 壁纸套数切换、启用/停用、把 PROGMEM 壁纸写进 sprite |
| `wallpaper_data.h` | 5 套 RGB565 壁纸数据，每套 320×240 = 153,600 B |
| `net.cpp/.h` | WiFi 连接/AP 配网、WebServer 路由与两页 HTML、`/memo` 接口、NTP、`/css` |
| `settings.cpp/.h` | NVS 命名空间 `cfg`（天气/番茄钟/闹钟/巴法云）与 `net`（WiFi）读写 |
| `tftUtil.cpp/.h` | TFT 初始化、sprite 刷新、主题配色、时间/字符串小工具 |
| `font/memo_font_16.h` | 3,834 字 16×16 字模 + 码表（升序二分查找）+ 32×32 `memoTitle` 大字块 |

页面切换（单按键）：桌面单击→备忘录、双击→天气、长按→壁纸；备忘录单击翻页（末页进番茄）、双击回桌面；番茄双击重置、长按回桌面；壁纸单击换套、双击启停、长按回桌面；天气单击/长按回桌面。

### 2. 两块板状态

| 板子 | 端口 | 固件 | 当前 IP | 备注 |
|---|---|---|---|---|
| 第二块（网页/功能验证常用） | COM12 | V01.16 | 192.168.1.112（曾为 .125） | 2026-11-22 验证时未插，端口号是历史值 |
| 第一块 | COM11 | V01.16 | 待复位后启动页确认 | 2026-11-22 已烧录并校验，IP 以启动信息页/路由器 DHCP 列表为准 |

- IP 全部由路由器 **DHCP 分配**，重启或租约到期会漂移（.125 → .112 已发生过）。长期使用建议在路由器里**按 MAC 绑定**固定 IP；MAC 可从烧录日志 `MAC: …` 或启动串口日志读到，不要写进源码或文档。
- 端口号也可能随插拔顺序变化，烧录前先枚举串口确认（`[System.IO.Ports.SerialPort]::GetPortNames()` 或设备管理器），认准 `USB 串行设备 (VID_303A)`。

### 3. 未完成 / 待决事项

- **仓库可见性**：已由用户改为 **public**，2026-11-22 匿名 API 验证为 200、`private:false`、`visibility:public`、`default_branch:main`、`license:MIT`。如需回退 private，只能在 GitHub 网页/Token 操作。
- **V01.5 标签**：指向字库扩充那次提交（历史重写后为 `eb44b57`），不是当前 HEAD；仅为版本标记，不要误认为当前代码。
- **字库缺口**：`⟶`(U+27F6)、`⇒`、emoji、CJK 扩展区生僻字仍会显示空心方框（属预期）。再补字走 16 px SimSun 生成、并入 `memo_font_16.h`。
- **天气接口偶发不可达**：曾出现 DNS/TCP 正常但 HTTP 8 秒无响应（`-11` 读超时）。已有 60→120→240→…→1800 秒退避，成功后 30 分钟；失败保留旧数据。偶发首次取不到属正常。
- **配置页 Content-Type** 目前是 `text/html`，未带 `charset=utf-8`；页面内有 `<meta charset='utf-8'>`，中文正常，属可选优化。
- 未做 OTA、未做多按键/触摸、壁纸只有白天版。

### 4. 继续开发操作手册

**编译**（Windows，Arduino IDE 自带 CLI）

```
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile `
  --fqbn "esp32:esp32:esp32c3:PartitionScheme=huge_app,FlashFreq=40,FlashMode=dio,CDCOnBoot=cdc" `
  --build-path "$env:TEMP\dudu_pomo_build" maoshu_Calendar
```

- app 分区 3,145,728 B；编译结束看 `Sketch uses …` 与 `Global variables …`。
- 改了 `common.h` 版本号、字库或壁纸数据后必须全量重编（build path 里旧缓存偶尔不刷新，可先删目录）。

**进下载模式**：按住 **BOOT** 不放 → 点一下 **RST** → 松开 BOOT。串口会重新枚举。若 esptool 报 `Wrong boot mode detected (0xc)`，说明没进下载模式，重做一次即可，不要反复盲试。

**烧录**（应用地址固定 0x10000）

```
esptool --chip esp32c3 --port COMxx --before usb_reset --after hard_reset write_flash -z `
  --flash_mode dio --flash_freq 40m --flash_size detect `
  0xe000 firmware\boot_app0.bin `
  0x0    firmware\maoshu_Calendar.ino.bootloader.bin `
  0x8000 firmware\maoshu_Calendar.ino.partitions.bin `
  0x10000 firmware\maoshu_Calendar.ino.bin
```

- 想烧最新编译产物，把后三个路径换成 `$env:TEMP\dudu_pomo_build\maoshu_Calendar.ino.*.bin`；烧完记得把三个 bin 同步回 `firmware/` 再提交。
- **RTS 复位对这块板无效**：`--after hard_reset` 不会真正重启，烧完必须**手按 RST** 才会运行新固件。

**分段读回校验**（app >1 MB，整包一次读会断流）

1. 先枚举端口，确认目标板。
2. 每段单独连接；芯片已在跑应用时 `--before no_reset` 连不上，改用 `--before usb_reset`（每段都加，读回不受影响）。
3. 分段：`0x0 0x30B0`（bootloader 12,464）、`0x8000 0xC00`（partitions 3,072）、`0xE000 0x2000`（boot_app0 8,192）、app 先读 `0x10000 0x100000`，再读 `0x110000` 起**剩余长度**（app 总长 − 1,048,576）。
4. 两段按内存顺序拼接成完整 app，分别对源文件与读回文件算 SHA256，并逐字节比较；差异必须为 0。

**抓串口日志**：用任意串口工具（PuTTY / 串口助手 / `[IO.Ports.SerialPort]`）打开目标口，**115200 8N1**；上电或按 RST 即可看到启动横幅、WiFi/天气、巴法云、备忘录等日志。注意烧录时不要同时占用串口。

**查设备 IP**：启动信息页停留约 10 秒；或浏览器访问 `http://<IP>/status`；实在不行看路由器 DHCP 客户端列表。

### 5. 踩坑速查

| 现象 | 原因 / 处理 |
|---|---|
| 读回在 1.4–2.2 MB 处 `Packet content transfer stopped` | 整包读超限，改 1 MB + 剩余分段，再拼接比对 |
| 烧完没反应，仍跑旧固件 | 板子 EN 没接 RTS，`hard_reset` 无效，手按 RST |
| `Failed to connect: No serial data received`（读回时） | 芯片在跑应用，`--before no_reset` 无效，改用 `--before usb_reset` |
| `Wrong boot mode detected (0xc)` | 未进下载模式：按住 BOOT → 点 RST → 松 BOOT |
| 壁纸红蓝互换 | sprite 内部是字节交换格式，写缓冲前要做 `(c>>8)\|(c<<8)` |
| 网页提交中文变乱码/收不到字段 | 表单必须 UTF-8 且带 `application/x-www-form-urlencoded`；`WebClient`/裸 POST 不带 Content-Type 时 `server->arg()` 解析不到 |
| 推送 600 字节消息收不到 | MQTT 默认缓冲太小，已设 `setBufferSize(1024)` |
| 重启后同一条巴法云消息重复入库 | 巴法云会重放保留消息；用 `lastPush` 去重（仅云端生效，局域网不去重） |
| 天气首次重试等了 120 秒 | 退避顺序写反（先翻倍再当等待）；应“本次用当前阶梯、下次再翻倍”（`waitGap`/`backoff` 分开） |
| 字库重生成后备忘录大标题没了 | 生成脚本只保留 CJK 段之前内容，会冲掉 `memoTitle` 32×32 块，生成后从 HEAD 补回 |
| 字库出现两行 `MEMO_CJK_COUNT` | 重生成时 head 已含旧 `#define` 又追加一次；编译器取后值不报错，需核对并删旧行 |
| 字库缺字显示方框 | 码点不在字库时 `drawMemoChar` 画空心方框；补符号用 SimSun 16 px 并保持码表升序 |
| git-filter-repo 替换文件不生效 | 替换文件带 BOM 导致匹配失败；用 `UTF8Encoding($false)` 写无 BOM |
| PowerShell 取标签指向报 `ambiguous argument '+'` | 用 `$ref = $t + '^{}'; git rev-parse --verify $ref` |

### 6. 后续可做与空间预算（V01.16 实测）

- Flash：**1,897,858 / 3,145,728 B（60%）**，剩余 **1,247,870 B（约 1.19 MB，40%）**；RAM 41,564 / 327,680 B（12%），堆余量充足。
- 构成（`size -A` + `nm -S`）：
  - `.flash.text` 代码 **768,512 B**；
  - `.flash.rodata` 常量 **1,058,688 B**，其中壁纸 **768,000 B（约 73%）**、字库 **约 130,356 B（3,834×32 + 码表，约 12%）**、网页 HTML/文案若干；
  - `.flash.rodata_noload` 16,695 B（NVS 相关，不占 RAM）。
- 加功能的大致代价：
  - 新增 1 套壁纸：+153,600 B（可先降为 8-bit 调色板或压缩来省空间，但要改绘制链）；
  - 新增 1,000 个汉字：+32,000 B 字模 + 2,000 B 码表；
  - 再拆/扩充配置页：每页十几 KB 量级，可忽略。
- 不建议直接加 OTA：当前 `huge_app` 无 OTA 槽，需改分区表并牺牲约一半 app 空间；要做先规划双槽与回滚。
- 可优先考虑的小功能：闹钟贪睡、天气页加降水概率、备忘录按时间排序/删除单条、配置页显示剩余 NVS、局域网推送可选 key。

---

## 2026-11-22

### 一、备忘录支持局域网推送（V01.10）

- 复用现有 `WebServer`：`GET /memo` 返回手机表单页，`POST /memo` 读取 `text` 字段，直接调 `memoAddText(t, false)`，与巴法云消息共用同一入库/存储路径（去 `\r`、trim、UTF-8 截断（V01.10 为 240 字节，V01.11 起 600 字节）、满 10 条丢最旧、写 `memo` 命名空间并重绘）。
- `memoAddText(txt, fromCloud)` 增加来源标志：只有云端消息做 `lastPush` 保留消息去重，局域网同文本可以重复提交。
- 不加登录或 key；页面与 README 均明示“同网段任何人可写，公共网络勿用”。页面与源码不写入真实 UID/主题/SSID。

### 二、容量上限与前端计数（V01.11）

- `MEMO_MAX_BYTES` 240 → **600**（约 200 汉字）；`PubSubClient` 缓冲 512 → **1024**，否则 600 字节 payload 放不下。
- NVS 占用核算：NVS 分区 20,480 B；600 B 字符串约 19 个数据槽 + 1 个索引槽 = 约 640 B/条，10 条约 6.4 KB，加 `lastPush` 约 7 KB（约 34%），仍留有垃圾回收页。
- `/memo` 用 `TextEncoder` 按 UTF-8 实时显示“已用 XX/600 字节”；超限变红、按钮禁用、`onsubmit` 兜底；后端也直接拒绝超过 600 字节的提交，不再静默截断。页面中的上限用 `__MAXB__` 占位符在发送时替换，前后端不会漂移。

### 三、推送跳页与翻页（V01.11 → V01.12 调整）

- V01.11 初版同时做了“推送后不切页”和“删除 6 秒自动翻页”。
- V01.12 按要求恢复推送后自动跳回备忘录页第 1 页；6 秒自动翻页保持删除，页内翻页只由按键触发。
- 经验：**“推送后跳转”和“页内自动翻页”是两件事**，需求变化时要拆开确认，避免一起改后再回滚。

### 四、启动信息页与桌面 IP（V01.11）

- 新增启动信息页：标题、版本、当前 IP、访问提示；`BOOT_INFO_MS = 10000`（**10 秒**）。原流程在 NTP 同步后约 1~2 秒就被桌面覆盖，来不及记录 IP；10 秒足够看清/拍照。
- 信息页期间按任意键立即进桌面；配网模式仍显示原热点提示页。
- 桌面宜/忌下方 y=202 用 4×8 小号 ASCII（8×16 点阵行列两两合并）显示 `IP x.x.x.x`：距忌行 9 px、距卡片底 18 px、宽约 80 px（面板 104 px），不挤压原排版，且只在整页刷新时绘制，不额外重绘。

### 五、配置页“打不开”排查（V01.12 烧录后）

- 串口确认版本 **V01.12**；DHCP 把 IP 从 `192.168.1.125` 改成了 **`192.168.1.112`**。
- curl 实测：`/` 返回 200、`text/html`、11,401 字节，与源码 `CONFIG_HTML` 的 UTF-8 字节数完全一致，结尾有 `</html>`；`/status` 200、`/memo` 2,062 字节；`GET /weather` 返回 404 属预期（该路由只注册了 POST）。
- 浏览器实际渲染正常、控制台无报错；结论是用户仍访问旧 IP 或浏览器缓存，不是设备返回或页面缺陷。Content-Type 未带 `charset`，但 HTML 内有 `<meta charset='utf-8'>`，可作后续可选优化。

### 六、版本与体积

- V01.10 1,889,926 B；V01.11 1,893,142 B；V01.12 **1,893,122 B / 3,145,728 B（60%）**，RAM 41,556 B（12%）。
- V01.10/V01.11 没有单独提交，三个版本同属 V01.12 这一次提交。

### 七、清空、两页配置、表单内嵌与符号字库（V01.13–V01.16）

**V01.13 备忘录清空（两个入口都要二次确认）**
- 网页：`POST /memo/clear` + 前端 `confirm`，提示只删备忘录、不影响其它设置。
- 设备：备忘录页长按 3 秒**只弹确认框**，再长按一次才执行；单击/双击/三击取消，8 秒未再长按自动取消；确认期间收到新推送也自动取消，避免把刚到的作业清掉。
- NVS：在 `memo` 命名空间逐个 `remove("m0")~("m9")` 并 `remove("cnt")`，不是只清内存列表；**保留 `lastPush`**（它是巴法云保留消息的去重标记，不是条目，保留可防止清空后回灌）。`cfg`/`net` 命名空间不碰。

**V01.14 配置页拆成两页**
- `/` 常用设置：番茄钟 → 闹钟 → 局域网备忘录；`/sys` 系统设置：网络 → 天气地区 → 巴法云。
- 共用 CSS 抽到 `/css`，两页顶部导航当前页蓝底高亮；各表单 action 不变，保存提示页的返回链接分别指向 `/` 或 `/sys`。
- 体积：页面常量 12,322 B（原单页 11,401 B），但单次加载变小（`/`≈4.6 KB、`/sys`≈8.9 KB）。

**V01.15 备忘录表单并入 `/`，提交不再跳独立提示页**
- 删除“打开备忘录推送页”入口，文本域、600 字节实时计数、超限禁用、清空按钮全部放在 `/`；说明文字压缩到两行，文本域 96 px。
- `POST /memo`、`POST /memo/clear` 改为 **303 回 `/?m=ok|empty|long|cleared`**，页面读取查询参数在导航下方显示绿色/红色提示条；超长时带 `n=字节数`。
- `/memo` GET 改为 **302 回 `/`**：保留旧书签可用，同时表单只在 `/` 维护一份。
- 踩点：拆两页后曾出现“备忘录既在原 `/memo`、又要搬到 `/`”的两处表单风险，容易再次不同步；最终用重定向统一到 `/` 解决。

**V01.16 字库补常用符号（修“→ 显示成方框”）**
- 先定位：扫全部 `.cpp/.ino/.h` 的界面字面量，缺失字符 **0**，屏上方框不是固件文案；是用户推送的备忘录里含 `→`（U+2192），旧字库无此码点，按设计画空心方框。
- 3778 → **3834** 字，新增 56 个符号：← ↑ → ↓、• ○ ●、–、①~⑩、Ⅰ~Ⅻ、° ℉ ％ ¥ ￥、【 】〔 〕、～、× ÷ ± √ ≥ ≤ ≠、★ ☆ ※ ─ ╱ ╲ □ ▲ △。
- 生成用 Windows **SimSun 16 px**；先对已有字“备/忘/录/注/意”逐位比对，差异 **0**，风格一致。ASCII 段与 `memoTitle` 32×32 大字块原样保留，头文件 UTF-8 无 BOM、LF。
- 踩点：重生成脚本的 head 截取已包含旧 `#define MEMO_CJK_COUNT`，又追加了一次，出现 3778/3834 两行定义；编译器取后者不报错，核对码表长度才发现，已删旧行。
- 仍未覆盖：`⟶`(U+27F6)、`⇒`、emoji、CJK 扩展区生僻字，仍会显示方框。

**体积**
- V01.13 1,894,846 B；V01.14 1,896,118 B；V01.15 1,895,954 B（删除独立 `/memo` 页后略降）；V01.16 **1,897,858 B / 3,145,728 B（60%）**；RAM 41,564 B（12%）。
- V01.13~V01.16 同属一个提交，四个标签都指向该提交。

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
