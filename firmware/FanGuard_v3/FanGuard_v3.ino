#include <Types_of_Faults_in_Fan_inferencing.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ── WiFi ─────────────────────────────────────────────────────
const char* WIFI_SSID = "GalaxyA17";
const char* WIFI_PASS = "Roshni123";

// ── Pins ─────────────────────────────────────────────────────
#define BUZZER_PIN  3
#define LED_PIN     2

// ── Thresholds ────────────────────────────────────────────────
#define CONFIRM_COUNT    3
#define FAULT_THRESHOLD  0.7f

// ── Objects ──────────────────────────────────────────────────
Adafruit_MPU6050 mpu;
WebServer server(80);

// ── Inference buffer ──────────────────────────────────────────
#define SAMPLE_COUNT     EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE
#define SAMPLE_RATE_HZ   100
#define SAMPLE_INTERVAL  (1000 / SAMPLE_RATE_HZ)
float ei_buffer[SAMPLE_COUNT];
int   buf_idx = 0;
unsigned long lastSampleTime = 0;

// ── State ─────────────────────────────────────────────────────
struct FanState {
  String label      = "normal";
  String severity   = "INFO";
  float  confidence = 0.0f;
  float  vrms       = 0.0f;
  float  ax = 0, ay = 0, az = 0;
  bool   isFault    = false;
  int    confirmCnt = 0;
  unsigned long inferenceCount = 0;
  unsigned long uptime         = 0;
  float  scores[5] = {0,0,0,0,0};
  String log[8];
  int    logHead = 0;
} S;

const char* EI_LABELS[5]      = {"normal","blade_imbalance","bearing_wear","motor_overload","loose_blade"};
const char* LABEL_NAMES[5]    = {"NORMAL","BLADE IMBALANCE","BEARING WEAR","MOTOR OVERLOAD","LOOSE BLADE"};
const char* CLASS_SEVERITY[5] = {"INFO","WARNING","WARNING","CRITICAL","WARNING"};

void addLog(String msg) { S.log[S.logHead % 8] = msg; S.logHead++; }

String uptimeStr() {
  unsigned long s = S.uptime;
  char buf[12];
  sprintf(buf, "%02lu:%02lu:%02lu", s/3600, (s%3600)/60, s%60);
  return String(buf);
}

// ── /data JSON ────────────────────────────────────────────────
void handleData() {
  StaticJsonDocument<512> doc;
  int ci = 0;
  for (int i = 0; i < 5; i++) { if (S.label == EI_LABELS[i]) { ci = i; break; } }

  doc["class_index"] = ci;
  doc["confidence"]  = S.confidence * 100.0f;
  doc["vrms"]        = S.vrms;
  doc["ax"]          = S.ax;
  doc["ay"]          = S.ay;
  doc["count"]       = S.inferenceCount;
  doc["uptime"]      = S.uptime;
  doc["severity"]    = S.severity;

  JsonArray sc = doc.createNestedArray("scores");
  for (int i = 0; i < 5; i++) sc.add(S.scores[i]);

  JsonArray lg = doc.createNestedArray("log");
  int cnt = min(S.logHead, 8);
  for (int i = cnt - 1; i >= 0; i--) lg.add(S.log[(S.logHead - 1 - i + 8) % 8]);

  String json;
  serializeJson(doc, json);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", json);
}

// ── / HTML — chunked so it fits in RAM ───────────────────────
void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");

  // chunk 1 — head + styles
  server.sendContent(F("<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>FanGuard</title><style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{background:#06090f;color:#b8cce0;font-family:'Courier New',monospace;min-height:100vh}"
    "body::before{content:'';position:fixed;inset:0;"
    "background-image:linear-gradient(rgba(0,220,255,.025)1px,transparent 1px),"
    "linear-gradient(90deg,rgba(0,220,255,.025)1px,transparent 1px);"
    "background-size:36px 36px;pointer-events:none;z-index:0}"
    "header{position:sticky;top:0;z-index:10;display:flex;align-items:center;"
    "justify-content:space-between;padding:14px 24px;border-bottom:1px solid #111e2e;"
    "background:rgba(6,9,15,.97)}"
    ".logo{font-size:1.3rem;font-weight:900;letter-spacing:.14em;color:#00dcff}"
    ".logo span{color:#fff;opacity:.3;font-weight:300}"
    ".sub{font-size:.58rem;color:#2a4060;margin-top:2px;letter-spacing:.1em}"
    ".hlive{display:flex;align-items:center;gap:6px;font-size:.62rem;color:#2a4060}"
    ".dot{width:6px;height:6px;border-radius:50%;background:#00d48a;"
    "box-shadow:0 0 5px #00d48a;animation:blink 1.4s ease-in-out infinite}"
    "@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}"
    ".page{max-width:880px;margin:0 auto;padding:18px 14px;position:relative;z-index:1;display:grid;gap:13px}"
    ".card{background:#0a0f18;border:1px solid #111e2e;border-radius:8px;padding:16px;"
    "position:relative;overflow:hidden;transition:border-color .4s,background .4s}"
    ".ctl{position:absolute;top:0;left:0;right:0;height:2px;transition:background .4s}"
    ".clabel{font-size:.56rem;letter-spacing:.15em;text-transform:uppercase;color:#2a4060;margin-bottom:10px}"
    ".hero{display:grid;grid-template-columns:1fr 118px;align-items:center;gap:18px}"
    ".pbadge{display:inline-flex;padding:4px 12px;border-radius:4px;margin-bottom:10px;"
    "font-size:.58rem;letter-spacing:.12em;font-weight:700;border:1px solid currentColor}"
    ".ptitle{font-size:2.4rem;font-weight:900;letter-spacing:.04em;text-transform:uppercase;"
    "line-height:1;transition:color .4s}"
    ".pdesc{font-size:.68rem;color:#3a5570;margin-top:8px;line-height:1.5;max-width:360px}"
    ".crow{display:flex;align-items:center;gap:10px;margin-top:11px}"
    ".cbar{flex:1;height:4px;background:#111e2e;border-radius:2px;overflow:hidden}"
    ".cfill{height:100%;border-radius:2px;transition:width .6s ease,background .4s}"
    ".cval{font-size:.72rem;font-family:monospace;min-width:44px;text-align:right;transition:color .4s}"
  ));

  // chunk 2 — more styles + fan svg styles
  server.sendContent(F(
    "#blades{transform-origin:60px 60px;transform-box:fill-box;"
    "animation:spin 1s linear infinite;animation-play-state:running}"
    "@keyframes spin{to{transform:rotate(360deg)}}"
    "#fring{opacity:0;transition:opacity .5s}"
    ".sgrid{display:grid;gap:7px}"
    ".sr{display:grid;grid-template-columns:138px 1fr 42px;align-items:center;gap:9px}"
    ".sn{font-size:.62rem;color:#2a4060;transition:color .3s}"
    ".sn.act{color:#b8cce0;font-weight:700}"
    ".sbg{height:4px;background:#111e2e;border-radius:2px;overflow:hidden}"
    ".sf{height:100%;border-radius:2px;transition:width .6s ease}"
    ".sp{font-size:.62rem;font-family:monospace;text-align:right;color:#2a4060;transition:color .3s}"
    ".sp.act{font-weight:700}"
    ".mgrid{display:grid;grid-template-columns:repeat(4,1fr);gap:9px}"
    ".mc{background:#080c14;border:1px solid #111e2e;border-radius:6px;padding:11px}"
    ".ml{font-size:.52rem;letter-spacing:.12em;text-transform:uppercase;color:#2a4060;margin-bottom:4px}"
    ".mv{font-size:1.35rem;font-weight:700;font-family:monospace;color:#00dcff}"
    ".mu{font-size:.55rem;color:#2a4060}"
    ".lb{font-size:.62rem;line-height:1.9;font-family:monospace}"
    ".ll{display:flex;gap:7px;border-bottom:1px solid rgba(255,255,255,.03);padding:1px 0}"
    ".lts{color:#2a4060;min-width:64px}"
    ".lok{color:#00d48a}.lerr{color:#ff0000}.lwarn{color:#ffb830}.linf{color:#00dcff}"
    ".si{color:#00dcff;border-color:#00dcff;background:rgba(0,220,255,.07)}"
    ".sw{color:#ffb830;border-color:#ffb830;background:rgba(255,184,48,.07)}"
    ".sc{color:#ff0000;border-color:#ff0000;background:rgba(255,0,0,.09)}"
    "footer{text-align:center;font-size:.52rem;color:#1a2e40;"
    "padding:13px;border-top:1px solid #111e2e;letter-spacing:.08em}"
    "</style></head><body>"
  ));

  // chunk 3 — header + page open + hero card
  server.sendContent(F(
    "<header>"
    "<div><div class='logo'>FAN<span>/</span>GUARD</div>"
    "<div class='sub'>ESP32-C3 &middot; MPU6050 &middot; EDGE IMPULSE &middot; 5-CLASS</div></div>"
    "<div class='hlive'><div class='dot'></div>LIVE&nbsp;<span id='hu'>--:--:--</span></div>"
    "</header>"
    "<div class='page'>"

    "<div class='card' id='hc'><div class='ctl' id='tl'></div>"
    "<div class='clabel'>Diagnostic Result</div>"
    "<div class='hero'><div>"
    "<div class='pbadge si' id='sb'>INFO</div>"
    "<div class='ptitle' id='pt'>STARTING</div>"
    "<div class='pdesc' id='pd'>Initializing...</div>"
    "<div class='crow'><div class='cbar'><div class='cfill' id='cf' style='width:0%'></div></div>"
    "<div class='cval' id='cv'>--%</div></div>"
    "</div>"

    "<svg width='118' height='118' viewBox='0 0 120 120'>"
    "<circle cx='60' cy='60' r='56' fill='none' stroke='#111e2e' stroke-width='1.5'/>"
    "<circle id='fring' cx='60' cy='60' r='56' fill='none' stroke='#ff0000' stroke-width='2.5'/>"
    "<g id='blades'>"
    "<path d='M60 60 C53 44 42 34 51 21 C58 12 72 16 70 31 C68 43 65 53 60 60Z' fill='#0f1e32' stroke='#1a3050' stroke-width='0.8'/>"
    "<path d='M60 60 C76 53 87 43 98 51 C107 58 103 72 88 70 C76 68 67 65 60 60Z' fill='#0f1e32' stroke='#1a3050' stroke-width='0.8'/>"
    "<path d='M60 60 C67 76 67 88 78 96 C87 104 99 95 93 80 C88 68 77 65 60 60Z' fill='#0f1e32' stroke='#1a3050' stroke-width='0.8'/>"
    "<path d='M60 60 C44 67 34 78 21 69 C12 62 16 48 31 50 C43 52 53 56 60 60Z' fill='#0f1e32' stroke='#1a3050' stroke-width='0.8'/>"
    "<path d='M60 60 C53 44 40 42 32 31 C24 20 35 10 46 19 C56 27 58 46 60 60Z' fill='#0d1b2e' stroke='#1a3050' stroke-width='0.8'/>"
    "</g>"
    "<circle cx='60' cy='60' r='9' fill='#060c14' stroke='#1a3050' stroke-width='1.5'/>"
    "<circle cx='60' cy='60' r='3.5' fill='#00dcff' opacity='.65'/>"
    "<g stroke='#111e2e' stroke-width='1'>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(0 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(45 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(90 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(135 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(180 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(225 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(270 60 60)'/>"
    "<line x1='60' y1='2' x2='60' y2='9' transform='rotate(315 60 60)'/>"
    "</g></svg></div></div>"
  ));

  // chunk 4 — scores card + metrics + log card
  server.sendContent(F(
    "<div class='card'><div class='clabel'>Class Confidence Scores</div>"
    "<div class='sgrid'>"
    "<div class='sr'><div class='sn act' id='sn0'>NORMAL</div>"
    "<div class='sbg'><div class='sf' id='sb0' style='width:0%;background:#00d48a'></div></div>"
    "<div class='sp' id='sp0'>--%</div></div>"
    "<div class='sr'><div class='sn' id='sn1'>BLADE IMBALANCE</div>"
    "<div class='sbg'><div class='sf' id='sb1' style='width:0%;background:#ffb830'></div></div>"
    "<div class='sp' id='sp1'>--%</div></div>"
    "<div class='sr'><div class='sn' id='sn2'>BEARING WEAR</div>"
    "<div class='sbg'><div class='sf' id='sb2' style='width:0%;background:#ff4060'></div></div>"
    "<div class='sp' id='sp2'>--%</div></div>"
    "<div class='sr'><div class='sn' id='sn3'>MOTOR OVERLOAD</div>"
    "<div class='sbg'><div class='sf' id='sb3' style='width:0%;background:#ff0000'></div></div>"
    "<div class='sp' id='sp3'>--%</div></div>"
    "<div class='sr'><div class='sn' id='sn4'>LOOSE BLADE</div>"
    "<div class='sbg'><div class='sf' id='sb4' style='width:0%;background:#ff8c00'></div></div>"
    "<div class='sp' id='sp4'>--%</div></div>"
    "</div></div>"

    "<div class='mgrid'>"
    "<div class='mc'><div class='ml'>Vrms</div><div class='mv' id='mv'>--</div><div class='mu'>Volts RMS</div></div>"
    "<div class='mc'><div class='ml'>Accel X</div><div class='mv' id='max'>--</div><div class='mu'>m/s&sup2;</div></div>"
    "<div class='mc'><div class='ml'>Accel Y</div><div class='mv' id='may'>--</div><div class='mu'>m/s&sup2;</div></div>"
    "<div class='mc'><div class='ml'>Inferences</div><div class='mv' id='mc'>--</div><div class='mu'>total runs</div></div>"
    "</div>"

    "<div class='card'><div class='clabel'>Event Log</div>"
    "<div class='lb' id='lg'>"
    "<div class='ll'><span class='lts'>--:--:--</span><span class='linf'>Waiting...</span></div>"
    "</div></div>"
    "</div>"
    "<footer>FANGUARD v3 &middot; 5-CLASS EDGE ML &middot; POLLS /data EVERY 1s &middot; NO CLOUD</footer>"
  ));

  // chunk 5 — JavaScript
  server.sendContent(F(
    "<script>"
    "const C=['#00d48a','#ffb830','#ff4060','#ff0000','#ff8c00'];"
    "const L=['NORMAL','BLADE IMBALANCE','BEARING WEAR','MOTOR OVERLOAD','LOOSE BLADE'];"
    "const D=["
    "'All systems nominal. Fan operating within normal parameters.',"
    "'Periodic vibration detected. One blade may be unbalanced or have debris.',"
    "'Irregular impact spikes detected. Bearing shows signs of wear.',"
    "'High sustained vibration. Fan may be blocked or drawing excess current.',"
    "'Intermittent burst vibration. A blade may be loose or not secured.'"
    "];"
    "const SV=['INFO','WARNING','WARNING','CRITICAL','WARNING'];"
    "const bl=document.getElementById('blades');"
    "const fr=document.getElementById('fring');"
    "function hr(h){"
    "return parseInt(h.slice(1,3),16)+','+parseInt(h.slice(3,5),16)+','+parseInt(h.slice(5,7),16)}"
    "function fu(s){"
    "return String(Math.floor(s/3600)).padStart(2,'0')+':'"
    "+String(Math.floor((s%3600)/60)).padStart(2,'0')+':'"
    "+String(s%60).padStart(2,'0')}"
    "async function poll(){"
    "try{"
    "const r=await fetch('/data');"
    "const d=await r.json();"
    "const ci=d.class_index,f=ci!==0,col=C[ci],sv=SV[ci];"
    "bl.style.animationPlayState=f?'paused':'running';"
    "fr.style.opacity=f?'0.85':'0';"
    "fr.setAttribute('stroke',col);"
    "const hc=document.getElementById('hc');"
    "hc.style.borderColor=f?col:'#111e2e';"
    "hc.style.background=f?'rgba('+hr(col)+',.04)':'#0a0f18';"
    "document.getElementById('tl').style.background='linear-gradient(90deg,'+col+',transparent)';"
    "document.getElementById('pt').textContent=L[ci];"
    "document.getElementById('pt').style.color=col;"
    "document.getElementById('pd').textContent=D[ci];"
    "document.getElementById('cf').style.width=d.confidence.toFixed(1)+'%';"
    "document.getElementById('cf').style.background=col;"
    "document.getElementById('cv').textContent=d.confidence.toFixed(1)+'%';"
    "document.getElementById('cv').style.color=col;"
    "const sbEl=document.getElementById('sb');"
    "sbEl.textContent=sv;"
    "sbEl.className='pbadge '+(sv==='INFO'?'si':sv==='WARNING'?'sw':'sc');"
    "for(let i=0;i<5;i++){"
    "const p=(d.scores[i]*100).toFixed(1);"
    "document.getElementById('sb'+i).style.width=p+'%';"
    "document.getElementById('sp'+i).textContent=p+'%';"
    "const a=i===ci;"
    "document.getElementById('sn'+i).className='sn'+(a?' act':'');"
    "document.getElementById('sp'+i).className='sp'+(a?' act':'');"
    "if(a){document.getElementById('sp'+i).style.color=col;"
    "document.getElementById('sn'+i).style.color='#b8cce0';}"
    "else{document.getElementById('sp'+i).style.color='';"
    "document.getElementById('sn'+i).style.color='';}"
    "}"
    "document.getElementById('mv').textContent=d.vrms.toFixed(4);"
    "document.getElementById('max').textContent=d.ax.toFixed(3);"
    "document.getElementById('may').textContent=d.ay.toFixed(3);"
    "document.getElementById('mc').textContent=d.count;"
    "document.getElementById('hu').textContent=fu(d.uptime);"
    "if(d.log&&d.log.length){"
    "document.getElementById('lg').innerHTML=d.log.map(e=>{"
    "const t=e.substring(0,8),m=e.substring(9);"
    "let c='linf';"
    "if(m.includes('OVERLOAD'))c='lerr';"
    "else if(m.includes('WEAR')||m.includes('IMBALANCE')||m.includes('LOOSE'))c='lwarn';"
    "else if(m.includes('NORMAL'))c='lok';"
    "return '<div class=\\'ll\\'><span class=\\'lts\\'>'+t+'</span><span class=\\''+c+'\\'>'+m+'</span></div>';"
    "}).join('');}"
    "}catch(e){console.error('poll err:',e);}"
    "}"
    "poll();"
    "setInterval(poll,1000);"
    "</script></body></html>"
  ));

  server.sendContent("");  // end chunked
}

void handleNotFound() { server.send(404, "text/plain", "Not found"); }

// ── Inference ─────────────────────────────────────────────────
void runInference() {
  signal_t signal;
  numpy::signal_from_buffer(ei_buffer, SAMPLE_COUNT, &signal);
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) { Serial.printf("[EI] Err:%d\n", err); return; }

  float bestVal = 0; int bestIdx = 0;
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT && i < 5; i++) {
    S.scores[i] = result.classification[i].value;
    if (result.classification[i].value > bestVal) {
      bestVal = result.classification[i].value; bestIdx = i;
    }
  }
  S.inferenceCount++;

  if (bestIdx != 0 && bestVal >= FAULT_THRESHOLD) {
    S.confirmCnt++;
    if (S.confirmCnt >= CONFIRM_COUNT) {
      S.label = EI_LABELS[bestIdx]; S.confidence = bestVal;
      S.severity = CLASS_SEVERITY[bestIdx]; S.isFault = true;
      if (S.severity == String("CRITICAL")) {
        for (int b = 0; b < 3; b++) { digitalWrite(BUZZER_PIN,HIGH); delay(80); digitalWrite(BUZZER_PIN,LOW); delay(80); }
      } else { digitalWrite(BUZZER_PIN,HIGH); delay(200); digitalWrite(BUZZER_PIN,LOW); }
      digitalWrite(LED_PIN, HIGH);
      addLog(uptimeStr()+" "+String(LABEL_NAMES[bestIdx])+" conf:"+String(bestVal*100,0)+"%|"+String(CLASS_SEVERITY[bestIdx]));
    }
  } else {
    if (S.isFault) addLog(uptimeStr()+" NORMAL restored");
    S.confirmCnt=0; S.isFault=false;
    S.label="normal"; S.confidence=S.scores[0]; S.severity="INFO";
    digitalWrite(BUZZER_PIN,LOW); digitalWrite(LED_PIN,LOW);
    static int nt=0;
    if(++nt>=10){ nt=0; addLog(uptimeStr()+" NORMAL vrms:"+String(S.vrms,3)); }
  }
  Serial.printf("[#%lu] %s %.1f%%\n", S.inferenceCount, LABEL_NAMES[bestIdx], bestVal*100);
}

float computeVrms(float ax,float ay,float az){ return sqrt(ax*ax+ay*ay+az*az)*0.1f; }

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200); delay(800);
  Serial.println("\n=== FanGuard v3 ===");

  pinMode(BUZZER_PIN,OUTPUT); pinMode(LED_PIN,OUTPUT);
  digitalWrite(BUZZER_PIN,LOW); digitalWrite(LED_PIN,LOW);
  digitalWrite(BUZZER_PIN,HIGH); delay(100); digitalWrite(BUZZER_PIN,LOW);

  Wire.begin(8,9);
  if (!mpu.begin()) { Serial.println("[ERR] MPU6050 not found!"); while(1) delay(500); }
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("[OK] MPU6050 ready");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WiFi] Connecting");
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); }
  Serial.println("\n[WiFi] Connected!");
  Serial.printf("[WiFi] IP: http://%s\n", WiFi.localIP().toString().c_str());

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.onNotFound(handleNotFound);
  server.begin();

  addLog("00:00:00 Online|"+WiFi.localIP().toString());
  Serial.println("[OK] Web server started\n=== Ready ===");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  server.handleClient();

  static unsigned long lastSec = 0;
  if (millis() - lastSec >= 1000) { S.uptime++; lastSec = millis(); }

  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = now;
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    S.ax = a.acceleration.x; S.ay = a.acceleration.y; S.az = a.acceleration.z;
    S.vrms = computeVrms(S.ax, S.ay, S.az);
    if (buf_idx < SAMPLE_COUNT) ei_buffer[buf_idx++] = S.vrms;
    if (buf_idx >= SAMPLE_COUNT) { runInference(); buf_idx = 0; }
  }
}
