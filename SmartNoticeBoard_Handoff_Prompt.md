# HANDOFF PROMPT — Smart Notice Board (ESP32) — continue from Session 2

Paste everything below into Claude. It contains the full project context, what is already built and working, the exact wiring, the current working code, and the detailed spec for what to build next (firmware upgrade + web dashboard).

---

## 0. How to work with me (IMPORTANT — read first)

- I am a **beginner** in electronics and embedded code. Explain in **very simple words**.
- Always answer in **short numbered steps (Step 1, Step 2 …)**. One small task at a time, then wait for me to confirm before moving on.
- **Always give me complete files to paste** (full sketch / full HTML), not partial edits. Partial "find this line and add below" edits confused me before. If a file is long, still give the whole file.
- Tell me exactly **which file / which window** to paste into, and what I should see when it works.
- **Never change the hardware wiring** unless I ask. All hardware is finished. If a change is truly needed, explain why first and ask.
- Before any wiring change: remind me to **unplug the USB cable**.
- Compiling takes **2–4 minutes** on my laptop (normal). Red `#pragma message ("BT: forcing BR/EDR max sync conn eff ...")` lines during compile are **harmless notes**, not errors.
- Keep it **short and simple**. No cloud, no internet, no soldering, no extra parts.

---

## 1. Project info

- **Project:** Smart Notice Board (IoE Lab mini project)
- **College:** Don Bosco Institute of Technology (Bombay Salesian Society), Dept. of Information Technology, BE Sem VII, AY 2026–27
- **Team:** Fauzan Ansari (02), Soham Dhakras (17), Sabiq Khot (30), Rayan Shaikh (51)
- **Concept (final, simplified):** A physical digital notice board. The ESP32 creates its **own WiFi hotspot** (no internet, no cloud, no Firebase). Staff connect a phone/laptop to it, open a **web dashboard** at `192.168.4.1`, and post notices that appear on a 3.5" colour screen. Includes clock, scheduling, categories, SOS emergency mode, buzzer alerts, status LEDs and a physical button.
- **Note:** The original presentation (PPT) mentioned Firebase / cloud / mobile app / 16x2 LCD. Those were **dropped**. The slides will need updating later (Introduction, Scope, Literature Survey "proposed system" row, Implementation Plan, Hardware & Software tables). Cloud/remote access = "Future enhancement".

---

## 2. My setup (software / PC)

- **Laptop:** Windows. Phone: **iPhone** (Safari).
- **Arduino IDE 2.3.10**, board = **ESP32 Dev Module**, port **COM3**.
- **ESP32 Arduino core: 3.3.7** (Espressif).
- **Sketchbook location:** `C:\Arduino` (moved there on purpose — the old OneDrive Documents path contained Vietnamese characters "Tài liệu" which broke compiling. Do NOT move it back.)
- **Libraries installed:**
  - **TFT_eSPI** by Bodmer (display) — configured via `C:\Arduino\libraries\TFT_eSPI\User_Setup.h` (contents below)
  - **RTClib** by Adafruit (DS3231 clock)
  - Built-in: `WiFi.h`, `WebServer.h`, `Preferences.h`
  - **Not yet installed:** ArduinoJson (will likely be needed for the API — tell me how to install it: Library Manager → "ArduinoJson" by Benoit Blanchon → Install)
- **VS Code** with Claude extension — I want to **build the dashboard (HTML/CSS/JS) in VS Code**.

### TFT_eSPI `User_Setup.h` (current, working — do not change)
```cpp
// Smart Notice Board - display settings
#define RPI_DISPLAY_TYPE
#define ILI9486_DRIVER

#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TOUCH_CS  5

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY        20000000
#define SPI_TOUCH_FREQUENCY   2500000
```

---

## 3. Hardware (ALL DONE — working and tested)

| Part | Details |
|---|---|
| **ESP32 DevKit** | ESP-WROOM-32, **30-pin**, Micro-USB. Powered by USB from laptop (later a 5V phone charger). Sits **off the breadboard** (too wide); wires plug directly onto its pins with female sockets. |
| **Display** | **"3.5inch RPi Display"** (Raspberry-Pi style board, 26-hole female socket), **ILI9486** driver via 74HC4094 shift registers (TFT_eSPI `RPI_DISPLAY_TYPE`), 480×320, **XPT2046 resistive touch** (touch wired but NOT used). **Backlight is always on** (cannot be dimmed by code). Colours confirmed correct (R→G→B test). Black looks slightly bluish at an angle — normal. Used in **landscape: `tft.setRotation(1)`**. |
| **RTC** | **DS3231** module with CR2032 coin cell (keeps time when unplugged). I2C. Time is synced from the phone via the web page. |
| **Push button** | 4-leg tactile button on breadboard across the middle groove, two diagonal legs used. `INPUT_PULLUP`, pressed = LOW. |
| **Buzzer** | **Active 5V buzzer**, driven **directly** from a GPIO at 3.3V (no transistor). HIGH = beep. |
| **Green LED** | with **1kΩ** resistor to GND. HIGH = on. |
| **Red LED** | with **1kΩ** resistor to GND. HIGH = on. |
| **Breadboard** | Used only for button, LEDs/resistors and a shared **GND strip ("G")**. |

### Removed / NOT used (do not suggest them again)
4-channel relay, HC-SR04 ultrasonic, 18650 battery + TP4056 charger, MT3608 boost converter, BC547 transistors, capacitors, touch input, soldering, Firebase/cloud/internet, mobile app.

### Exact pin map (FINAL)

| Device | Device pin | ESP32 pin | GPIO |
|---|---|---|---|
| Display | hole 2 (5V) | VIN | – |
| Display | hole 6 (GND) | GND (the one next to 3V3) | – |
| Display | hole 19 (MOSI) | D23 | 23 |
| Display | hole 21 (MISO / touch out) | D19 | 19 |
| Display | hole 23 (SCLK) | D18 | 18 |
| Display | hole 24 (LCD CS) | D15 | 15 |
| Display | hole 18 (DC/RS) | D2 | 2 |
| Display | hole 22 (RST) | D4 | 4 |
| Display | hole 26 (Touch CS) | D5 | 5 |
| RTC DS3231 | VCC | 3V3 | – |
| RTC DS3231 | SDA | D21 | 21 |
| RTC DS3231 | SCL | D22 | 22 |
| RTC DS3231 | GND | G strip | – |
| Button | one diagonal leg | D33 | 33 (INPUT_PULLUP) |
| Button | other diagonal leg | G strip | – |
| Buzzer | + (long leg) | D13 | 13 |
| Buzzer | − | G strip | – |
| Green LED | long leg (via breadboard line) | RX2 | 16 |
| Green LED | short leg → 1kΩ → | G strip | – |
| Red LED | long leg (via breadboard line) | TX2 | 17 |
| Red LED | short leg → 1kΩ → | G strip | – |
| **G strip** | fed by | GND (the one next to VIN) | – |

Free GPIOs if ever needed: 14, 25, 26, 27, 32 (avoid 12 – boot strapping; 34–39 are input-only; 1/3 are USB serial).

---

## 4. Current working firmware (uploaded, tested — everything below works)

What works now:
- ESP32 hotspot **SSID `SmartNoticeBoard`, password `12345678`**, IP **192.168.4.1** (phone connects fine; iPhone may say "no internet" → stay connected).
- Simple web page at `/`: textarea → "Show on board", "Clear notice", "Sync clock with my phone" (sends phone's local time as epoch to `/settime?t=`).
- **One** notice shown on screen (yellow text, word-wrapped), saved in flash (`Preferences`, namespace `board`, key `notice`) — survives power off.
- Header bar: "NOTICE BOARD" + live clock `HH:MM:SS  DD/MM` (top-right, refreshes every second).
- Button: short press → green footer "Button works!" + 1 beep + green LED; long press 3 s → red footer "Long press: SOS (coming soon)" + 3 beeps + red LED.
- Buzzer: 1 beep at boot, 2 beeps when a new notice is posted.
- LEDs: green on at boot (normal), red on for SOS test.

```cpp
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <RTClib.h>

const char* WIFI_NAME = "SmartNoticeBoard";
const char* WIFI_PASS = "12345678";
const int BUTTON_PIN = 33;
const int BUZZER_PIN = 13;
const int GREEN_LED  = 16;   // RX2 pin
const int RED_LED    = 17;   // TX2 pin

TFT_eSPI tft = TFT_eSPI();
WebServer server(80);
Preferences prefs;
RTC_DS3231 rtc;
bool rtcOK = false;
String notice = "";
unsigned long lastClock = 0;

bool lastBtn = HIGH;
unsigned long pressStart = 0;
bool longDone = false;

void beep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(onMs);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

void setLeds(bool allOK) {
  digitalWrite(GREEN_LED, allOK ? HIGH : LOW);
  digitalWrite(RED_LED,   allOK ? LOW  : HIGH);
}

void drawWrapped(const String& text, int x, int y, int maxW, int font) {
  int lineH = tft.fontHeight(font) + 6;
  String line = "", word = "";
  for (int i = 0; i <= (int)text.length(); i++) {
    char c = (i < (int)text.length()) ? text[i] : ' ';
    if (c == ' ' || c == '\n') {
      if (word.length() || c == '\n') {
        String test = line.length() ? line + " " + word : word;
        if (word.length() && tft.textWidth(test, font) > maxW && line.length()) {
          tft.drawString(line, x, y, font); y += lineH; line = word;
        } else {
          line = test;
        }
        word = "";
      }
      if (c == '\n') { tft.drawString(line, x, y, font); y += lineH; line = ""; }
      if (y > tft.height() - lineH) return;
    } else {
      word += c;
    }
  }
  if (line.length()) tft.drawString(line, x, y, font);
}

void drawClock() {
  char buf[24];
  if (rtcOK) {
    DateTime now = rtc.now();
    sprintf(buf, "%02d:%02d:%02d  %02d/%02d", now.hour(), now.minute(), now.second(), now.day(), now.month());
  } else {
    strcpy(buf, "No clock");
  }
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth("88:88:88  88/88", 4));
  tft.drawString(buf, tft.width() - 10, 10, 4);
  tft.setTextPadding(0);
  tft.setTextDatum(TL_DATUM);
}

void showFooter(const String& msg, uint16_t color) {
  tft.fillRect(0, tft.height() - 36, tft.width(), 36, color);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(msg, 12, tft.height() - 30, 4);
}

void showNotice() {
  tft.fillScreen(TFT_BLACK);
  tft.fillRect(0, 0, tft.width(), 44, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("NOTICE BOARD", 12, 10, 4);
  drawClock();

  if (notice.length() == 0) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawWrapped("No notice yet.\n\nWiFi: " + String(WIFI_NAME) +
                "\nPassword: " + String(WIFI_PASS) +
                "\nThen open: 192.168.4.1", 12, 60, tft.width() - 24, 4);
  } else {
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    drawWrapped(notice, 12, 60, tft.width() - 24, 4);
  }
}

void checkButton() {
  bool nowBtn = digitalRead(BUTTON_PIN);
  if (nowBtn != lastBtn) delay(20);
  nowBtn = digitalRead(BUTTON_PIN);

  if (nowBtn == LOW && lastBtn == HIGH) {
    pressStart = millis();
    longDone = false;
  }
  if (nowBtn == LOW && !longDone && millis() - pressStart > 3000) {
    longDone = true;
    setLeds(false);
    showFooter("Long press: SOS (coming soon)", TFT_RED);
    beep(3, 150, 100);
  }
  if (nowBtn == HIGH && lastBtn == LOW && !longDone) {
    setLeds(true);
    showFooter("Button works!", TFT_DARKGREEN);
    beep(1, 80, 0);
  }
  lastBtn = nowBtn;
}

String htmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  return s;
}

void handleRoot() {
  String page =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Smart Notice Board</title><style>"
    "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 16px}"
    "textarea{width:100%;height:140px;font-size:16px;box-sizing:border-box}"
    "button{width:100%;padding:12px;font-size:18px;margin-top:8px}"
    "</style></head><body><h2>Smart Notice Board</h2>"
    "<form method='POST' action='/post'>"
    "<textarea name='msg' maxlength='300' placeholder='Type your notice here'>";
  page += htmlEscape(notice);
  page += "</textarea><button type='submit'>Show on board</button></form>"
          "<form method='POST' action='/clear'><button type='submit'>Clear notice</button></form>"
          "<button onclick=\"fetch('/settime?t='+Math.floor((Date.now()-new Date().getTimezoneOffset()*60000)/1000)).then(()=>alert('Clock synced'))\">Sync clock with my phone</button>"
          "</body></html>";
  server.send(200, "text/html", page);
}

void saveAndShow(const String& text) {
  notice = text;
  notice.replace("\r", "");
  notice.trim();
  prefs.putString("notice", notice);
  showNotice();
  if (notice.length()) beep(2, 120, 80);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handlePost()  { saveAndShow(server.arg("msg")); }
void handleClear() { saveAndShow(""); }

void handleSetTime() {
  if (rtcOK && server.hasArg("t")) {
    uint32_t t = strtoul(server.arg("t").c_str(), nullptr, 10);
    rtc.adjust(DateTime(t));
    drawClock();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "No clock");
  }
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  setLeds(true);

  tft.init();
  tft.setRotation(1);

  prefs.begin("board", false);
  notice = prefs.getString("notice", "");

  rtcOK = rtc.begin();
  if (rtcOK && rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  WiFi.softAP(WIFI_NAME, WIFI_PASS);
  server.on("/", handleRoot);
  server.on("/post", HTTP_POST, handlePost);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/settime", handleSetTime);
  server.begin();

  showNotice();
  beep(1, 80, 0);
}

void loop() {
  server.handleClient();
  checkButton();
  if (millis() - lastClock >= 1000) {
    lastClock = millis();
    drawClock();
  }
}
```

---

## 5. What to build next (Session 2) — firmware upgrade + web dashboard

### 5.1 Architecture (agreed)
- **ESP32 firmware = the brain.** Controls screen/clock/buzzer/LEDs/button AND exposes a small **JSON REST API** over its own WiFi.
- **Dashboard = the remote control.** A single-page website (HTML + CSS + vanilla JavaScript) that I write in **VS Code** as `index.html`. It calls the API with `fetch()`.
- **Final deployment:** the dashboard is **served by the ESP32 itself** at `http://192.168.4.1/`. Plan: after I finish `index.html` in VS Code, it gets pasted into the Arduino project as a separate tab/file **`dashboard.h`** containing a raw string literal, e.g.
  `const char DASHBOARD_HTML[] PROGMEM = R"rawliteral( ...html... )rawliteral";`
  and served with `server.send_P(200, "text/html", DASHBOARD_HTML);`. (Avoid needing LittleFS upload plugins for the HTML.)
- **Development/testing in VS Code:** I connect my **laptop to the `SmartNoticeBoard` WiFi** and open `index.html` locally. So:
  - The firmware must send **CORS headers** on every API response (`Access-Control-Allow-Origin: *`, allow methods GET/POST/PUT/DELETE/OPTIONS, allow header Content-Type) and answer **OPTIONS** preflight requests (e.g. in `server.onNotFound`).
  - The dashboard should use a configurable `API_BASE`: if the page is opened from `file://` (or not from 192.168.4.1) use `http://192.168.4.1`, otherwise use relative URLs (`""`).
  - Optional nice-to-have: a **demo/mock mode** in the dashboard (fake data) so I can design the UI without the board connected.

### 5.2 HARD constraints for the dashboard
- **No internet on the ESP32 hotspot** → **NO CDNs, NO Google Fonts, NO external libraries, NO images from the web.** Everything inline in one `index.html` (CSS in `<style>`, JS in `<script>`). System fonts only.
- **Small file size** (aim < 40 KB) — it lives in ESP32 flash.
- **Mobile-first**, must work well on **iPhone Safari** (big tap targets, 16px+ inputs to avoid iOS zoom), also fine on laptop.
- Plain, clean, professional look suitable for a college demo.

### 5.3 Firmware features to add
1. **Multiple notices** (store up to ~10). Each notice:
   `id` (int), `title` (≤ 40 chars), `text` (≤ 300 chars), `category` (`General`, `Exam`, `Event`, `Placement`, `Urgent`), `priority` (`normal`, `high`, `urgent`), `start` (local epoch seconds, 0 = immediately), `end` (local epoch seconds, 0 = never expires), `created` (local epoch).
   - Persist across power loss. NVS `Preferences` strings are limited (~4 KB per key), so either store **each notice under its own key** or use **LittleFS** (`LittleFS.begin(true)`, file `/notices.json`, default partition scheme). Pick the simplest robust option and explain it to me.
   - Migrate/ignore the old single `notice` key.
2. **Rotation on screen:** active notices cycle automatically every **N seconds** (default **8 s**, configurable from dashboard). Order: urgent first, then high, then normal; newest first within same priority. Show a page indicator like `2/5` and the **category tag** in its colour.
3. **Category colours** (screen + dashboard should match):
   General = white/grey, Exam = yellow/amber, Event = green, Placement = cyan/blue, Urgent = red.
4. **Scheduling:** a notice is shown only when `start <= now` and (`end == 0` or `now < end`), using the **DS3231 time**. Expired notices are hidden (and can be auto-deleted or kept as "expired" — dashboard shows status).
5. **Screen layout (480×320 landscape):** header bar (title "NOTICE BOARD" left, clock right — already exists), category tag + title, word-wrapped text body, footer with `n/N` indicator (and maybe "WiFi: SmartNoticeBoard  |  192.168.4.1" when no notices). Keep the existing "No notice yet + WiFi instructions" screen when there are no active notices. Avoid full-screen redraw flicker where possible (clock redraw only updates its area).
6. **SOS / Emergency mode:**
   - Triggered from the **dashboard** (preset messages: "FIRE DRILL – Evacuate the building now", "MEDICAL EMERGENCY – Help needed", "LOCKDOWN – Stay inside, lock doors", "EARTHQUAKE – Move to open area", or a custom message) **or** by **holding the physical button 3 s** (default message "EMERGENCY – Please follow instructions").
   - While active: screen **flashes red/white** (or red background) with the message in **big text**, buzzer beeps in a repeating pattern (e.g. 3 beeps every ~5 s, non-blocking), **red LED blinks**, green LED off; dashboard shows a **red SOS banner** at the top for every viewer.
   - Cleared from the dashboard ("Clear SOS"), and (nice for demo) holding the button 3 s again while SOS is active also clears it.
   - SOS state does not need to survive reboot (optional).
7. **Button:** short press → show **next notice** immediately (+ short beep). Long press 3 s → SOS on/off (as above).
8. **Buzzer:** 1 beep at boot, 2 beeps on new notice, SOS pattern as above. Make beeps **non-blocking** (millis-based) so the web server stays responsive (current `beep()` uses `delay()`).
9. **LEDs:** green ON = normal; red blinking = SOS; red ON solid = problem (e.g. RTC not found).
10. **Clock sync:** keep the existing `/settime?t=<local epoch>` behaviour (phone sends `Math.floor((Date.now() - new Date().getTimezoneOffset()*60000)/1000)` = local time as epoch; RTC stores **local IST time**). Expose it in the API too.
11. **Text safety:** TFT built-in fonts only support basic ASCII — replace/strip non-ASCII characters (smart quotes, emoji, ₹ etc.) before drawing. Escape HTML in the dashboard.
12. Keep `loop()` non-blocking (no long `delay()`s).

### 5.4 API spec (suggested — keep it simple and consistent; all JSON; add CORS)
| Method | Path | Body / query | Response |
|---|---|---|---|
| GET | `/` | – | dashboard HTML (from `dashboard.h`) |
| GET | `/api/status` | – | `{ "time":"2026-10-10 14:30:05", "epoch":1791..., "rtc":true, "uptime":12345, "heap":180000, "clients":1, "notices":3, "active":2, "rotate":8, "sos":{"active":false,"message":""} }` |
| GET | `/api/notices` | – | `[ {id,title,text,category,priority,start,end,created,state:"active"/"scheduled"/"expired"} ]` |
| POST | `/api/notices` | JSON notice (no id) | created notice (with id) — triggers 2 beeps + shows it on screen |
| PUT | `/api/notices?id=3` | JSON fields to update | updated notice |
| DELETE | `/api/notices?id=3` | – | `{ "ok": true }` |
| POST | `/api/notices/clear` | – | delete all |
| POST | `/api/next` | – | board shows next notice |
| POST | `/api/sos` | `{ "message": "..." }` | `{ "ok": true }` |
| POST | `/api/sos/clear` | – | `{ "ok": true }` |
| POST | `/api/time` | `{ "t": <local epoch> }` (keep old `/settime?t=` too) | `{ "ok": true }` |
| GET/POST | `/api/settings` | `{ "rotate": 8 }` | settings |

(ESP32 `WebServer` has no path parameters → use `?id=` query. JSON body is in `server.arg("plain")`. Use **ArduinoJson v7**.)

### 5.5 Dashboard features (index.html)
1. **Header:** "Smart Notice Board" + connection status dot (green connected / red offline) + board time (from `/api/status`).
2. **SOS banner:** full-width red banner at the very top when SOS is active, showing the message + "Clear SOS" button.
3. **Post notice form:** Title, Message (textarea with live character counter, max 300), Category (dropdown), Priority (Normal / High / Urgent), "Show from" and "Show until" (`<input type="datetime-local">`, optional; convert to the same local-epoch format as the clock sync), "Post to board" button. Validation + inline error messages. Success toast.
4. **Notice list:** cards showing title, text preview, category colour tag, priority, schedule, and status badge (Active / Scheduled / Expired). Buttons: **Edit** (fills the form, then "Save changes") and **Delete** (with confirm). "Clear all" button with confirm. Empty state message.
5. **SOS panel:** big red **SOS** button → choose preset or type custom message → confirm → sends `/api/sos`.
6. **Board controls:** "Show next notice", "Sync clock with this phone", rotation speed (seconds) setting.
7. **Status panel:** board time, RTC OK, uptime, free memory, connected devices, number of notices / active notices.
8. **Auto-refresh** status + list every ~3–5 s (polling). Show "Board offline" if requests fail.
9. Toast notifications for success/error. Disable buttons while a request is running.
10. Optional (only if easy): simple **admin PIN** prompt before posting/SOS (stored in firmware). Not required.

### 5.6 Suggested order of work (one step at a time, wait for my OK each time)
1. Install ArduinoJson (tell me exactly how).
2. Give me the **full upgraded firmware** (multiple notices, categories, rotation, scheduling, SOS, non-blocking buzzer/LEDs, CORS JSON API). Temporarily keep a minimal built-in page at `/` so I can still test from the phone. I upload and test each feature with simple checks you tell me.
3. Build the dashboard `index.html` in VS Code step by step (layout → notice form → list → SOS → status), testing against the real board from my laptop (laptop on SmartNoticeBoard WiFi).
4. Convert the finished `index.html` into `dashboard.h` and serve it at `/`. Upload, test on iPhone.
5. Final full test checklist for the demo.
6. Later: enclosure, update PPT slides & report (remove cloud/Firebase), demo script for viva.

---

## 6. Known gotchas (already solved — don't repeat the mistakes)
- Sketchbook must stay `C:\Arduino` (non-ASCII path breaks the ESP32 compiler).
- First compile after adding a library is slow (2–4 min) — normal.
- If upload is stuck on "Connecting…", hold the **BOOT** button on the ESP32.
- Time showed ~4 min behind after upload (compile-time stamp) → fixed with "Sync clock with my phone".
- Resistors are **4-band**: read with gold band on the right. 1kΩ = brown-black-red; 10kΩ = brown-black-orange.
- Breadboard has no printed +/− lines; I marked the left outer strip column "G" as ground. Breadboard short lines = 5 holes across between edge strip and middle groove.
- The ESP32 is not on the breadboard (too wide) — all its connections are female sockets directly on its pins.
