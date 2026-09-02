#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <MQTT.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// KONFIGURASI
const char* AP_SSID     = "Angklung-Robot";
const char* AP_PASSWORD = "UCIC67";
const char* MQTT_SERVER = "broker.hivemq.com";
const char* MQTT_TOPIC  = "muhayara/example";  // + "/0" s/d "/7"

const int ledPins[8] = {13, 14, 18, 19, 21, 22, 23, 25};
bool      ledStatus[8] = {false};

// ── Pin LED RGB (indikator status) ──
const int RGB_R = 26;  // Merah  = booting
const int RGB_G = 27;  // Hijau  = WiFi OK
const int RGB_B = 32;  // Biru   = MQTT OK

WebServer   server(80);
Preferences prefs;
WiFiClient  espClient;
MQTTClient  mqttClient;

String savedSSID="", savedPass="", pendingSSID="", pendingPass="";
bool   wifiConnected=false, pendingConnect=false;
bool   scanReady=false, scanRunning=false;

// RGB HELPER
void rgbSet(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r ? HIGH : LOW);
  digitalWrite(RGB_G, g ? HIGH : LOW);
  digitalWrite(RGB_B, b ? HIGH : LOW);
}
void rgbMerah()  { rgbSet(1, 0, 0); }
void rgbHijau()  { rgbSet(0, 1, 0); }
void rgbBiru()   { rgbSet(0, 0, 1); }
void rgbKuning() { rgbSet(1, 1, 0); }
void rgbMati()   { rgbSet(0, 0, 0); }

// MQTT
void onMessage(String &topic, String &payload) {
  String prefix = String(MQTT_TOPIC) + "/";
  if (topic.startsWith(prefix)) {
    int ch = topic.substring(prefix.length()).toInt();
    if (ch >= 0 && ch < 8) {
      ledStatus[ch] = (payload == "1");
      // ACTIVE LOW relay: LOW = ON, HIGH = OFF
      digitalWrite(ledPins[ch], ledStatus[ch] ? LOW : HIGH);
      String label = (ch == 0) ? "[RELAY]" : "[CH]";
      Serial.println(label + " CH" + String(ch) +
                     " GPIO" + String(ledPins[ch]) +
                     " = " + (ledStatus[ch] ? "ON  ← MOTOR JALAN" : "OFF ← MOTOR BERHENTI"));
    }
  }
}

void reconnectMQTT() {
  if (!mqttClient.connected()) {
    String id = "Angklung-" + String(random(0xffff), HEX);
    Serial.print("[MQTT] Konek... ");
    if (mqttClient.connect(id.c_str())) {
      for (int i = 0; i < 8; i++)
        mqttClient.subscribe(String(MQTT_TOPIC) + "/" + String(i));
      // Clear retained message + pastikan semua relay OFF (active-low = HIGH)
      for (int i = 0; i < 8; i++) {
        mqttClient.publish(String(MQTT_TOPIC) + "/" + String(i), "0", false, 0);
        digitalWrite(ledPins[i], HIGH);  // ACTIVE LOW: HIGH = relay OFF
      }
      rgbBiru();
      Serial.println("OK! Subscribe + clear all CH: " + String(MQTT_TOPIC) + "/0~7");
    } else {
      Serial.println("Gagal.");
    }
  }
}

// CSS PORTAL
const char CSS[] PROGMEM = R"CSS(
*{margin:0;padding:0;box-sizing:border-box}
body{background:#0a0a0a;color:#eee;font-family:'Segoe UI',sans-serif;padding:16px;min-height:100vh}
h1{text-align:center;color:#00e5ff;font-size:22px;letter-spacing:3px;padding:16px 0 4px}
.sub{text-align:center;color:#444;font-size:12px;margin-bottom:16px}
.card{background:#161616;border:1px solid #222;border-radius:14px;padding:14px 16px;margin-bottom:12px}
.card-label{font-size:10px;color:#444;letter-spacing:2px;text-transform:uppercase;margin-bottom:10px}
.status-row{display:flex;justify-content:space-between;align-items:center;font-size:13px}
.btn-forget{background:transparent;color:#ff4444;border:1px solid #ff4444;border-radius:8px;padding:5px 12px;font-size:11px;cursor:pointer}
.section-label{font-size:10px;color:#444;letter-spacing:2px;text-transform:uppercase;margin:16px 0 8px}
.wifi-item{display:flex;align-items:center;justify-content:space-between;padding:13px 14px;border-radius:12px;margin-bottom:6px;cursor:pointer;border:1px solid #1e1e1e;background:#111;transition:background 0.15s}
.wifi-item:hover{background:#1a1a1a;border-color:#333}
.wifi-item.saved{border-color:#00e5ff30;background:#00e5ff06}
.wifi-left{display:flex;align-items:center;gap:12px}
.bars{display:flex;align-items:flex-end;gap:2px;height:16px}
.bar{width:4px;border-radius:1px}
.wifi-info{display:flex;flex-direction:column;gap:3px}
.wifi-name{font-size:14px;color:#ddd}
.badge{font-size:10px;color:#00e5ff;background:#00e5ff18;border-radius:4px;padding:1px 6px;margin-left:6px}
.wifi-meta{font-size:11px;color:#444}
.arrow{color:#333;font-size:22px;line-height:1}
.modal-bg{display:none;position:fixed;inset:0;background:rgba(0,0,0,0.85);z-index:99;align-items:flex-end;justify-content:center}
.modal-bg.show{display:flex}
.modal{background:#181818;border:1px solid #2a2a2a;border-radius:20px 20px 0 0;padding:24px 20px 36px;width:100%;max-width:440px}
.modal h3{font-size:16px;margin-bottom:4px}
.modal-sub{font-size:12px;color:#555;margin-bottom:18px}
label{display:block;font-size:12px;color:#555;margin-bottom:6px;margin-top:14px}
input[type=password]{width:100%;padding:12px 14px;background:#111;border:1px solid #2a2a2a;border-radius:10px;color:white;font-size:15px;outline:none}
input:focus{border-color:#00e5ff}
.btn-ok{width:100%;padding:13px;margin-top:18px;background:#00e5ff;color:#000;font-size:15px;font-weight:bold;border:none;border-radius:12px;cursor:pointer}
.btn-cancel{width:100%;padding:11px;margin-top:8px;background:transparent;color:#555;font-size:13px;border:1px solid #222;border-radius:12px;cursor:pointer}
.alert-ok{padding:12px 16px;border-radius:10px;font-size:13px;margin-bottom:12px;background:rgba(0,200,100,0.08);border:1px solid #00c864;color:#00c864}
.alert-err{padding:12px 16px;border-radius:10px;font-size:13px;margin-bottom:12px;background:rgba(255,68,68,0.08);border:1px solid #ff4444;color:#ff4444}
.btn-scan{width:100%;padding:10px;background:transparent;color:#00e5ff;border:1px solid #00e5ff22;border-radius:10px;font-size:13px;cursor:pointer;margin-bottom:10px}
.btn-scan:disabled{color:#444;border-color:#222;cursor:not-allowed}
.empty{color:#444;text-align:center;font-size:13px;padding:24px}
.scanning{color:#00e5ff88;text-align:center;font-size:13px;padding:20px}
.chip{display:inline-block;padding:3px 10px;border-radius:20px;font-size:11px;font-weight:bold}
.chip-on{background:#00e5ff18;color:#00e5ff;border:1px solid #00e5ff33}
.chip-off{background:#ff444418;color:#ff6666;border:1px solid #ff444433}
.pin-table{width:100%;border-collapse:collapse;font-size:12px;margin-top:8px}
.pin-table td{padding:6px 10px;border:1px solid #222;color:#888}
.pin-table td:first-child{color:#00e5ff;font-weight:bold;width:40px}
)CSS";

// BUILD HTML
String buildWifiList() {
  if (scanRunning) return "<p class='scanning'>&#128268; Scanning... mohon tunggu</p>";
  if (!scanReady)  return "<p class='empty'>Tekan <b>Scan Ulang</b> untuk mencari jaringan WiFi.</p>";
  int n = WiFi.scanComplete();
  if (n <= 0) return "<p class='empty'>Tidak ada jaringan ditemukan.</p>";
  String html = "";
  for (int i = 0; i < n; i++) {
    String ssid = WiFi.SSID(i);
    int rssi    = WiFi.RSSI(i);
    bool locked = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    bool saved  = (ssid == savedSSID);
    if (ssid.length() == 0) continue;
    int bars; String bc;
    if      (rssi > -55) { bars=4; bc="#00e5ff"; }
    else if (rssi > -67) { bars=3; bc="#00e5ff"; }
    else if (rssi > -78) { bars=2; bc="#ffaa00"; }
    else                 { bars=1; bc="#ff4444"; }
    html += "<div class='wifi-item" + String(saved?" saved":"") + "' onclick=\"selectWifi('" + ssid + "')\">";
    html += "<div class='wifi-left'><div class='bars'>";
    for (int b=1;b<=4;b++)
      html += "<div class='bar' style='height:"+String(b*4)+"px;background:"+bc+";opacity:"+(b<=bars?"1":"0.2")+"'></div>";
    html += "</div><div class='wifi-info'><span class='wifi-name'>"+ssid;
    if (saved) html += " <span class='badge'>Aktif</span>";
    html += "</span><span class='wifi-meta'>"+String(rssi)+" dBm &nbsp;"+(locked?"&#128274;":"&#128275;")+"</span>";
    html += "</div></div><span class='arrow'>&#8250;</span></div>";
  }
  return html;
}

String buildPage(String alertClass="", String alertMsg="") {
  String chip = wifiConnected
    ? "<span class='chip chip-on'>&#9679; Online</span>"
    : "<span class='chip chip-off'>&#9679; Offline</span>";
  String wifiStatus = (savedSSID!="")
    ? "Tersimpan: <b style='color:#00e5ff'>"+savedSSID+"</b>"
    : "<span style='color:#555'>Belum ada WiFi tersimpan</span>";

  String h = "<!DOCTYPE html><html lang='id'><head>";
  h += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  h += "<title>Angklung Robot</title><style>"; h += CSS; h += "</style></head><body>";
  h += "<h1>ANGKLUNG ROBOT</h1><p class='sub'>TEST MODE &#127981; LED Breadboard</p>";
  if (alertClass!="") h += "<div class='"+alertClass+"'>"+alertMsg+"</div>";

  h += "<div class='card'><div class='card-label'>Status Koneksi</div>";
  h += "<div class='status-row' style='margin-bottom:10px'><span style='font-size:12px;color:#555'>Internet</span>"+chip+"</div>";
  h += "<div class='status-row'><span>"+wifiStatus+"</span>";
  if (savedSSID!="") h += "<form action='/reset' method='POST' style='display:inline'><button type='submit' class='btn-forget'>Lupakan</button></form>";
  h += "</div></div>";

  // Tabel pin
  const char* noteNames[] = {"Do","Re","Mi","Fa","Sol","La","Si","Do'"};
  int pins[] = {16,17,18,19,23,25,26,27};
  h += "<div class='card'><div class='card-label'>&#128161; Pin LED Nada</div>";
  h += "<table class='pin-table'>";
  for (int i=0;i<8;i++)
    h += "<tr><td>CH"+String(i)+"</td><td>GPIO "+String(pins[i])+"</td><td style='color:#555'>"+noteNames[i]+"</td></tr>";
  h += "</table>";
  h += "<p style='font-size:11px;color:#444;margin-top:10px'>RGB: <b style='color:#ff4444'>R=4</b> &nbsp; <b style='color:#00c864'>G=2</b> &nbsp; <b style='color:#4488ff'>B=32</b></p>";
  h += "</div>";

  h += "<div class='section-label'>Jaringan WiFi</div>";
  String scanLabel = scanRunning ? "Scanning..." : "&#8635; Scan Ulang";
  h += "<form action='/scan' method='POST' style='margin-bottom:10px'>";
  h += "<button type='submit' class='btn-scan'"+String(scanRunning?" disabled":"")+">" + scanLabel + "</button></form>";
  h += buildWifiList();

  h += "<div class='modal-bg' id='modalBg'><div class='modal'>";
  h += "<h3 id='modalTitle'></h3><p class='modal-sub'>Masukkan password untuk terhubung</p>";
  h += "<form action='/simpan' method='POST'>";
  h += "<input type='hidden' name='ssid' id='hiddenSSID'>";
  h += "<label>Password WiFi</label>";
  h += "<input type='password' name='password' placeholder='Kosongkan jika open network' autocomplete='off'>";
  h += "<button type='submit' class='btn-ok'>Hubungkan</button></form>";
  h += "<button class='btn-cancel' onclick='closeModal()'>Batal</button>";
  h += "</div></div>";
  h += "<script>function selectWifi(s){document.getElementById('modalTitle').textContent=s;document.getElementById('hiddenSSID').value=s;document.getElementById('modalBg').classList.add('show');}function closeModal(){document.getElementById('modalBg').classList.remove('show');}document.getElementById('modalBg').addEventListener('click',function(e){if(e.target===this)closeModal();});</script>";
  h += "</body></html>";
  return h;
}

String buildScanningPage() {
  String h="<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='8;url=/'><title>Scanning...</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#0a0a0a;color:#eee;font-family:'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;flex-direction:column;gap:20px;padding:20px;text-align:center}h1{color:#00e5ff;font-size:20px;letter-spacing:3px}.sp{width:48px;height:48px;border:4px solid #222;border-top:4px solid #00e5ff;border-radius:50%;animation:spin .8s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}p{color:#555;font-size:13px}</style></head><body><h1>ANGKLUNG ROBOT</h1><div class='sp'></div><p>Scanning WiFi...<br>Halaman otomatis diperbarui</p></body></html>";
  return h;
}

String buildLoading(String ssid) {
  String h="<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><meta http-equiv='refresh' content='4;url=/status'><title>Menghubungkan...</title><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#0a0a0a;color:#eee;font-family:'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;flex-direction:column;gap:20px;padding:20px;text-align:center}h1{color:#00e5ff;font-size:20px;letter-spacing:3px}.sp{width:48px;height:48px;border:4px solid #222;border-top:4px solid #00e5ff;border-radius:50%;animation:spin .8s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}.nm{color:#fff;font-size:16px;font-weight:bold}p{color:#555;font-size:13px}</style></head><body><h1>ANGKLUNG ROBOT</h1><div class='sp'></div><div class='nm'>"+ssid+"</div><p>Sedang menghubungkan...<br>Halaman otomatis diperbarui</p></body></html>";
  return h;
}

// HANDLER
void handleRoot()  { server.send(200,"text/html",buildPage()); }
void handleScan()  { WiFi.scanDelete(); WiFi.scanNetworks(true); scanRunning=true; scanReady=false; Serial.println("[Scan] Mulai..."); server.send(200,"text/html",buildScanningPage()); }
void handleReset() { prefs.begin("wifi",false); prefs.clear(); prefs.end(); savedSSID=""; savedPass=""; pendingSSID=""; pendingPass=""; wifiConnected=false; WiFi.disconnect(); scanReady=false; scanRunning=false; WiFi.scanDelete(); server.send(200,"text/html",buildPage("alert-ok","WiFi dilupakan!")); Serial.println("[WiFi] Dilupakan."); }

void handleSimpan() {
  String ssid=server.arg("ssid"), pass=server.arg("password");
  if (ssid.isEmpty()){server.send(200,"text/html",buildPage("alert-err","SSID kosong!"));return;}
  prefs.begin("wifi",false); prefs.putString("ssid",ssid); prefs.putString("pass",pass); prefs.end();
  savedSSID=ssid; savedPass=pass; pendingSSID=ssid; pendingPass=pass;
  pendingConnect=true; wifiConnected=false;
  Serial.println("[WiFi] Antri: "+ssid);
  server.send(200,"text/html",buildLoading(ssid));
}

void handleStatus() {
  if (wifiConnected) {
    String h="<!DOCTYPE html><html><head><meta charset='UTF-8'><style>*{margin:0;padding:0;box-sizing:border-box}body{background:#0a0a0a;color:#eee;font-family:'Segoe UI',sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;flex-direction:column;gap:16px;padding:20px;text-align:center}h1{color:#00e5ff;font-size:20px;letter-spacing:3px}.ic{font-size:64px}.mg{color:#00c864;font-size:18px;font-weight:bold}p{color:#555;font-size:13px;line-height:1.7}.btn{display:inline-block;margin-top:8px;padding:12px 28px;background:#00e5ff;color:#000;font-size:14px;font-weight:bold;border-radius:12px;text-decoration:none}</style></head><body><h1>ANGKLUNG ROBOT</h1><div class='ic'>&#9989;</div><div class='mg'>WiFi Terhubung!</div><p>Buka <b>angklung_controller.html</b><br>di HP/laptop yang sama WiFi-nya.<br>Tekan nada → LED nyala!</p><a class='btn' href='/'>Kembali</a></body></html>";
    server.send(200,"text/html",h);
  } else if (pendingConnect) {
    server.sendHeader("Refresh","3;url=/status");
    server.send(200,"text/html",buildLoading(pendingSSID));
  } else {
    server.send(200,"text/html",buildPage("alert-err","Gagal konek ke <b>"+pendingSSID+"</b>. Cek password."));
  }
}

// LOAD WIFI TERSIMPAN
bool loadSavedWifi() {
  prefs.begin("wifi",true);
  savedSSID=prefs.getString("ssid",""); savedPass=prefs.getString("pass","");
  prefs.end();
  if (savedSSID=="") return false;
  Serial.println("[WiFi] Mencoba: "+savedSSID);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());
  for (int i=0;i<20&&WiFi.status()!=WL_CONNECTED;i++){delay(500);Serial.print(".");}
  if (WiFi.status()==WL_CONNECTED){Serial.println("\n[WiFi] OK! IP: "+WiFi.localIP().toString());return true;}
  Serial.println("\n[WiFi] Gagal."); return false;
}

// SETUP
void setup() {
  // ── MATIKAN RELAY SECEPAT MUNGKIN ──
  // Harus sebelum apapun supaya relay tidak sempat nyala saat boot
  // ACTIVE LOW: HIGH = relay OFF
  for (int i = 0; i < 8; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], HIGH);
  }

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  Serial.begin(115200);
  delay(500);
  Serial.println("\n============================");
  Serial.println(" Angklung Robot — TEST MODE");
  Serial.println("============================");
  Serial.println(" RELAY CH0 : GPIO 13 → Motor DC JGA25-370");
  Serial.println(" RGB       : R=26  G=27  B=32");
  Serial.println("============================");
  Serial.println(" ACTIVE LOW relay: LOW=ON, HIGH=OFF");

  // Init semua pin output (CH0~7)
  // ACTIVE HIGH: set LOW dulu = semua relay OFF saat boot
  for (int i = 0; i < 8; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // Init RGB
  pinMode(RGB_R, OUTPUT);
  pinMode(RGB_G, OUTPUT);
  pinMode(RGB_B, OUTPUT);
  rgbMerah();

  // ── TEST BOOT: hanya CH0 (active-low relay) ──
  Serial.println("[BOOT TEST] Relay CH0 ON → Motor jalan...");
  digitalWrite(ledPins[0], LOW);    // Active-LOW: LOW = relay ON = motor jalan
  delay(1000);
  digitalWrite(ledPins[0], HIGH);   // Active-LOW: HIGH = relay OFF = motor berhenti
  Serial.println("[BOOT TEST] Relay CH0 OFF → Motor berhenti. Lanjut.");

  // ── TEST BOOT: CH0 ON 1 detik lalu OFF ──
  Serial.println("[BOOT TEST] GPIO13 LOW → Relay ON → Motor jalan...");
  digitalWrite(ledPins[0], LOW);   // ACTIVE LOW: LOW = relay ON
  delay(1000);
  digitalWrite(ledPins[0], HIGH);  // ACTIVE LOW: HIGH = relay OFF
  Serial.println("[BOOT TEST] GPIO13 HIGH → Relay OFF → Motor berhenti.");
  WiFi.mode(WIFI_AP_STA); delay(100);
  WiFi.softAP(AP_SSID, AP_PASSWORD, 6); delay(500);

  Serial.println("[AP]  Hotspot  : "+String(AP_SSID));
  Serial.println("[AP]  Password : "+String(AP_PASSWORD));
  Serial.println("[AP]  IP       : "+WiFi.softAPIP().toString());
  Serial.println("[Web] Portal   : http://192.168.4.1");

  server.on("/",       HTTP_GET,  handleRoot);
  server.on("/scan",   HTTP_POST, handleScan);
  server.on("/simpan", HTTP_POST, handleSimpan);
  server.on("/reset",  HTTP_POST, handleReset);
  server.on("/status", HTTP_GET,  handleStatus);
  server.begin();

  if (loadSavedWifi()) {
    wifiConnected=true; rgbHijau();
    mqttClient.begin(MQTT_SERVER, espClient);
    mqttClient.onMessage(onMessage);
    Serial.println("[OK] Siap! Tekan nada di web controller.");
  } else {
    rgbMerah();
    Serial.println("[INFO] Konek ke WiFi Angklung-Robot → buka 192.168.4.1");
  }
}

// LOOP
void loop() {
  server.handleClient();

  if (scanRunning) {
    int result=WiFi.scanComplete();
    if (result>=0){scanRunning=false;scanReady=true;Serial.println("[Scan] Selesai: "+String(result)+" jaringan");}
  }

  if (pendingConnect) {
    pendingConnect=false; rgbKuning();
    Serial.println("[WiFi] Konek ke: "+pendingSSID);
    WiFi.begin(pendingSSID.c_str(), pendingPass.c_str());
    for (int i=0;i<20&&WiFi.status()!=WL_CONNECTED;i++){delay(500);Serial.print(".");server.handleClient();}
    if (WiFi.status()==WL_CONNECTED){
      wifiConnected=true; rgbHijau();
      Serial.println("\n[WiFi] OK: "+WiFi.localIP().toString());
      mqttClient.begin(MQTT_SERVER, espClient);
      mqttClient.onMessage(onMessage);
      Serial.println("[OK] Siap!");
    } else {
      rgbMerah();
      Serial.println("\n[WiFi] Gagal!");
      prefs.begin("wifi",false); prefs.clear(); prefs.end();
      savedSSID=""; savedPass=""; pendingSSID="";
    }
  }

  if (wifiConnected) {
    if (!mqttClient.connected()){rgbHijau(); reconnectMQTT();}
    else rgbBiru();
    mqttClient.loop();
  } else {
    static unsigned long lastBlink=0;
    if (millis()-lastBlink>800){
      lastBlink=millis();
      static bool state=false;
      state=!state;
      state ? rgbMerah() : rgbMati();
    }
  }
}