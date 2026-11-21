#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "cal.h"
#include "settings.h"
#include "weather.h"
#include "pomodoro.h"
#include "alarm.h"
#include "net.h"

// 网络：WiFi连接 + 精简AP配网 + NTP校时
static Preferences netPrefs;
static WebServer* server = nullptr;
static bool portalMode = false;

// 配网模式屏幕提示
static void drawPortalScreen(){
  reflashTFT();
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  clk.fillSprite(backFillColor);
  drawMemoString(clk, (320 - 160) / 2, 50, "未连接WiFi，进入配网", orangeColor);
  drawMemoString(clk, (320 - 104) / 2, 95, "手机连接热点:", penColor);
  drawMemoString(clk, (320 - 208) / 2, 125, String(AP_SSID) + " 密码 " + AP_PASS, cyanColor);
  drawMemoString(clk, (320 - 88) / 2, 165, "浏览器打开:", penColor);
  drawMemoString(clk, (320 - 88) / 2, 195, "192.168.4.1", cyanColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}

// 两页共用样式(单独路由 /css，避免两页各存一份)
static const char PAGE_CSS[] PROGMEM = R"rawliteral(
body{font-family:sans-serif;max-width:460px;margin:0 auto;padding:10px;font-size:1rem}
h3{margin:16px 0 8px;font-size:1.05rem;border-left:4px solid #1E90FF;padding-left:8px}
input,select{width:100%;height:38px;font-size:.95rem;margin:4px 0;padding:0 6px;box-sizing:border-box}
button{width:100%;height:40px;font-size:1rem;background:#1E90FF;color:#fff;border:0;border-radius:6px;margin-top:6px}
#cur{padding:8px;background:#f0f7ff;border-radius:6px;font-size:.95rem}
#list{max-height:34vh;overflow-y:auto;margin-top:8px;border:1px solid #eee;border-radius:6px}
.ap{padding:9px 8px;border-bottom:1px solid #eee;display:flex;justify-content:space-between;cursor:pointer}
.ap:active{background:#e3f2fd}
.row{display:flex;gap:8px}
input[type=checkbox]{width:26px;height:26px;margin:0;padding:0;flex:0 0 26px}
a{color:#1E90FF;font-size:.9rem}
.tip{color:#888;font-size:.85rem;margin:2px 0 6px}
nav{display:flex;gap:8px;margin:0 0 8px}
nav a{flex:1;text-align:center;padding:9px 0;border-radius:6px;background:#eef4fb;text-decoration:none;font-size:.95rem}
nav a.on{background:#1E90FF;color:#fff;font-weight:600}
textarea{width:100%;height:96px;font-size:1rem;padding:8px;box-sizing:border-box;border:1px solid #ccc;border-radius:6px;resize:vertical}
.msg{display:none;padding:8px;border-radius:6px;margin:0 0 8px;font-size:.95rem}
.msg.ok{background:#e8f5e9;color:#1b5e20}
.msg.err{background:#ffebee;color:#b71c1c}
)rawliteral";

// 配置页(系统)：网络 / 天气地区 / 巴法云推送
static const char SYS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>maoshu_Calendar 系统设置</title>
<link rel='stylesheet' href='/css'></head><body>
<nav><a href='/'>常用设置</a><a href='/sys' class='on'>系统设置</a></nav>

<h3>网络</h3>
<div id='cur'>正在获取状态…</div>
<form action='/save' method='post'>
<input id='ssid' name='ssid' placeholder='WiFi名称(点下方列表选择)'>
<input name='pass' type='password' placeholder='WiFi密码'>
<button type='submit'>保存并重启</button>
</form>
<p><a href='#' onclick='loadScan();return false'>重新扫描周边WiFi</a></p>
<div id='list'>正在搜索周围WiFi…</div>

<h3>天气地区</h3>
<div class='tip' id='wcur'></div>
<form action='/weather' method='post'>
<select id='prov' onchange='fillCity()'></select>
<select id='city' name='city' onchange='fillCoord()'></select>
<div class='row'>
<input id='lat' name='lat' placeholder='纬度'>
<input id='lon' name='lon' placeholder='经度'>
</div>
<button type='submit'>保存地区</button>
</form>
<div class='tip'>选择省市会自动填好经纬度，也可直接手填(保留两位小数)</div>

<h3>巴法云推送</h3>
<div class='tip'>UID=巴法云控制台32位私钥，主题名与小程序的发送主题一致</div>
<form action='/bemfa' method='post'>
<input id='uid' name='uid' placeholder='私钥UID(32位)' maxlength='40'>
<input id='topic' name='topic' placeholder='主题名' maxlength='32'>
<button type='submit'>保存并连接</button>
</form>
<div class='tip' id='bcur'></div>

<div class='tip' id='ver' style='text-align:center;margin-top:14px'></div>

<script>
var REG=[["北京",[["北京",39.90,116.41]]],["天津",[["天津",39.13,117.20]]],["上海",[["上海",31.23,121.47]]],["重庆",[["重庆",29.56,106.55]]],
["河北",[["石家庄",38.04,114.51],["唐山",39.63,118.18],["保定",38.87,115.46],["邯郸",36.63,114.54],["秦皇岛",39.94,119.60]]],
["山西",[["太原",37.87,112.55],["大同",40.08,113.30],["临汾",36.09,111.52],["运城",35.03,111.00]]],
["内蒙古",[["呼和浩特",40.84,111.75],["包头",40.66,109.84],["赤峰",42.26,118.89],["鄂尔多斯",39.61,109.78]]],
["辽宁",[["沈阳",41.80,123.43],["大连",38.91,121.61],["鞍山",41.11,122.99],["锦州",41.10,121.13]]],
["吉林",[["长春",43.82,125.32],["吉林",43.84,126.55],["延吉",42.89,129.51]]],
["黑龙江",[["哈尔滨",45.80,126.53],["齐齐哈尔",47.35,123.92],["大庆",46.59,125.10],["牡丹江",44.55,129.63]]],
["江苏",[["南京",32.06,118.80],["苏州",31.30,120.58],["无锡",31.49,120.31],["常州",31.81,119.97],["南通",31.98,120.89],["徐州",34.26,117.19]]],
["浙江",[["杭州",30.27,120.15],["宁波",29.87,121.55],["温州",27.99,120.70],["绍兴",30.00,120.58],["金华",29.08,119.65]]],
["安徽",[["合肥",31.82,117.23],["芜湖",31.35,118.43],["蚌埠",32.92,117.39],["安庆",30.51,117.05],["黄山",29.71,118.34]]],
["福建",[["福州",26.07,119.30],["厦门",24.48,118.09],["泉州",24.87,118.68],["漳州",24.51,117.65],["龙岩",25.08,117.03]]],
["江西",[["南昌",28.68,115.86],["九江",29.71,116.00],["赣州",25.83,114.94],["上饶",28.45,117.94]]],
["山东",[["济南",36.65,117.12],["青岛",36.07,120.38],["烟台",37.46,121.45],["潍坊",36.71,119.16],["临沂",35.10,118.36],["济宁",35.41,116.59]]],
["河南",[["郑州",34.75,113.63],["洛阳",34.62,112.45],["开封",34.80,114.31],["南阳",32.99,112.53],["新乡",35.30,113.93]]],
["湖北",[["武汉",30.59,114.31],["宜昌",30.69,111.29],["襄阳",32.01,112.12],["荆州",30.33,112.24]]],
["湖南",[["长沙",28.23,112.94],["株洲",27.83,113.13],["衡阳",26.89,112.57],["岳阳",29.36,113.13],["张家界",29.13,110.48]]],
["广东",[["广州",23.13,113.26],["深圳",22.54,114.06],["珠海",22.27,113.58],["佛山",23.02,113.12],["东莞",23.02,113.75],["汕头",23.35,116.68],["湛江",21.27,110.36]]],
["广西",[["南宁",22.82,108.37],["桂林",25.27,110.29],["柳州",24.31,109.41],["北海",21.48,109.12]]],
["海南",[["海口",20.04,110.32],["三亚",18.25,109.51]]],
["四川",[["成都",30.57,104.07],["绵阳",31.47,104.68],["乐山",29.55,103.77],["宜宾",28.77,104.62],["南充",30.84,106.11]]],
["贵州",[["贵阳",26.65,106.63],["遵义",27.73,106.93],["六盘水",26.59,104.83],["安顺",26.25,105.95]]],
["云南",[["昆明",25.04,102.71],["大理",25.61,100.27],["丽江",26.86,100.23],["曲靖",25.49,103.80],["景洪",22.00,100.80],["香格里拉",27.83,99.71]]],
["西藏",[["拉萨",29.65,91.14],["日喀则",29.27,88.88],["林芝",29.65,94.36]]],
["陕西",[["西安",34.34,108.94],["宝鸡",34.36,107.24],["咸阳",34.33,108.71],["延安",36.59,109.49],["汉中",33.07,107.02]]],
["甘肃",[["兰州",36.06,103.83],["天水",34.58,105.72],["酒泉",39.73,98.49],["张掖",38.93,100.45]]],
["青海",[["西宁",36.62,101.78],["海东",36.50,102.10],["格尔木",36.40,94.90]]],
["宁夏",[["银川",38.49,106.23],["石嘴山",38.98,106.38],["吴忠",37.99,106.20]]],
["新疆",[["乌鲁木齐",43.83,87.62],["喀什",39.47,75.99],["伊宁",43.92,81.32],["库尔勒",41.73,86.17],["克拉玛依",45.58,84.89]]],
["台湾",[["台北",25.03,121.57],["高雄",22.63,120.30],["台中",24.15,120.68]]],
["香港",[["香港",22.32,114.17]]],["澳门",[["澳门",22.20,113.54]]]];

var provSel=document.getElementById('prov'),citySel=document.getElementById('city');
for(var i=0;i<REG.length;i++){
  var o=document.createElement('option');o.value=i;o.textContent=REG[i][0];provSel.appendChild(o);
}
function fillCity(){
  var p=REG[provSel.value][1];
  citySel.innerHTML='';
  for(var i=0;i<p.length;i++){
    var o=document.createElement('option');o.value=i;o.textContent=p[i][0];citySel.appendChild(o);
  }
  fillCoord();
}
function fillCoord(){
  var p=REG[provSel.value][1][citySel.value];
  document.getElementById('lat').value=p[1];
  document.getElementById('lon').value=p[2];
}
fillCity();

fetch('/status').then(function(r){return r.json()}).then(function(s){
  var el=document.getElementById('cur');
  if(s.mode==='ap') el.textContent='当前未连接WiFi，请选择网络配置';
  else if(s.ssid) el.textContent='当前网络：'+s.ssid+' ('+s.ip+')';
  else el.textContent='WiFi未连接';
  document.getElementById('wcur').textContent='当前地区：'+s.city+' ('+s.lat+', '+s.lon+')';
  document.getElementById('lat').value=s.lat;
  document.getElementById('lon').value=s.lon;
  document.getElementById('uid').value=s.uid;
  document.getElementById('topic').value=s.topic;
  document.getElementById('bcur').textContent=s.uid?'已配置，设备会自动重连巴法云':'尚未配置';
  document.getElementById('ver').textContent='maoshu_Calendar 固件 '+s.ver+' · 设备 '+s.id;
}).catch(function(){});

function loadScan(){
  var list=document.getElementById('list');
  list.textContent='正在搜索周围WiFi…';
  fetch('/scan').then(function(r){return r.json()}).then(function(d){
    list.innerHTML='';
    if(!d.length){list.textContent='未发现WiFi，请手动输入';return;}
    d.forEach(function(n){
      var div=document.createElement('div');
      div.className='ap';
      var name=document.createElement('span');
      name.textContent=n.ssid;
      var sig=document.createElement('span');
      var s=n.rssi>-55?'强':(n.rssi>-70?'中':'弱');
      sig.textContent=s+' '+n.rssi+'dBm';
      sig.style.color=n.rssi>-55?'#4CAF50':(n.rssi>-70?'#FF9800':'#F44336');
      div.appendChild(name);div.appendChild(sig);
      div.onclick=function(){document.getElementById('ssid').value=n.ssid;window.scrollTo(0,0);};
      list.appendChild(div);
    });
  }).catch(function(){list.textContent='扫描失败，请手动输入';});
}
loadScan();
</script>
</body></html>
)rawliteral";

// 配置页(常用)：番茄钟 / 闹钟 / 局域网备忘录
static const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>maoshu_Calendar 常用设置</title>
<link rel='stylesheet' href='/css'></head><body>
<nav><a href='/' class='on'>常用设置</a><a href='/sys'>系统设置</a></nav>
<div id='msg' class='msg'></div>

<h3>番茄钟</h3>
<div class='tip'>单位分钟；保存后设备端按新时长重置当前阶段</div>
<form action='/pomo' method='post'>
<div class='row'>
<input id='pFocus' name='pFocus' type='number' min='1' max='120' placeholder='专注分钟'>
<input id='pShort' name='pShort' type='number' min='1' max='60' placeholder='短休分钟'>
</div>
<div class='row'>
<input id='pLong' name='pLong' type='number' min='1' max='60' placeholder='长休分钟'>
<input id='pCycle' name='pCycle' type='number' min='1' max='12' placeholder='几个专注进长休'>
</div>
<button type='submit'>保存番茄钟设置</button>
</form>

<h3>闹钟</h3>
<div class='tip'>最多 4 组；到点响 60 秒，按任意键停止；"仅一次"的响过自动失效</div>
<form action='/alarm' method='post'>
<div class='row' style='align-items:center'>
<input type='checkbox' id='on0' name='on0'><input type='time' id='t0' name='t0' style='flex:1'>
<select id='rep0' name='rep0' style='flex:0 0 92px'><option value='d'>每天</option><option value='o'>仅一次</option></select>
</div>
<div class='row' style='align-items:center'>
<input type='checkbox' id='on1' name='on1'><input type='time' id='t1' name='t1' style='flex:1'>
<select id='rep1' name='rep1' style='flex:0 0 92px'><option value='d'>每天</option><option value='o'>仅一次</option></select>
</div>
<div class='row' style='align-items:center'>
<input type='checkbox' id='on2' name='on2'><input type='time' id='t2' name='t2' style='flex:1'>
<select id='rep2' name='rep2' style='flex:0 0 92px'><option value='d'>每天</option><option value='o'>仅一次</option></select>
</div>
<div class='row' style='align-items:center'>
<input type='checkbox' id='on3' name='on3'><input type='time' id='t3' name='t3' style='flex:1'>
<select id='rep3' name='rep3' style='flex:0 0 92px'><option value='d'>每天</option><option value='o'>仅一次</option></select>
</div>
<button type='submit'>保存闹钟</button>
</form>
<div class='tip' id='acur'></div>

<h3>局域网备忘录</h3>
<div class='tip'>同一 WiFi 可直接推送；最多 10 条，单条上限 __MAXB__ 字节</div>
<form action='/memo' method='post' onsubmit='return chk()'>
<textarea id='text' name='text' placeholder='输入内容，回车可换行'></textarea>
<div class='tip' id='cnt'>已用 0/__MAXB__ 字节</div>
<div class='tip' id='warn' style='color:#F80000;display:none'>已超出 __MAXB__ 字节上限，请删减后再提交</div>
<button type='submit' id='send'>推送到设备</button>
</form>
<form action='/memo/clear' method='post' onsubmit="return confirm('确定清空全部备忘录？只删除备忘录内容，不影响闹钟、番茄钟、天气与巴法云设置。');">
<button type='submit' class='danger'>清空全部备忘录</button>
</form>
<div class='tip'>同网段任何人都可写入，公共网络勿用</div>
<script>
var ta=document.getElementById('text'),cnt=document.getElementById('cnt'),warn=document.getElementById('warn'),btn=document.getElementById('send');
var LIM=__MAXB__;
function bytes(){ try{ return new TextEncoder().encode(ta.value).length; }catch(e){ return ta.value.length; } }
function upd(){
  var n=bytes(), over=n>LIM;
  cnt.textContent='已用 '+n+'/'+LIM+' 字节';
  cnt.style.color=over?'#F80000':'#888';
  warn.style.display=over?'block':'none';
  btn.disabled=over;
  btn.style.background=over?'#bbb':'#1E90FF';
}
function chk(){ upd(); return bytes()<=LIM; }
ta.addEventListener('input',upd);
upd();
var qs=new URLSearchParams(location.search),mm=qs.get('m');
if(mm){
  var box=document.getElementById('msg'),txt='',ok=(mm==='ok'||mm==='cleared');
  if(mm==='ok') txt='已推送到设备，屏幕已切到备忘录页';
  else if(mm==='cleared') txt='已清空全部备忘录';
  else if(mm==='empty') txt='内容不能为空，未推送';
  else if(mm==='long') txt='内容超长：'+(qs.get('n')||'?')+'/'+LIM+' 字节，请删减后再提交';
  box.textContent=txt;
  box.className='msg '+(ok?'ok':'err');
  box.style.display=txt?'block':'none';
}
</script>

<div class='tip' id='ver' style='text-align:center;margin-top:14px'></div>

<script>
fetch('/status').then(function(r){return r.json()}).then(function(s){
  document.getElementById('ver').textContent='maoshu_Calendar 固件 '+s.ver+' · 设备 '+s.id;
  document.getElementById('pFocus').value=s.pFocus;
  document.getElementById('pShort').value=s.pShort;
  document.getElementById('pLong').value=s.pLong;
  document.getElementById('pCycle').value=s.pCycle;
  if(s.alarm){
    var st='';
    for(var i=0;i<4;i++){
      var a=s.alarm[i];
      document.getElementById('on'+i).checked=a.on;
      document.getElementById('t'+i).value=a.t;
      document.getElementById('rep'+i).value=a.once?'o':'d';
      if(a.on) st+=(st?'，':'')+a.t+(a.once?(a.fired?'(已响过，应用后重新生效)':'(仅一次)'):'(每天)');
    }
    document.getElementById('acur').textContent=st?'当前已启用：'+st:'当前没有启用的闹钟';
  }
}).catch(function(){});
</script>
</body></html>
)rawliteral";

// 扫描周围WiFi，返回JSON(按信号强度降序，同名去重留最强)
static void handleScan(){
  int n = WiFi.scanNetworks();      // 同步扫描，约2秒
  String json = "[";
  int cnt = 0;
  for(int i = 0; i < n && cnt < 20; i++){
    String ssid = WiFi.SSID(i);
    if(!ssid.length()) continue;    // 跳过隐藏网络
    ssid.replace("\\", "\\\\");     // JSON转义
    ssid.replace("\"", "\\\"");
    if(json.indexOf("\"" + ssid + "\"") >= 0) continue;  // 同名去重
    if(cnt) json += ",";
    json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    cnt++;
  }
  json += "]";
  WiFi.scanDelete();
  server->send(200, "application/json", json);
}

// JSON字符串转义
static String jsonEsc(const String& v){
  String s = v;
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  return s;
}

// 当前网络/配置状态
static void handleStatus(){
  bool online = (WiFi.status() == WL_CONNECTED);
  String s = String("{\"mode\":\"") + (portalMode ? "ap" : "sta") + "\"";
  s += ",\"ssid\":\"" + jsonEsc(online ? WiFi.SSID() : String("")) + "\"";
  s += ",\"ip\":\"" + (online ? WiFi.localIP().toString() : String("")) + "\"";
  s += ",\"city\":\"" + jsonEsc(cfgWeatherCity()) + "\"";
  s += ",\"lat\":\"" + cfgWeatherLat() + "\",\"lon\":\"" + cfgWeatherLon() + "\"";
  s += ",\"uid\":\"" + jsonEsc(cfgBemfaUid()) + "\",\"topic\":\"" + jsonEsc(cfgBemfaTopic()) + "\"";
  s += ",\"pFocus\":" + String(cfgPomoFocus()) + ",\"pShort\":" + String(cfgPomoShort());
  s += ",\"pLong\":" + String(cfgPomoLong()) + ",\"pCycle\":" + String(cfgPomoCycle());
  s += ",\"alarm\":" + alarmJson();
  s += ",\"id\":\"" + cfgDeviceId() + "\"";
  s += ",\"ver\":\"" FW_VERSION "\"}";
  server->send(200, "application/json", s);
}

// 校验经纬度文本
static bool coordOk(const String& v, double lim){
  if(!v.length()) return false;
  char* end = nullptr;
  double d = strtod(v.c_str(), &end);
  if(end == v.c_str() || *end != 0) return false;
  return d >= -lim && d <= lim;
}

// 保存天气地区
static void handleWeather(){
  String lat = server->arg("lat");   lat.trim();
  String lon = server->arg("lon");   lon.trim();
  String city = server->arg("city"); city.trim();
  if(!coordOk(lat, 90) || !coordOk(lon, 180)){
    server->send(200, "text/html", "<meta charset='utf-8'>经纬度格式不正确，请重新填写<br><a href='/sys'>返回系统设置</a>");
    return;
  }
  if(!city.length()) city = lat + "," + lon;
  settingsSetWeather(lat, lon, city);
  weatherRefreshNow();
  Serial.println("地区已更新: " + city + " (" + lat + "," + lon + ")");
  server->send(200, "text/html", "<meta charset='utf-8'>地区已保存：" + city + " (" + lat + ", " + lon + ")<br>天气将在几秒内刷新<br><a href='/sys'>返回系统设置</a>");
}

// 保存巴法云UID/主题并重连
static void handleBemfa(){
  String uid = server->arg("uid");     uid.trim();
  String topic = server->arg("topic"); topic.trim();
  settingsSetBemfa(uid, topic);
  memoReloadConfig();
  bool ok = uid.length() && topic.length();
  Serial.println(ok ? ("巴法云配置已更新: " + topic) : String("巴法云配置已清空"));
  server->send(200, "text/html", String("<meta charset='utf-8'>") +
    (ok ? "已保存，正在重新连接巴法云..." : "已清空巴法云配置，推送功能暂停") +
    "<br><a href='/sys'>返回系统设置</a>");
}

// 303 回配置页并带提示参数(m=ok/empty/long/cleared)，不再使用独立提示页
static void redirectHome(const String& query){
  server->sendHeader("Location", String("/") + query, true);
  server->send(303, "text/plain", "");
}

// 局域网备忘录：POST 提交内容，与巴法云消息共用入队/存储路径
static void handleMemoPost(){
  String t = server->arg("text");   // 表单为 UTF-8 urlencoded，arg() 还原为原始 UTF-8 字节
  t.trim();
  if(!t.length()){
    redirectHome("?m=empty");
    return;
  }
  if(t.length() > MEMO_MAX_BYTES){   // 后端兜底：不静默截断，直接拒绝
    redirectHome("?m=long&n=" + String(t.length()));
    return;
  }
  memoAddText(t, false);            // false = 本机提交，不做巴法云保留消息去重
  Serial.println("局域网推送备忘录, " + String(t.length()) + " 字节");
  redirectHome("?m=ok");
}

// 局域网备忘录：清空全部条目(仅删除 memo 命名空间，不动闹钟/番茄钟/天气/巴法云设置)
static void handleMemoClear(){
  memoClearAll();
  Serial.println("局域网请求清空备忘录");
  redirectHome("?m=cleared");
}

// 保存番茄钟时长(分钟)并重置当前阶段
static void handlePomo(){
  int focus = server->arg("pFocus").toInt();
  int shortMin = server->arg("pShort").toInt();
  int longMin = server->arg("pLong").toInt();
  int cycle = server->arg("pCycle").toInt();
  if(focus < 1 || shortMin < 1 || longMin < 1 || cycle < 1){
    server->send(200, "text/html", "<meta charset='utf-8'>请填写大于0的数值<br><a href='/'>返回常用设置</a>");
    return;
  }
  settingsSetPomo(focus, shortMin, longMin, cycle);
  pomoApplyConfig();
  Serial.println("番茄钟设置: 专注" + String(focus) + " 短休" + String(shortMin) +
                 " 长休" + String(longMin) + " 每" + String(cycle) + "个进长休");
  server->send(200, "text/html", "<meta charset='utf-8'>番茄钟设置已保存<br><a href='/'>返回常用设置</a>");
}

// 保存 4 组闹钟(启用/时间/每天或仅一次)
static void handleAlarm(){
  int enabled = 0;
  for(int i = 0; i < ALARM_MAX; i++){
    String sfx = String(i);
    bool on = server->arg("on" + sfx).length() > 0;
    String tv = server->arg("t" + sfx); tv.trim();
    int hh = 0, mm = 0;
    int colon = tv.indexOf(':');
    if(colon > 0){
      hh = tv.substring(0, colon).toInt();
      mm = tv.substring(colon + 1).toInt();
    }
    if(hh < 0 || hh > 23 || mm < 0 || mm > 59) on = false;   // 时间不合法即视为停用
    bool once = (server->arg("rep" + sfx) == "o");
    alarmSet(i, on, hh, mm, once);
    if(on) enabled++;
  }
  Serial.println(String("闹钟设置已保存，启用 ") + enabled + " 组");
  server->send(200, "text/html", String("<meta charset='utf-8'>闹钟已保存(") + enabled +
               " 组启用)<br><a href='/'>返回常用设置</a>");
}

// 保存新凭证并重启
static void handleSave(){
  String ssid = server->arg("ssid");
  String pass = server->arg("pass");
  if(ssid.length()){
    netPrefs.begin("net", false);
    netPrefs.putString("ssid", ssid);
    netPrefs.putString("pass", pass);
    netPrefs.end();
    server->send(200, "text/html", "<meta charset='utf-8'>已保存，设备重启中...");
    delay(1000);
    ESP.restart();
  } else {
    server->send(200, "text/html", "<meta charset='utf-8'>WiFi名称不能为空");
  }
}

static void registerRoutes(){
  server->on("/", [](){
    String html = CONFIG_HTML;
    html.replace("__MAXB__", String(MEMO_MAX_BYTES));
    server->send(200, "text/html", html);
  });
  server->on("/sys", [](){ server->send(200, "text/html", SYS_HTML); });
  server->on("/css", [](){ server->send(200, "text/css", PAGE_CSS); });
  server->on("/scan", handleScan);
  server->on("/status", handleStatus);
  server->on("/save", HTTP_POST, handleSave);
  server->on("/weather", HTTP_POST, handleWeather);
  server->on("/pomo", HTTP_POST, handlePomo);
  server->on("/alarm", HTTP_POST, handleAlarm);
  server->on("/bemfa", HTTP_POST, handleBemfa);
  server->on("/memo", HTTP_GET, [](){   // 旧地址统一回到第一页，表单只在 / 维护一份
    server->sendHeader("Location", "/", true);
    server->send(302, "text/plain", "");
  });
  server->on("/memo", HTTP_POST, handleMemoPost);
  server->on("/memo/clear", HTTP_POST, handleMemoClear);
}

static void startPortal(){
  portalMode = true;
  WiFi.mode(WIFI_AP_STA);  // AP+STA双模：STA用于扫描周围WiFi
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println(String("进入配网模式，热点: ") + AP_SSID);
  drawPortalScreen();
  server = new WebServer(80);
  registerRoutes();
  server->begin();
}

static void tryConnect(const String& ssid, const String& pass, int seconds){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("连接WiFi: " + ssid);
  int n = 0;
  while(WiFi.status() != WL_CONNECTED && n < seconds * 2){
    delay(500);
    n++;
    Serial.print(".");
  }
  Serial.println();
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi连接成功, IP: " + WiFi.localIP().toString());
    configTime(8 * 3600, 0, NTP1, NTP2, NTP3);   // NTP校时(UTC+8)
    setenv("TZ", TZ_INFO, 1);
    tzset();
    // 常驻配置服务器：浏览器访问设备IP可查看/更换网络
    server = new WebServer(80);
    registerRoutes();
    server->begin();
  }
}

void netBegin(){
  netPrefs.begin("net", true);
  String ssid = netPrefs.getString("ssid", "");
  String pass = netPrefs.getString("pass", "");
  netPrefs.end();
  if(ssid.length()){
    tryConnect(ssid, pass, 20);          // 首次最多等20秒(DHCP慢时不至于误判)
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("连接超时，重试一次...");
      delay(500);
      tryConnect(ssid, pass, 10);        // 再等10秒，仍失败才进配网
    }
    if(WiFi.status() == WL_CONNECTED) return;
  }
  startPortal();  // 无凭证或连接失败 → 配网模式
}

void netLoop(){
  if(server) server->handleClient();
  if(portalMode){
    // 配网期间后台STA若连上，自动退出配网，恢复正常运行(不必重启)
    if(WiFi.status() == WL_CONNECTED){
      portalMode = false;
      WiFi.softAPdisconnect(true);
      Serial.println("WiFi已连上，退出配网模式, IP: " + WiFi.localIP().toString());
      configTime(8 * 3600, 0, NTP1, NTP2, NTP3);
      setenv("TZ", TZ_INFO, 1);
      tzset();
      calBootInfoShow();   // 显示启动信息页(版本+新IP)，停留10秒
    }
    return;
  }
  static unsigned long lastCheck = 0;
  if(millis() - lastCheck > 30000){
    lastCheck = millis();
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi断线，重连中...");
      WiFi.reconnect();
    }
  }
}

bool netConnected(){
  return !portalMode && WiFi.status() == WL_CONNECTED;
}

String netDiag(){
  if(portalMode) return String("配网热点:") + AP_SSID;
  if(WiFi.status() == WL_CONNECTED) return "IP:" + WiFi.localIP().toString();
  return "WiFi未连接";
}

bool inPortalMode(){
  return portalMode;
}
