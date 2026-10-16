// =====================================================================
//  Smart Notice Board - ESP32 firmware (Session 2)
//  Multiple notices, categories, rotation, scheduling, SOS mode,
//  non-blocking buzzer/LEDs and a JSON API for the dashboard.
// =====================================================================

// ---------- Libraries ----------
#include <WiFi.h>          // ESP32 makes its own WiFi hotspot
#include <WebServer.h>     // small web server (phone page + dashboard API)
#include <Preferences.h>   // saves notices/settings in flash (survive power off)
#include <TFT_eSPI.h>      // 3.5" colour display
#include <RTClib.h>        // DS3231 clock module
#include <ArduinoJson.h>   // reads/writes JSON for the dashboard
#include "qrcode.h"        // QR code maker (built into the ESP32 core, no install needed)

// ---------- Settings ----------
// WiFi hotspot name and password (phone/laptop connects to this)
const char* WIFI_NAME = "SmartNoticeBoard";
const char* WIFI_PASS = "12345678";
// Pins - same as your finished wiring (do not change)
const int BUTTON_PIN = 33;
const int BUZZER_PIN = 13;
const int GREEN_LED  = 16;   // RX2 pin
const int RED_LED    = 17;   // TX2 pin

// Limits: max notices stored, and max characters allowed
const int MAX_NOTICES = 10;
const int TITLE_MAX   = 40;
const int TEXT_MAX    = 300;
const int SOS_MAX     = 120;
const int QR_MAX      = 120;   // max length of a notice's QR link
const int IDLE_MAX    = 150;   // max length of the default message
// Message used when SOS is started by holding the physical button
const char* SOS_DEFAULT = "EMERGENCY - Please follow instructions";
// Default message shown when there are no notices (can be changed from the dashboard)
const char* IDLE_DEFAULT = "Welcome to Don Bosco Institute of Technology - Department of Information Technology";
// Size of the QR code square on screen (pixels)
const int QR_BOX = 140;

// Screen areas (480 x 320 landscape)
const int HEADER_H = 44;             // height of the navy top bar
const int FOOTER_H = 36;             // height of the grey bottom bar
const uint16_t FOOTER_BG = 0x2124;   // dark grey
const uint16_t LINE_GREY = 0x4208;   // thin line under the title

// Categories and their colours (same as the dashboard)
// General = grey, Exam = orange, Event = green, Placement = cyan, Urgent = red
const char* CATS[] = { "General", "Exam", "Event", "Placement", "Urgent" };
const uint16_t CAT_COL[] = { TFT_LIGHTGREY, TFT_ORANGE, TFT_GREEN, TFT_CYAN, TFT_RED };

// ---------- Hardware objects ----------
TFT_eSPI tft = TFT_eSPI();   // the display
WebServer server(80);        // web server on port 80 (normal web address)
Preferences prefs;           // flash storage
RTC_DS3231 rtc;              // the clock module
bool rtcOK = false;          // true if the clock module was found

// ---------- Notices ----------
// One notice = one "box" of information
struct Notice {
  bool used = false;                         // is this box holding a notice?
  int id = 0;                                // unique number for each notice
  String title, text, category, priority;    // what the notice says
  String qr;                                 // optional link shown as a QR code ("" = no QR)
  uint32_t start = 0, end = 0, created = 0;  // schedule times (0 = no limit)
};
Notice slots[MAX_NOTICES];   // 10 boxes for notices
int nextId = 1;              // number the next new notice will get
int phoneId = 0;             // the notice posted from the simple phone page
int rotateSec = 8;           // how many seconds each notice stays on screen
String idleMsg = IDLE_DEFAULT;   // default message when there are no notices

// Notices showing right now (slot numbers, in display order)
int act[MAX_NOTICES];
int actCount = 0;            // how many notices are showing now
String lastSig = "";         // remembers the list, to notice when it changes
int shownId = 0;             // id of the notice on screen (0 = none)
bool needRedraw = true;      // true = screen must be drawn again
unsigned long lastRotate = 0, lastTick = 0;   // timers for rotation and the clock

// ---------- SOS ----------
bool sosActive = false;      // is emergency mode on?
String sosMsg = "";          // emergency message shown on screen
bool sosFlash = false;       // swaps red/white for the flashing effect
unsigned long lastFlash = 0, lastSosBeep = 0;   // timers for flashing and beeping

// ---------- Clock backup (used only if the RTC is missing) ----------
uint32_t softBase = 0;
unsigned long softAt = 0;

// ---------- Buzzer (non-blocking) ----------
int bzLeft = 0, bzOnMs = 0, bzOffMs = 0;   // beeps left, beep length, gap length
bool bzOn = false;                         // is the buzzer sounding right now?
unsigned long bzNext = 0;                  // when to switch the buzzer next

// ---------- Button ----------
bool btnRaw = HIGH, btnStable = HIGH, longDone = false;
unsigned long btnChanged = 0, pressStart = 0;

// =====================================================================
//  Time
// =====================================================================
// FEATURE: read the current time (seconds) from the clock module
uint32_t nowEpoch() {
  if (rtcOK) return rtc.now().unixtime();
  return softBase + (millis() - softAt) / 1000;   // backup if no clock module
}

void drawClock();

// FEATURE: set the clock (used by "Sync clock" on phone page and dashboard)
void setClock(uint32_t t) {
  if (rtcOK) rtc.adjust(DateTime(t));
  softBase = t;
  softAt = millis();
  if (!sosActive) drawClock();   // show the new time right away
}

// =====================================================================
//  Buzzer and LEDs
// =====================================================================
// FEATURE: start a beep pattern (e.g. 2 beeps). Runs in the background,
// so the board keeps working while it beeps.
void startBeep(int times, int onMs, int offMs) {
  bzLeft = times; bzOnMs = onMs; bzOffMs = offMs;
  bzOn = false;
  digitalWrite(BUZZER_PIN, LOW);
  bzNext = millis();
}

// FEATURE: stop the buzzer immediately
void stopBeep() {
  bzLeft = 0; bzOn = false;
  digitalWrite(BUZZER_PIN, LOW);
}

// Called again and again from loop(): switches the buzzer on/off at the right time
void updateBuzzer() {
  if (!bzOn && bzLeft == 0) return;              // nothing to beep
  if ((long)(millis() - bzNext) < 0) return;     // not time yet
  if (bzOn) {                                    // beep finished -> silence
    digitalWrite(BUZZER_PIN, LOW);
    bzOn = false;
    bzLeft--;
    bzNext = millis() + bzOffMs;
  } else if (bzLeft > 0) {                       // gap finished -> next beep
    digitalWrite(BUZZER_PIN, HIGH);
    bzOn = true;
    bzNext = millis() + bzOnMs;
  }
}

// FEATURE: status LEDs
void updateLeds() {
  if (sosActive) {                       // SOS: red blinking
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, (millis() / 300) % 2);
  } else if (!rtcOK) {                   // problem: red solid
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
  } else {                               // normal: green on
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
  }
}

// =====================================================================
//  Text helpers
// =====================================================================
// FEATURE: text safety.
// The screen fonts only know plain English characters.
// This swaps smart quotes, dashes, Rupee sign etc. and removes the rest (emoji...).
String cleanText(const String& in, int maxLen) {
  String out = "";
  int n = in.length();
  for (int i = 0; i < n;) {
    uint8_t c = in[i];
    if (c == '\r') { i++; continue; }                          // remove
    if (c == '\t') { out += ' '; i++; continue; }              // tab -> space
    if (c == '\n') { out += '\n'; i++; continue; }             // keep new lines
    if (c >= 32 && c < 127) { out += (char)c; i++; continue; } // normal character
    if (c < 0xC0) { i++; continue; }
    // Special (multi-byte) character: replace the common ones, skip the rest
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : 2;
    uint8_t c1 = (i + 1 < n) ? in[i + 1] : 0;
    uint8_t c2 = (i + 2 < n) ? in[i + 2] : 0;
    if (c == 0xE2 && c1 == 0x80) {
      if (c2 == 0x98 || c2 == 0x99) out += '\'';        // smart single quotes
      else if (c2 == 0x9C || c2 == 0x9D) out += '"';    // smart double quotes
      else if (c2 == 0x93 || c2 == 0x94) out += '-';    // long dashes
      else if (c2 == 0xA6) out += "...";                // ... character
    } else if (c == 0xE2 && c1 == 0x82 && c2 == 0xB9) {
      out += "Rs ";                                     // Rupee sign
    } else if (c == 0xC2 && c1 == 0xA0) {
      out += ' ';                                       // non-breaking space
    }
    i += len;
  }
  out.trim();
  if ((int)out.length() > maxLen) out = out.substring(0, maxLen);   // cut to max length
  return out;
}

// Category helpers: find the category number (for its colour); unknown -> General
int catIndex(const String& c) {
  for (int i = 0; i < 5; i++) if (c.equalsIgnoreCase(CATS[i])) return i;
  return 0;
}
String cleanCat(const String& c) { return CATS[catIndex(c)]; }
// Priority helpers: only "urgent", "high" or "normal" are allowed
String cleanPrio(String p) {
  p.toLowerCase();
  return (p == "urgent" || p == "high") ? p : "normal";
}
// Sorting rank: urgent = 0 (first), high = 1, normal = 2
int prioRank(const String& p) { return p == "urgent" ? 0 : p == "high" ? 1 : 2; }

// FEATURE: scheduling - is the notice waiting, showing, or finished?
const char* stateOf(const Notice& n, uint32_t now) {
  if (n.start && n.start > now) return "scheduled";   // start time not reached yet
  if (n.end && now >= n.end) return "expired";        // end time has passed
  return "active";                                    // showing now
}
bool isActive(const Notice& n, uint32_t now) { return n.used && strcmp(stateOf(n, now), "active") == 0; }

// =====================================================================
//  Saving notices (each notice has its own key: n0 ... n9)
// =====================================================================
String slotKey(int i) { return "n" + String(i); }

// FEATURE: save one notice to flash (or delete it from flash if the box is empty)
void saveSlot(int i) {
  String key = slotKey(i);
  if (!slots[i].used) { prefs.remove(key.c_str()); return; }
  JsonDocument d;
  d["id"] = slots[i].id;
  d["title"] = slots[i].title;
  d["text"] = slots[i].text;
  d["category"] = slots[i].category;
  d["priority"] = slots[i].priority;
  d["qr"] = slots[i].qr;
  d["start"] = slots[i].start;
  d["end"] = slots[i].end;
  d["created"] = slots[i].created;
  String s;
  serializeJson(d, s);
  prefs.putString(key.c_str(), s);
}

// FEATURE: load all saved notices from flash at power on
void loadAll() {
  for (int i = 0; i < MAX_NOTICES; i++) {
    String key = slotKey(i);
    if (!prefs.isKey(key.c_str())) continue;   // nothing saved in this box
    JsonDocument d;
    if (deserializeJson(d, prefs.getString(key.c_str(), ""))) continue;   // skip broken data
    Notice& n = slots[i];
    n.used = true;
    n.id = d["id"].as<int>();
    n.title = d["title"].as<String>();
    n.text = d["text"].as<String>();
    n.category = cleanCat(d["category"].as<String>());
    n.priority = cleanPrio(d["priority"].as<String>());
    n.qr = d["qr"] | "";
    n.start = d["start"].as<uint32_t>();
    n.end = d["end"].as<uint32_t>();
    n.created = d["created"].as<uint32_t>();
    if (n.id >= nextId) nextId = n.id + 1;
  }
}

// Find which box holds the notice with this id (-1 = not found)
int findSlot(int id) {
  for (int i = 0; i < MAX_NOTICES; i++) if (slots[i].used && slots[i].id == id) return i;
  return -1;
}

// Free slot, or the oldest expired notice if the board is full
int freeSlot(uint32_t now) {
  for (int i = 0; i < MAX_NOTICES; i++) if (!slots[i].used) return i;
  int best = -1;
  for (int i = 0; i < MAX_NOTICES; i++) {
    if (strcmp(stateOf(slots[i], now), "expired") == 0 && (best < 0 || slots[i].end < slots[best].end)) best = i;
  }
  return best;
}

// FEATURE: delete one notice (from memory and flash)
void deleteSlot(int i) {
  if (slots[i].id == phoneId) { phoneId = 0; prefs.putInt("phone", 0); }
  slots[i] = Notice();
  saveSlot(i);
  needRedraw = true;
}

// FEATURE: add a new notice -> save it, show it on screen, 2 beeps
int addNotice(const String& title, const String& text, const String& cat, const String& prio,
              const String& qr, uint32_t start, uint32_t end) {
  uint32_t now = nowEpoch();
  int i = freeSlot(now);
  if (i < 0) return -1;                  // board is full
  if (slots[i].used) deleteSlot(i);      // reuse an expired notice's box
  Notice& n = slots[i];
  n.used = true;
  n.id = nextId++;
  n.title = title;
  n.text = text;
  n.category = cat;
  n.priority = prio;
  n.qr = qr;
  n.start = start;
  n.end = end;
  n.created = now;
  prefs.putInt("seq", nextId);
  saveSlot(i);
  if (isActive(n, now)) { shownId = n.id; lastRotate = millis(); }   // show it right away
  needRedraw = true;
  startBeep(2, 120, 80);                 // 2 beeps = new notice
  return i;
}

// =====================================================================
//  Which notices are showing now, and in what order
// =====================================================================
// FEATURE: display order - urgent first, then high, then normal; newest first
bool comesBefore(int x, int y) {
  int rx = prioRank(slots[x].priority), ry = prioRank(slots[y].priority);
  if (rx != ry) return rx < ry;                                  // urgent first
  if (slots[x].created != slots[y].created) return slots[x].created > slots[y].created;  // newest first
  return slots[x].id > slots[y].id;
}

// Builds the list of notices showing right now, sorted in display order
String computeActive() {
  uint32_t now = nowEpoch();
  actCount = 0;
  for (int i = 0; i < MAX_NOTICES; i++) if (isActive(slots[i], now)) act[actCount++] = i;
  for (int a = 1; a < actCount; a++) {            // sort the list
    int k = act[a], b = a - 1;
    while (b >= 0 && comesBefore(k, act[b])) { act[b + 1] = act[b]; b--; }
    act[b + 1] = k;
  }
  String sig = "";
  for (int i = 0; i < actCount; i++) sig += String(slots[act[i]].id) + ",";
  return sig;
}

// FEATURE: rotation - move to the next notice (used by timer, button and dashboard)
void nextNotice() {
  computeActive();
  if (actCount == 0) return;
  int pos = -1;
  for (int i = 0; i < actCount; i++) if (slots[act[i]].id == shownId) pos = i;
  pos = (pos + 1) % actCount;          // after the last one, go back to the first
  shownId = slots[act[pos]].id;
  lastRotate = millis();
  needRedraw = true;
}

// =====================================================================
//  Drawing on the screen
// =====================================================================
// Draws word-wrapped text from (x, y), stopping before maxY.
// With draw = false it only measures. Returns the y below the last line.
int wrapText(const String& text, int x, int y, int maxW, int maxY, int font, bool draw) {
  int lineH = tft.fontHeight(font) + 4;
  int n = text.length();
  String line = "";
  int i = 0;
  while (i <= n) {
    // Take the next word
    int j = i;
    while (j < n && text[j] != ' ' && text[j] != '\n') j++;
    String word = text.substring(i, j);
    String test = line.length() ? line + " " + word : word;
    // Word does not fit -> print the line and start a new one
    if (line.length() && tft.textWidth(test, font) > maxW) {
      if (y + lineH > maxY) return y;
      if (draw) tft.drawString(line, x, y, font);
      y += lineH;
      line = word;
    } else {
      line = test;
    }
    // A very long word: cut it into pieces that fit
    while (line.length() > 1 && tft.textWidth(line, font) > maxW) {
      int k = line.length() - 1;
      while (k > 1 && tft.textWidth(line.substring(0, k), font) > maxW) k--;
      if (y + lineH > maxY) return y;
      if (draw) tft.drawString(line.substring(0, k), x, y, font);
      y += lineH;
      line = line.substring(k);
    }
    // End of text or a new line (Enter) -> print what is left
    if (j >= n || text[j] == '\n') {
      if (line.length() || j < n) {
        if (y + lineH > maxY) return y;
        if (draw) tft.drawString(line, x, y, font);
        y += lineH;
      }
      line = "";
    }
    i = j + 1;
  }
  return y;
}

// FEATURE: clock in the top-right corner (only this small area is redrawn - no flicker)
void drawClock() {
  char buf[24];
  DateTime now(nowEpoch());
  sprintf(buf, "%02d:%02d:%02d  %02d/%02d", now.hour(), now.minute(), now.second(), now.day(), now.month());
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(TR_DATUM);
  tft.setTextPadding(tft.textWidth("88:88:88  88/88", 4));
  tft.drawString(buf, tft.width() - 10, 10, 4);
  tft.setTextPadding(0);
  tft.setTextDatum(TL_DATUM);
}

// SCREEN: navy header bar with "NOTICE BOARD" and the clock
void drawHeader() {
  tft.fillRect(0, 0, tft.width(), HEADER_H, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("NOTICE BOARD", 12, 10, 4);
  drawClock();
}

// SCREEN: grey footer bar - WiFi info on the left, "2 / 5" page number on the right
void drawFooter(const String& right) {
  int y = tft.height() - FOOTER_H;
  tft.fillRect(0, y, tft.width(), FOOTER_H, FOOTER_BG);
  tft.setTextColor(TFT_WHITE, FOOTER_BG);
  tft.drawString("WiFi: " + String(WIFI_NAME) + "   |   192.168.4.1", 12, y + 10, 2);
  if (right.length()) {
    tft.setTextDatum(TR_DATUM);
    tft.drawString(right, tft.width() - 12, y + 5, 4);
    tft.setTextDatum(TL_DATUM);
  }
}

// Clears the middle area (between header and footer)
void clearBody() {
  tft.fillRect(0, HEADER_H, tft.width(), tft.height() - HEADER_H - FOOTER_H, TFT_BLACK);
}

// SCREEN: default message (when there are no notices) + small staff instructions
void drawEmpty() {
  int W = tft.width();
  clearBody();
  int bottom = tft.height() - FOOTER_H - 52;    // space kept for the 2 small lines

  // Default message: big font if it fits, otherwise smaller; centred on screen
  int font = (wrapText(idleMsg, W / 2, 0, W - 40, 100000, 4, false) <= bottom - HEADER_H - 20) ? 4 : 2;
  int h = wrapText(idleMsg, W / 2, 0, W - 40, 100000, font, false);
  int y = HEADER_H + max(16, (bottom - HEADER_H - h) / 2);
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  wrapText(idleMsg, W / 2, y, W - 40, bottom, font, true);

  // Small grey instructions for staff
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Staff: join WiFi " + String(WIFI_NAME) + " (password " + String(WIFI_PASS) + ")", W / 2, bottom + 8, 2);
  tft.drawString("then open 192.168.4.1 to post notices", W / 2, bottom + 28, 2);
  tft.setTextDatum(TL_DATUM);
  drawFooter("");
}

// ---------- QR code ----------
int qrX = 0, qrY = 0;   // where the QR square goes
bool qrDone = false;    // true if the QR was drawn

// Called by the QR maker: draws the black and white squares on the screen
void qrDraw(esp_qrcode_handle_t q) {
  int size = esp_qrcode_get_size(q);
  int scale = QR_BOX / (size + 4);              // pixel size of one square (+ white border)
  if (scale < 1) scale = 1;
  int total = (size + 4) * scale;
  int ox = qrX + (QR_BOX - total) / 2, oy = qrY + (QR_BOX - total) / 2;
  tft.fillRect(ox, oy, total, total, TFT_WHITE);  // white background + border
  ox += 2 * scale; oy += 2 * scale;
  for (int y = 0; y < size; y++) {
    int x = 0;
    while (x < size) {                          // draw each row in black runs (faster)
      if (!esp_qrcode_get_module(q, x, y)) { x++; continue; }
      int run = x;
      while (run < size && esp_qrcode_get_module(q, run, y)) run++;
      tft.fillRect(ox + x * scale, oy + y * scale, (run - x) * scale, scale, TFT_BLACK);
      x = run;
    }
  }
  qrDone = true;
}

// FEATURE: draw a QR code for "link" with its top-left corner at (x, y)
void drawQr(const String& link, int x, int y) {
  qrX = x; qrY = y; qrDone = false;
  esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
  cfg.display_func = qrDraw;
  esp_qrcode_generate(&cfg, link.c_str());
  tft.setTextDatum(TC_DATUM);
  if (qrDone) {
    tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    tft.drawString("Scan me", x + QR_BOX / 2, y + QR_BOX + 6, 2);
  } else {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Link too long", x + QR_BOX / 2, y + QR_BOX / 2, 2);
  }
  tft.setTextDatum(TL_DATUM);
}

// SCREEN: one notice - category tag, priority, title, message, QR, page number
void drawNotice(int i, int pos, int count) {
  Notice& n = slots[i];
  int W = tft.width();
  clearBody();

  // With a QR code the text gets narrower to make room on the right
  bool hasQr = n.qr.length() > 0;
  int textW = W - 24 - (hasQr ? QR_BOX + 14 : 0);
  if (hasQr) drawQr(n.qr, W - 12 - QR_BOX, 84);

  // Category tag (coloured box, e.g. "EXAM")
  int c = catIndex(n.category);
  uint16_t col = CAT_COL[c];
  uint16_t tagText = (c == 4) ? TFT_WHITE : TFT_BLACK;
  String tag = n.category;
  tag.toUpperCase();
  int tw = tft.textWidth(tag, 2) + 16;
  tft.fillRoundRect(12, 54, tw, 22, 4, col);
  tft.setTextColor(tagText, col);
  tft.drawString(tag, 20, 57, 2);

  // Priority label next to the tag ("URGENT" red / "HIGH PRIORITY" orange)
  if (n.priority == "urgent") {
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("URGENT", 12 + tw + 10, 57, 2);
  } else if (n.priority == "high") {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.drawString("HIGH PRIORITY", 12 + tw + 10, 57, 2);
  }

  // Title (max 2 lines)
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  int y = wrapText(n.title, 12, 84, textW, 84 + 2 * (tft.fontHeight(4) + 4), 4, true);
  tft.drawFastHLine(12, y + 2, textW, LINE_GREY);
  y += 10;

  // Message: big font if it fits, otherwise smaller font
  int bottom = tft.height() - FOOTER_H - 4;
  int font = (wrapText(n.text, 12, y, textW, 100000, 4, false) <= bottom) ? 4 : 2;
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  wrapText(n.text, 12, y, textW, bottom, font, true);

  // Footer with page number, e.g. "2 / 5"
  drawFooter(String(pos + 1) + " / " + String(count));
}

// SCREEN: draws the whole normal screen (header + notice or "No notice yet")
void drawScreen() {
  needRedraw = false;
  lastSig = computeActive();
  drawHeader();
  if (actCount == 0) { shownId = 0; drawEmpty(); return; }
  int pos = -1;
  for (int i = 0; i < actCount; i++) if (slots[act[i]].id == shownId) pos = i;
  if (pos < 0) { pos = 0; shownId = slots[act[0]].id; lastRotate = millis(); }
  drawNotice(act[pos], pos, actCount);
}

// SCREEN: SOS emergency screen - flashes red/white with big text
void drawSos() {
  uint16_t bg = sosFlash ? TFT_WHITE : TFT_RED;
  uint16_t fg = sosFlash ? TFT_RED : TFT_WHITE;
  int W = tft.width();
  tft.fillScreen(bg);
  tft.setTextColor(fg, bg);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);                                       // double size text
  tft.drawString("! EMERGENCY !", W / 2, 16, 4);
  tft.drawFastHLine(20, 80, W - 40, fg);
  tft.setTextSize(sosMsg.length() <= 50 ? 2 : 1);           // long message -> normal size
  wrapText(sosMsg, W / 2, 96, W - 30, tft.height() - 6, 4, true);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
}

// =====================================================================
//  SOS
// =====================================================================
// FEATURE: turn SOS ON - flashing screen, 3 beeps, red LED blinking
void startSos(const String& msg) {
  sosActive = true;
  sosMsg = msg.length() ? msg : String(SOS_DEFAULT);
  sosFlash = false;
  lastFlash = 0;
  lastSosBeep = millis();
  startBeep(3, 150, 100);
}

// FEATURE: turn SOS OFF - one long beep, back to the normal notice screen
void stopSos() {
  if (!sosActive) return;
  sosActive = false;
  sosMsg = "";
  stopBeep();
  startBeep(1, 200, 0);
  needRedraw = true;
}

// =====================================================================
//  Button (short press = next notice, hold 3 s = SOS on/off)
// =====================================================================
// BUTTON: physical push button on pin 33
void checkButton() {
  bool r = digitalRead(BUTTON_PIN);
  if (r != btnRaw) { btnRaw = r; btnChanged = millis(); }
  if (millis() - btnChanged < 30) return;     // debounce

  if (r != btnStable) {
    btnStable = r;
    if (r == LOW) {                            // button pushed down: start timing
      pressStart = millis();
      longDone = false;
    } else if (!longDone) {                    // short press released
      // SHORT PRESS: 1 short beep + show the next notice
      startBeep(1, 60, 0);
      if (!sosActive) nextNotice();
    }
  }
  // LONG PRESS (held 3 seconds): SOS on, or SOS off if it is already on
  if (btnStable == LOW && !longDone && millis() - pressStart >= 3000) {
    longDone = true;
    if (sosActive) stopSos();
    else startSos(SOS_DEFAULT);
  }
}

// =====================================================================
//  Simple phone page (unchanged look)
// =====================================================================
// Makes text safe to put inside the web page
String htmlEscape(String s) {
  s.replace("&", "&amp;"); s.replace("<", "&lt;"); s.replace(">", "&gt;");
  return s;
}

// Text of the phone page's own notice (to fill the text box)
String phoneText() {
  int i = findSlot(phoneId);
  return i >= 0 ? slots[i].text : "";
}

// PAGE: the simple phone page at 192.168.4.1
void handleRoot() {
  String page =
    "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>Smart Notice Board</title><style>"
    "body{font-family:sans-serif;max-width:480px;margin:20px auto;padding:0 16px}"
    "textarea{width:100%;height:140px;font-size:16px;box-sizing:border-box}"
    "button{width:100%;padding:12px;font-size:18px;margin-top:8px}"
    "</style></head><body><h2>Smart Notice Board</h2>"
    // Text box + "Show on board" button
    "<form method='POST' action='/post'>"
    "<textarea name='msg' maxlength='300' placeholder='Type your notice here'>";
  page += htmlEscape(phoneText());
  page += "</textarea><button type='submit'>Show on board</button></form>"
          // "Clear notice" button
          "<form method='POST' action='/clear'><button type='submit'>Clear notice</button></form>"
          // "Sync clock with my phone" button (sends the phone's time)
          "<button onclick=\"fetch('/settime?t='+Math.floor((Date.now()-new Date().getTimezoneOffset()*60000)/1000)).then(()=>alert('Clock synced'))\">Sync clock with my phone</button>"
          "</body></html>";
  server.send(200, "text/html", page);
}

// The phone page controls ONE notice of its own (title "Notice")
void saveAndShow(const String& raw) {
  String text = cleanText(raw, TEXT_MAX);
  int i = findSlot(phoneId);
  if (text.length() == 0) {
    // Empty text -> delete the phone notice
    if (i >= 0) deleteSlot(i);
  } else if (i >= 0) {
    // Phone notice already exists -> change its text and show it
    slots[i].text = text;
    saveSlot(i);
    shownId = phoneId;
    lastRotate = millis();
    needRedraw = true;
    startBeep(2, 120, 80);
  } else {
    // No phone notice yet -> create one
    int s = addNotice("Notice", text, "General", "normal", "", 0, 0);
    if (s >= 0) { phoneId = slots[s].id; prefs.putInt("phone", phoneId); }
  }
  // Go back to the phone page
  server.sendHeader("Location", "/");
  server.send(303);
}

// PHONE BUTTON: "Show on board"
void handlePost()  { saveAndShow(server.arg("msg")); }
// PHONE BUTTON: "Clear notice"
void handleClear() { saveAndShow(""); }

// PHONE BUTTON: "Sync clock with my phone"  (/settime?t=...)
void handleSetTime() {
  if (server.hasArg("t")) {
    setClock(strtoul(server.arg("t").c_str(), nullptr, 10));
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing time");
  }
}

// =====================================================================
//  JSON API for the dashboard
// =====================================================================
// CORS: allows the dashboard opened from the laptop (VS Code) to talk to the board
void addCors() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
}

// Send a JSON answer to the dashboard
void sendJson(int code, const String& body) {
  addCors();
  server.send(code, "application/json", body);
}

// Send an error message to the dashboard (shown as a red pop-up there)
void sendError(int code, const char* msg) {
  JsonDocument d;
  d["error"] = msg;
  String s;
  serializeJson(d, s);
  sendJson(code, s);
}

// Send {"ok": true} = "done"
void sendOk() { sendJson(200, "{\"ok\":true}"); }

// Read the JSON sent by the dashboard
bool readBody(JsonDocument& d) {
  if (!server.hasArg("plain")) return false;
  return !deserializeJson(d, server.arg("plain"));
}

// Put one notice's details into JSON
void fillNotice(JsonObject o, int i, uint32_t now) {
  Notice& n = slots[i];
  o["id"] = n.id;
  o["title"] = n.title;
  o["text"] = n.text;
  o["category"] = n.category;
  o["priority"] = n.priority;
  o["qr"] = n.qr;
  o["start"] = n.start;
  o["end"] = n.end;
  o["created"] = n.created;
  o["state"] = stateOf(n, now);
}

// Send one notice back to the dashboard
void sendNotice(int i) {
  JsonDocument d;
  fillNotice(d.to<JsonObject>(), i, nowEpoch());
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

// GET /api/status
// DASHBOARD: top bar clock, summary cards, "System" panel and SOS banner
// (asked every 4 seconds by the dashboard)
void handleStatus() {
  uint32_t now = nowEpoch();
  DateTime t(now);
  char buf[24];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
  int total = 0, active = 0;
  for (int i = 0; i < MAX_NOTICES; i++) {
    if (!slots[i].used) continue;
    total++;
    if (isActive(slots[i], now)) active++;
  }
  JsonDocument d;
  d["time"] = buf;                              // board time as text
  d["epoch"] = now;                             // board time as number
  d["rtc"] = rtcOK;                             // clock module OK?
  d["uptime"] = millis() / 1000;                // seconds since power on
  d["heap"] = ESP.getFreeHeap();                // free memory
  d["clients"] = WiFi.softAPgetStationNum();    // devices connected to the WiFi
  d["notices"] = total;
  d["active"] = active;
  d["rotate"] = rotateSec;
  d["idle"] = idleMsg;                          // default message
  d["showing"] = shownId;
  d["sos"]["active"] = sosActive;
  d["sos"]["message"] = sosMsg;
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

// GET /api/notices
// DASHBOARD: the notices list (all notices with their status)
void handleGetNotices() {
  uint32_t now = nowEpoch();
  JsonDocument d;
  JsonArray arr = d.to<JsonArray>();
  for (int i = 0; i < MAX_NOTICES; i++) {
    if (slots[i].used) fillNotice(arr.add<JsonObject>(), i, now);
  }
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

// POST /api/notices
// DASHBOARD BUTTON: "Post to board" (new notice form)
void handleAddNotice() {
  JsonDocument d;
  if (!readBody(d)) return sendError(400, "Bad request");
  String title = cleanText(d["title"] | "", TITLE_MAX);
  String text = cleanText(d["text"] | "", TEXT_MAX);
  if (!title.length() || !text.length()) return sendError(400, "Title and message are required");
  uint32_t start = d["start"].as<uint32_t>();
  uint32_t end = d["end"].as<uint32_t>();
  if (start && end && end <= start) return sendError(400, "End time must be after start time");
  String qr = cleanText(d["qr"] | "", QR_MAX);
  int i = addNotice(title, text, cleanCat(d["category"] | "General"), cleanPrio(d["priority"] | "normal"), qr, start, end);
  if (i < 0) return sendError(400, "Board is full (10 notices). Delete one first.");
  sendNotice(i);
}

// PUT /api/notices?id=3
// DASHBOARD BUTTON: "Save changes" (after clicking Edit on a notice)
void handleUpdateNotice() {
  int i = findSlot(server.arg("id").toInt());
  if (i < 0) return sendError(404, "Notice not found");
  JsonDocument d;
  if (!readBody(d)) return sendError(400, "Bad request");
  Notice n = slots[i];
  // Change only the details the dashboard sent
  if (!d["title"].isNull()) n.title = cleanText(d["title"].as<String>(), TITLE_MAX);
  if (!d["text"].isNull()) n.text = cleanText(d["text"].as<String>(), TEXT_MAX);
  if (!d["category"].isNull()) n.category = cleanCat(d["category"].as<String>());
  if (!d["priority"].isNull()) n.priority = cleanPrio(d["priority"].as<String>());
  if (!d["qr"].isNull()) n.qr = cleanText(d["qr"].as<String>(), QR_MAX);
  if (!d["start"].isNull()) n.start = d["start"].as<uint32_t>();
  if (!d["end"].isNull()) n.end = d["end"].as<uint32_t>();
  if (!n.title.length() || !n.text.length()) return sendError(400, "Title and message are required");
  if (n.start && n.end && n.end <= n.start) return sendError(400, "End time must be after start time");
  slots[i] = n;
  saveSlot(i);
  if (isActive(n, nowEpoch())) { shownId = n.id; lastRotate = millis(); }   // show the edited notice
  needRedraw = true;
  sendNotice(i);
}

// DELETE /api/notices?id=3
// DASHBOARD BUTTON: Delete (bin icon on a notice)
void handleDeleteNotice() {
  int i = findSlot(server.arg("id").toInt());
  if (i < 0) return sendError(404, "Notice not found");
  deleteSlot(i);
  sendOk();
}

// POST /api/notices/clear
// DASHBOARD BUTTON: "Clear all"
void handleClearNotices() {
  for (int i = 0; i < MAX_NOTICES; i++) if (slots[i].used) deleteSlot(i);
  sendOk();
}

// POST /api/next
// DASHBOARD BUTTON: "Show next notice"
void handleNext() {
  if (!sosActive) { nextNotice(); startBeep(1, 60, 0); }
  sendOk();
}

// POST /api/sos   { "message": "..." }
// DASHBOARD BUTTON: "Start SOS" (in the emergency pop-up)
void handleSos() {
  JsonDocument d;
  readBody(d);
  startSos(cleanText(d["message"] | "", SOS_MAX));
  sendOk();
}

// POST /api/sos/clear
// DASHBOARD BUTTON: "Clear SOS"
void handleSosClear() {
  stopSos();
  sendOk();
}

// POST /api/time   { "t": <local epoch> }
// DASHBOARD BUTTON: "Sync clock with this laptop"
void handleTime() {
  JsonDocument d;
  if (!readBody(d)) return sendError(400, "Bad request");
  uint32_t t = d["t"].as<uint32_t>();
  if (t < 1600000000UL) return sendError(400, "Bad time");
  setClock(t);
  sendOk();
}

// GET / POST /api/settings   { "rotate": 8, "idle": "Welcome..." }
// Sends the current settings (rotation seconds + default message)
void sendSettings() {
  JsonDocument d;
  d["rotate"] = rotateSec;
  d["idle"] = idleMsg;
  String s;
  serializeJson(d, s);
  sendJson(200, s);
}

// DASHBOARD BUTTON: "Save" next to "Rotate every ... sec"
void handleSaveSettings() {
  JsonDocument d;
  if (!readBody(d)) return sendError(400, "Bad request");
  if (!d["rotate"].isNull()) {
    int r = d["rotate"].as<int>();
    if (r < 3 || r > 60) return sendError(400, "Rotation must be 3 to 60 seconds");
    rotateSec = r;
    prefs.putInt("rotate", rotateSec);   // remember it after power off
    lastRotate = millis();
  }
  // DASHBOARD BUTTON: "Save" under "Default message"
  if (!d["idle"].isNull()) {
    String m = cleanText(d["idle"].as<String>(), IDLE_MAX);
    if (!m.length()) return sendError(400, "Default message cannot be empty");
    idleMsg = m;
    prefs.putString("idle", idleMsg);    // remember it after power off
    needRedraw = true;                   // update the screen if it is showing
  }
  sendSettings();
}

// Browsers ask "OPTIONS" first before sending JSON from another page (CORS)
// Any other unknown address -> "Not found"
void handleNotFound() {
  if (server.method() == HTTP_OPTIONS) {
    addCors();
    server.send(204);
  } else {
    sendError(404, "Not found");
  }
}

// =====================================================================
//  Setup and loop
// =====================================================================
// SETUP: runs once at power on
void setup() {
  Serial.begin(115200);
  // Pins for buzzer, button and LEDs
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  // Start the display (landscape)
  tft.init();
  tft.setRotation(1);

  // Clock first (notices need the time)
  softBase = DateTime(F(__DATE__), F(__TIME__)).unixtime();
  softAt = millis();
  rtcOK = rtc.begin();
  if (rtcOK && rtc.lostPower()) rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  updateLeds();

  // Load saved notices and settings
  prefs.begin("board", false);
  nextId = prefs.getInt("seq", 1);
  phoneId = prefs.getInt("phone", 0);
  rotateSec = constrain(prefs.getInt("rotate", 8), 3, 60);
  if (prefs.isKey("idle")) idleMsg = prefs.getString("idle", IDLE_DEFAULT);
  loadAll();
  if (findSlot(phoneId) < 0) phoneId = 0;

  // Move the old single notice (from the first version) into the new list
  if (prefs.isKey("notice")) {
    String old = cleanText(prefs.getString("notice", ""), TEXT_MAX);
    prefs.remove("notice");
    if (old.length()) {
      int s = addNotice("Notice", old, "General", "normal", "", 0, 0);
      if (s >= 0) { phoneId = slots[s].id; prefs.putInt("phone", phoneId); }
    }
  }

  // WiFi hotspot and web server
  WiFi.softAP(WIFI_NAME, WIFI_PASS);
  // Simple phone page and its buttons
  server.on("/", handleRoot);
  server.on("/post", HTTP_POST, handlePost);
  server.on("/clear", HTTP_POST, handleClear);
  server.on("/settime", handleSetTime);

  // Dashboard API (one line per dashboard feature/button)
  server.on("/api/status", HTTP_GET, handleStatus);                 // status + clock + SOS banner
  server.on("/api/notices", HTTP_GET, handleGetNotices);            // notices list
  server.on("/api/notices", HTTP_POST, handleAddNotice);            // "Post to board"
  server.on("/api/notices", HTTP_PUT, handleUpdateNotice);          // "Save changes"
  server.on("/api/notices", HTTP_DELETE, handleDeleteNotice);       // Delete
  server.on("/api/notices/clear", HTTP_POST, handleClearNotices);   // "Clear all"
  server.on("/api/next", HTTP_POST, handleNext);                    // "Show next notice"
  server.on("/api/sos", HTTP_POST, handleSos);                      // "Start SOS"
  server.on("/api/sos/clear", HTTP_POST, handleSosClear);           // "Clear SOS"
  server.on("/api/time", HTTP_POST, handleTime);                    // "Sync clock"
  server.on("/api/settings", HTTP_GET, sendSettings);               // read rotation speed
  server.on("/api/settings", HTTP_POST, handleSaveSettings);        // "Save" rotation speed / default message
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Smart Notice Board ready at 192.168.4.1");
  drawScreen();          // first screen
  lastRotate = millis();
  startBeep(1, 80, 0);   // 1 beep = board is ready
}

// LOOP: runs again and again, very fast (nothing here waits/blocks)
void loop() {
  server.handleClient();   // answer phone page / dashboard
  checkButton();           // physical button
  updateBuzzer();          // beeping in the background
  updateLeds();            // green / red LEDs

  // SOS MODE: flash the screen and beep, skip the normal screen
  if (sosActive) {
    if (lastFlash == 0 || millis() - lastFlash >= 700) {   // flash red / white
      lastFlash = millis();
      sosFlash = !sosFlash;
      drawSos();
    }
    if (millis() - lastSosBeep >= 5000) {                  // 3 beeps every 5 s
      lastSosBeep = millis();
      startBeep(3, 150, 100);
    }
    return;
  }

  // NORMAL MODE: every second update the clock and check schedules
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    drawClock();
    if (computeActive() != lastSig) needRedraw = true;    // a notice started or expired
  }
  // ROTATION: next notice every "rotateSec" seconds (only if more than 1 notice)
  if (actCount > 1 && millis() - lastRotate >= (unsigned long)rotateSec * 1000UL) nextNotice();
  // Redraw the screen only when something changed (avoids flicker)
  if (needRedraw) drawScreen();
}
