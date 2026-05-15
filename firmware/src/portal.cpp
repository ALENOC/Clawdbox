#include "portal.h"
#include "wifi_cfg.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static const char* AP_SSID = "Clawdmeter-setup";
static AsyncWebServer server(80);
static DNSServer dns;
static bool active = false;
static volatile bool reboot_pending = false;
static uint32_t reboot_at_ms = 0;

// WiFi-only setup. OAuth pairing happens on first boot via on-device QR.
static const char* PAGE = R"HTML(
<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Clawdmeter setup</title>
<style>
 body{font-family:system-ui,sans-serif;max-width:520px;margin:24px auto;padding:0 16px;background:#111;color:#eee}
 h1{font-size:20px;margin:0 0 8px}
 h2{font-size:14px;margin:20px 0 6px;color:#aaa;text-transform:uppercase;letter-spacing:.08em}
 label{display:block;font-size:13px;margin:8px 0 4px;color:#bbb}
 input{width:100%;box-sizing:border-box;padding:8px 10px;background:#222;color:#eee;border:1px solid #333;border-radius:6px;font-family:monospace;font-size:13px}
 button{margin-top:18px;width:100%;padding:12px;background:#cc7a3a;color:#fff;border:0;border-radius:6px;font-size:15px;font-weight:600;cursor:pointer}
 .hint{font-size:12px;color:#888;margin-top:4px}
 .ssid_list{display:flex;flex-direction:column;gap:4px;margin:8px 0;max-height:240px;overflow:auto}
 .ssid_row{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;background:#1d1d1d;border:1px solid #2a2a2a;border-radius:6px;cursor:pointer;font-size:13px}
 .ssid_row:hover{background:#262626;border-color:#3a3a3a}
 .ssid_row.sel{background:#cc7a3a22;border-color:#cc7a3a}
 .ssid_meta{color:#888;font-size:11px;font-family:monospace}
 .reload{display:inline-block;margin-left:8px;color:#cc7a3a;text-decoration:underline;cursor:pointer;font-size:12px}
</style></head><body>
<h1>Clawdmeter setup</h1>
<form method=POST action=/save>
 <h2>WiFi <span class=hint id=scan_status>(scanning...)</span></h2>
 <div id=ssid_list class=ssid_list></div>
 <label>SSID</label><input name=ssid id=ssid_input required autocomplete=off>
 <label>Password</label><input name=psk type=password>
 <div class=hint>After reboot the device will display a QR code to authorize Claude.</div>
 <button type=submit>Save & reboot</button>
</form>
<script>
let scanTries=0;
async function loadScan(){
 const st=document.getElementById('scan_status');
 const list=document.getElementById('ssid_list');
 const input=document.getElementById('ssid_input');
 try{
  const r=await fetch('/scan'); const j=await r.json();
  if(j.networks.length===0 && scanTries<8){
   scanTries++; st.textContent='(scanning... '+scanTries+')';
   setTimeout(loadScan,1500); return;
  }
  list.innerHTML='';
  j.networks.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
   const row=document.createElement('div'); row.className='ssid_row';
   row.innerHTML='<span>'+n.ssid.replace(/[<>&]/g,c=>({"<":"&lt;",">":"&gt;","&":"&amp;"}[c]))+(n.secure?' 🔒':'')+'</span><span class=ssid_meta>'+n.rssi+' dBm</span>';
   row.onclick=()=>{
    input.value=n.ssid;
    document.querySelectorAll('.ssid_row').forEach(r=>r.classList.remove('sel'));
    row.classList.add('sel');
   };
   list.appendChild(row);
  });
  st.innerHTML='('+j.networks.length+' found) <span class=reload onclick="scanTries=0;loadScan()">rescan</span>';
 }catch(e){st.textContent='(scan failed)';}
}
loadScan();
</script>
</body></html>
)HTML";

void portal_start(void) {
    // AP_STA mode required for WiFi.scanNetworks() while portal is up.
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("Portal AP up: %s  ip=%s\n", AP_SSID, ip.toString().c_str());

    // Kick off async scan so /scan can return cached results quickly.
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);

    dns.start(53, "*", ip);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
        req->send(200, "text/html", PAGE);
    });

    server.on("/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            req->send(200, "application/json", "{\"networks\":[]}");
            return;
        }
        if (n < 0) {
            // No scan in progress — kick one off, return empty for this call.
            WiFi.scanNetworks(true, false);
            req->send(200, "application/json", "{\"networks\":[]}");
            return;
        }
        String out = "{\"networks\":[";
        for (int i = 0; i < n; i++) {
            if (i) out += ",";
            String ssid = WiFi.SSID(i);
            ssid.replace("\\", "\\\\");
            ssid.replace("\"", "\\\"");
            out += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
                   ",\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
        }
        out += "]}";
        WiFi.scanDelete();
        // Trigger next scan in background for the next reload.
        WiFi.scanNetworks(true, false);
        req->send(200, "application/json", out);
    });

    // Captive portal probes — redirect everything to /
    server.onNotFound([](AsyncWebServerRequest* req) {
        req->redirect("/");
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest* req) {
        if (!req->hasParam("ssid", true)) {
            req->send(400, "text/plain", "missing ssid");
            return;
        }
        WifiCfg c;
        c.ssid = req->getParam("ssid", true)->value();
        c.psk  = req->hasParam("psk", true) ? req->getParam("psk", true)->value() : "";
        c.ssid.trim();
        c.psk.trim();
        // Leave tokens untouched (cfg_save skips empty strings).
        cfg_save(c);
        req->send(200, "text/html", "<h1>Saved. Rebooting...</h1>");
        reboot_pending = true;
        reboot_at_ms = millis() + 1500;
    });

    server.begin();
    active = true;
}

void portal_tick(void) {
    if (!active) return;
    dns.processNextRequest();
    if (reboot_pending && (int32_t)(millis() - reboot_at_ms) >= 0) {
        Serial.println("Portal: rebooting");
        ESP.restart();
    }
}

bool portal_is_active(void) { return active; }
const char* portal_ssid(void) { return AP_SSID; }
