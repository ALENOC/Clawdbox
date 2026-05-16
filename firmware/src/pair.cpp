#include "pair.h"
#include "wifi_cfg.h"
#include "anthropic_ca.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <mbedtls/base64.h>
#include <esp_random.h>
#include <esp_system.h>
#include <time.h>

// Anthropic Claude Code OAuth (Authorization Code + PKCE).
// If these stop working, re-verify against Claude Code CLI source.
static const char* CLIENT_ID     = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";
static const char* AUTHORIZE_URL = "https://claude.ai/oauth/authorize";
static const char* TOKEN_URL     = "https://console.anthropic.com/v1/oauth/token";
static const char* REDIRECT_URI  = "https://platform.claude.com/oauth/code/callback";
static const char* SCOPE         = "user:inference user:profile user:sessions:claude_code user:mcp_servers";

static AsyncWebServer pair_server(80);
static bool   server_started = false;
static bool   active = false;

static String code_verifier;
static String code_challenge;
static String state_token;
static String auth_url;
static String lan_url;
static pair_status_t status = PAIR_IDLE;
static String status_msg = "";

// Single-slot queue: code submitted via /pair POST, exchanged in pair_tick().
static volatile bool code_pending = false;
static String pending_code;

static String b64url_encode(const uint8_t* data, size_t len) {
    size_t need = 0;
    mbedtls_base64_encode(nullptr, 0, &need, data, len);
    String s;
    s.reserve(need);
    unsigned char* buf = (unsigned char*)malloc(need + 1);
    if (!buf) return s;
    size_t olen = 0;
    if (mbedtls_base64_encode(buf, need + 1, &olen, data, len) == 0) {
        buf[olen] = 0;
        s = (char*)buf;
    }
    free(buf);
    s.replace('+', '-');
    s.replace('/', '_');
    while (s.length() && s[s.length() - 1] == '=') s.remove(s.length() - 1);
    return s;
}

static String url_encode(const String& in) {
    String out;
    out.reserve(in.length() * 3);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
            out += buf;
        }
    }
    return out;
}

static void gen_pkce(void) {
    uint8_t verifier_bytes[32];
    uint8_t state_bytes[32];
    esp_fill_random(verifier_bytes, sizeof(verifier_bytes));
    esp_fill_random(state_bytes,    sizeof(state_bytes));
    code_verifier = b64url_encode(verifier_bytes, sizeof(verifier_bytes));
    state_token   = b64url_encode(state_bytes,    sizeof(state_bytes));

    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx,
        (const unsigned char*)code_verifier.c_str(), code_verifier.length());
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    code_challenge = b64url_encode(hash, 32);
}

static void build_auth_url(void) {
    auth_url  = String(AUTHORIZE_URL) + "?code=true";
    auth_url += "&client_id=" + String(CLIENT_ID);
    auth_url += "&response_type=code";
    auth_url += "&redirect_uri=" + url_encode(REDIRECT_URI);
    auth_url += "&scope=" + url_encode(SCOPE);
    auth_url += "&code_challenge=" + code_challenge;
    auth_url += "&code_challenge_method=S256";
    auth_url += "&state=" + state_token;
}

static void serve_form(AsyncWebServerRequest* req) {
    static const char* P =
        "<!doctype html><html><head><meta charset=utf-8>"
        "<meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Clawdmeter pair</title>"
        "<style>body{font-family:system-ui,sans-serif;max-width:480px;margin:24px auto;padding:0 16px;background:#111;color:#eee}"
        "h1{font-size:20px}label{display:block;margin:12px 0 4px;color:#bbb;font-size:13px}"
        "input{width:100%;box-sizing:border-box;padding:10px;background:#222;color:#eee;border:1px solid #333;border-radius:6px;font-family:monospace;font-size:13px}"
        "button{margin-top:16px;width:100%;padding:12px;background:#cc7a3a;color:#fff;border:0;border-radius:6px;font-size:15px;font-weight:600;cursor:pointer}"
        ".hint{font-size:12px;color:#888;margin:8px 0 16px}</style></head><body>"
        "<h1>Pair Clawdmeter</h1>"
        "<p class=hint>1. Scan the QR on the device.<br>"
        "2. Authorize on claude.ai.<br>"
        "3. Copy the full <b>CODE#STATE</b> from the callback page and paste it below.</p>"
        "<form method=POST action=/pair>"
        "<label>CODE#STATE</label>"
        "<input name=cs autocomplete=off autofocus required>"
        "<button type=submit>Submit</button></form></body></html>";
    req->send(200, "text/html", P);
}

static void serve_post(AsyncWebServerRequest* req) {
    if (!req->hasParam("cs", true)) {
        req->send(400, "text/plain", "missing cs");
        return;
    }
    if (code_pending) {
        req->send(429, "text/plain", "exchange already pending");
        return;
    }
    String cs = req->getParam("cs", true)->value();
    cs.trim();
    pending_code = cs;
    code_pending = true;
    req->send(200, "text/html",
        "<!doctype html><meta charset=utf-8>"
        "<style>body{font-family:system-ui;background:#111;color:#eee;padding:24px}</style>"
        "<h1>Received</h1><p>Check the device screen for status.</p>");
}

void pair_start(void) {
    Serial.println("pair: start");
    gen_pkce();
    build_auth_url();
    lan_url = String("http://") + WiFi.localIP().toString() + "/pair";

    Serial.println("pair: AUTH_URL =");
    Serial.println(auth_url);
    Serial.print("pair: LAN_URL = ");
    Serial.println(lan_url);

    if (!server_started) {
        pair_server.on("/pair", HTTP_GET,  serve_form);
        pair_server.on("/pair", HTTP_POST, serve_post);
        pair_server.on("/",     HTTP_GET,
            [](AsyncWebServerRequest* r){ r->redirect("/pair"); });
        pair_server.begin();
        server_started = true;
    }
    code_pending = false;
    pending_code = "";
    status = PAIR_WAITING;
    status_msg = "Waiting for code";
    active = true;
}

static bool exchange_code(const String& code) {
    Serial.println("pair: exchanging code");
    WiFiClientSecure client;
    client.setCACert(ANTHROPIC_ROOT_CA);
    HTTPClient http;
    if (!http.begin(client, TOKEN_URL)) {
        status_msg = "http.begin failed";
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String body;
    body.reserve(800);
    body  = "grant_type=authorization_code";
    body += "&code=" + url_encode(code);
    body += "&redirect_uri=" + url_encode(String(REDIRECT_URI));
    body += "&client_id=" + String(CLIENT_ID);
    body += "&code_verifier=" + code_verifier;
    body += "&state=" + state_token;

    int rc = http.POST(body);
    String resp = (rc > 0) ? http.getString() : "";
    http.end();

    if (rc != 200) {
        Serial.printf("pair: HTTP %d body=%s\n", rc, resp.c_str());
        status_msg = "HTTP " + String(rc);
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, resp)) {
        status_msg = "parse error";
        return false;
    }
    String access  = doc["access_token"]  | "";
    String refresh = doc["refresh_token"] | "";
    uint32_t exp_in = doc["expires_in"]   | 3600;
    if (!access.length() || !refresh.length()) {
        status_msg = "missing tokens in response";
        return false;
    }
    time_t now = time(nullptr);
    uint64_t exp_ms = (now > 1700000000) ? ((uint64_t)now + exp_in) * 1000ULL : 0;
    cfg_update_tokens(access, refresh, exp_ms);
    Serial.println("pair: tokens saved");
    return true;
}

void pair_tick(void) {
    if (!code_pending) return;
    String cs = pending_code;
    pending_code = "";
    code_pending = false;

    status = PAIR_EXCHANGING;
    status_msg = "Exchanging";

    int hash = cs.indexOf('#');
    String code     = (hash > 0) ? cs.substring(0, hash) : cs;
    String state_in = (hash > 0) ? cs.substring(hash + 1) : "";
    if (state_in.length() && state_in != state_token) {
        Serial.println("pair: state mismatch");
        status = PAIR_FAIL;
        status_msg = "state mismatch";
        return;
    }
    if (exchange_code(code)) {
        status = PAIR_OK;
        status_msg = "Paired. Rebooting";
        delay(1500);
        ESP.restart();
    } else {
        status = PAIR_FAIL;
    }
}

bool pair_is_active(void)             { return active; }
pair_status_t pair_get_status(void)   { return status; }
const char* pair_get_status_msg(void) { return status_msg.c_str(); }
const char* pair_get_auth_url(void)   { return auth_url.c_str(); }
const char* pair_get_lan_url(void)    { return lan_url.c_str(); }
