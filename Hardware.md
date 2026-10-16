# Smart Notice Board – Hardware Build Guide

**Project:** Smart Notice Board (IoE Lab Mini Project)
**Institute:** Don Bosco Institute of Technology, Dept. of Information Technology – BE Sem VII (AY 2026–27)
**Team:** Fauzan Ansari (02), Soham Dhakras (17), Sabiq Khot (30), Rayan Shaikh (51)

---

## 1. Overview

The Smart Notice Board is a standalone digital notice board built around an **ESP32** microcontroller. The ESP32 creates its own WiFi hotspot, and staff connect a phone or laptop to it and open a web page at `192.168.4.1` to post notices. Notices appear on a **3.5" colour TFT display**, together with a live clock.

Supporting hardware:

- **DS3231 RTC** keeps accurate time, even through power cuts (coin-cell backup).
- **Push button**: short press shows the next notice; a 3-second hold triggers SOS.
- **Buzzer** gives audio alerts for new notices and emergencies.
- **Green / red LEDs** show the board status (normal / alert).

No internet, cloud service, battery or soldering is required. The board runs from a USB cable (laptop or 5V phone charger).

```
          Phone / Laptop (browser, WiFi)
                      │
                      ▼   WiFi hotspot "SmartNoticeBoard"
 ┌──────────────────────────────────────────────┐
 │                   ESP32                      │
 │  SPI ──► 3.5" TFT Display (ILI9486 + touch)  │
 │  I2C ──► DS3231 RTC (time)                   │
 │  GPIO ◄─ Push button                         │
 │  GPIO ──► Buzzer                             │
 │  GPIO ──► Green LED / Red LED                │
 └──────────────────────────────────────────────┘
                      ▲
                 USB 5V power
```

---

## 2. Equipment Used

### 2.1 Main Components

| # | Component | Model / Specification | Qty | Purpose | Approx. Cost (₹) |
|---|---|---|---|---|---|
| 1 | Microcontroller | ESP32 DevKit (ESP-WROOM-32), 30-pin, Micro-USB | 1 | Main controller, built-in WiFi, runs the web server and display logic | 350–450 |
| 2 | Display | 3.5" "RPi Display" TFT, 480×320, ILI9486 driver, XPT2046 touch controller, 26-pin female header | 1 | Shows notices, clock and alerts | 700–1,000 |
| 3 | Real Time Clock | DS3231 RTC module (pins pre-soldered) | 1 | Keeps accurate date and time for the clock and scheduling | 150–250 |
| 4 | Coin cell | CR2032, 3V lithium | 1 | Backup power for the RTC when the board is unplugged | 20–40 |
| 5 | Push button | 6 mm tactile switch, 4-leg, momentary | 1 (+1 spare) | Next notice / SOS trigger | 10–20 |
| 6 | Buzzer | Active buzzer, 5V (works at 3.3V) | 1 | Audio alerts | 20–40 |
| 7 | LED – Green | 5 mm | 1 (+1 spare) | "System OK" indicator | 5 |
| 8 | LED – Red | 5 mm | 1 (+1 spare) | "SOS / problem" indicator | 5 |
| 9 | Resistors | 1 kΩ, ¼ W (4-band: brown-black-red-gold) | 2 | Current limiting for the LEDs | 10 |

### 2.2 Wiring and Accessories

| # | Item | Specification | Qty | Purpose |
|---|---|---|---|---|
| 10 | Breadboard | 830 tie-point | 1 | Holds the button and LEDs; provides a shared ground strip |
| 11 | Jumper wires | Male–Female, 20 cm | ~15 | ESP32 pins ↔ display / breadboard |
| 12 | Jumper wires | Female–Female, 20 cm | ~5 | ESP32 pins ↔ RTC / buzzer |
| 13 | Jumper wires | Male–Male, 20 cm | ~3 | Breadboard connections |
| 14 | USB cable | Micro-USB **data** cable | 1 | Programming and power |
| 15 | Power supply | Laptop USB port or 5V ≥1A phone charger | 1 | Powers the whole board |
| 16 | Enclosure *(final stage)* | ABS box approx. 150×100×70 mm | 1 | Housing with a cut-out for the screen |

### 2.3 Tools

- Laptop with **Arduino IDE 2.x** (ESP32 board package by Espressif installed)
- Small piece of tape and a pen (for labelling the ground strip and resistor strips)
- *Optional:* multimeter (continuity checks)

**Approximate total cost:** ₹1,300 – ₹2,000 (several parts reused from an earlier project)

### 2.4 Parts Considered but Not Used

| Part | Reason dropped |
|---|---|
| 4-channel relay module | No high-power load needed |
| HC-SR04 ultrasonic sensor | Optional "motion wake" feature skipped to keep the build simple |
| 18650 battery + TP4056 charger + MT3608 boost converter | Battery backup needs soldering; USB power is enough |
| BC547 transistor | Buzzer drawn current is small enough to drive directly from a GPIO |
| 100 nF capacitors | Button debouncing is done in software |
| 16×2 / 20×4 character LCD | Replaced by a full colour TFT screen |

---

## 3. Final Pin Map

| Device | Device pin | ESP32 pin | GPIO | Wire type |
|---|---|---|---|---|
| Display | Hole 2 (5V) | **VIN** | – | Male–Female |
| Display | Hole 6 (GND) | **GND** (next to 3V3) | – | Male–Female |
| Display | Hole 19 (MOSI) | D23 | 23 | Male–Female |
| Display | Hole 21 (MISO) | D19 | 19 | Male–Female |
| Display | Hole 23 (SCLK) | D18 | 18 | Male–Female |
| Display | Hole 24 (LCD CS) | D15 | 15 | Male–Female |
| Display | Hole 18 (DC / RS) | D2 | 2 | Male–Female |
| Display | Hole 22 (RESET) | D4 | 4 | Male–Female |
| Display | Hole 26 (Touch CS) | D5 | 5 | Male–Female |
| DS3231 RTC | VCC | **3V3** | – | Female–Female |
| DS3231 RTC | SDA | D21 | 21 | Female–Female |
| DS3231 RTC | SCL | D22 | 22 | Female–Female |
| DS3231 RTC | GND | G strip | – | Male–Female |
| Push button | Diagonal leg 1 | D33 | 33 | Male–Female |
| Push button | Diagonal leg 2 | G strip | – | Male–Male |
| Buzzer | + (long leg) | D13 | 13 | Female–Female |
| Buzzer | − (short leg) | G strip | – | Male–Female |
| Green LED | Long leg (+) | RX2 | 16 | Male–Female |
| Green LED | Short leg (−) → 1 kΩ → | G strip | – | resistor |
| Red LED | Long leg (+) | TX2 | 17 | Male–Female |
| Red LED | Short leg (−) → 1 kΩ → | G strip | – | resistor |
| **G strip** (breadboard ground) | – | **GND** (next to VIN) | – | Male–Female |

**Free GPIOs for future use:** 14, 25, 26, 27, 32. Avoid GPIO 12 (boot pin). GPIOs 34–39 are input-only.

---

## 4. Important Notes Before Building

1. **Always unplug the USB cable** before adding, moving or removing any wire.
2. The ESP32 is **too wide for the breadboard**: on one side it covers all the holes. It is therefore kept **off the breadboard**, and every connection goes directly onto its pins using **female** jumper sockets.
3. **Breadboard basics** (for this build):
   - The **two thin outer strips** run the full length; every hole in one column is connected. One outer column is used as the **shared ground ("G") strip**.
   - The **two wide middle sections** are made of **short lines of 5 holes** running *across* (from the edge strip to the middle groove). The 5 holes in a short line are connected; holes in neighbouring lines are **not**.
   - The **middle groove** separates the two halves.
4. **LED polarity:** long leg = +, short leg = −. A reversed LED will not light (but is not damaged).
5. **Resistors** have no polarity. They are 4-band: read with the **gold band on the right**. 1 kΩ = brown-black-red.
6. The display's **backlight is always on** once powered. A plain white screen means power is OK and it is waiting for code.

---

## 5. Step-by-Step Hardware Build

### Step 1 – Prepare the ESP32

1. Identify the two rows of pins. Pin names are printed next to each pin.
   - **VIN row** (from the USB end): VIN, GND, D13, D12, D14, D27, D26, D25, D33, D32, D35, D34, VN, VP, EN
   - **D-pin row** (from the USB end): 3V3, GND, D15, D2, D4, RX2, TX2, D5, D18, D19, D21, RX0, TX0, D22, D23
2. Connect the ESP32 to the laptop with the Micro-USB data cable and upload the **Blink** example (`LED_BUILTIN` → `2`) to confirm the board and cable work.
3. Unplug the USB cable. Place the ESP32 on a non-metal surface (e.g. bubble wrap or cardboard).

### Step 2 – Connect the Display

The display has a **26-hole socket** (2 rows × 13) on its back.

1. Place the display **screen-down** with the socket at the bottom-left.
2. Find **hole 1**: the socket end closest to the board corner.
   - The row along the board edge = **even holes** (2, 4, 6 … 26)
   - The inner row = **odd holes** (1, 3, 5 … 25)

   ```
   inner row:  1   3   5   7   9  11  13  15  17  19  21  23  25
   edge row:   2   4   6   8  10  12  14  16  18  20  22  24  26
               ▲ corner end
   ```

3. Use **9 Male–Female wires**. Push the **pin end** into the display hole and the **socket end** onto the ESP32 pin:

   | Colour (suggested) | Display hole | ESP32 pin |
   |---|---|---|
   | Red | 2 | VIN |
   | Black | 6 | GND (next to 3V3) |
   | Green | 19 | D23 |
   | Purple | 21 | D19 |
   | White | 23 | D18 |
   | Blue | 24 | D15 |
   | Brown | 18 | D2 |
   | Yellow | 22 | D4 |
   | Orange | 26 | D5 |

4. All other display holes stay **empty**.
5. Double-check: **red on VIN** and **black on GND** (power and ground must never be swapped).

### Step 3 – First Power Test

1. Plug in the USB cable.
2. Check that:
   - the **red power LED** on the ESP32 lights up, and
   - the **display glows white** (blank).
3. If anything smells or gets hot, **unplug immediately** and recheck the red and black wires.

### Step 4 – Display Software Check

1. In Arduino IDE, install the **TFT_eSPI** library (by Bodmer) from the Library Manager.
2. Replace the contents of `Arduino/libraries/TFT_eSPI/User_Setup.h` with:

   ```cpp
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

3. Upload a test sketch that fills the screen **red → green → blue** and prints text.
4. ✅ Expected: colours appear in the correct order, then the text.

> **Tip:** The Arduino sketchbook folder must not contain special characters (e.g. non-English folder names). This project uses `C:\Arduino`.

### Step 5 – Connect the RTC (DS3231)

1. Unplug the USB cable.
2. Insert the **CR2032** coin cell into the RTC holder with the **"+" side facing up**.
3. Using **Female–Female** wires, connect:

   | RTC pin | ESP32 pin |
   |---|---|
   | VCC | 3V3 |
   | SDA | D21 |
   | SCL | D22 |
   | GND | *(connected to the G strip in Step 6)* |

4. Install the **RTClib** library (by Adafruit).
5. ✅ Expected: after uploading the code, the time and date appear at the top-right of the screen. Use the dashboard's **"Sync clock with my phone"** button once to set the exact time.

### Step 6 – Create the Shared Ground Strip

Both ESP32 GND pins are now needed, and several parts need ground, so a shared ground strip is made on the breadboard.

1. Choose one **outer column** of a long edge strip on the breadboard. Mark it **"G"** with tape.
2. Connect a **Male–Female** wire from the **ESP32 GND next to VIN** to the **G strip**.
3. Connect the **RTC GND** pin to the **G strip** with a **Male–Female** wire.
4. Keep all ground wires close together on the same half of the strip (some breadboards split their strips in the middle).

### Step 7 – Connect the Push Button

1. Unplug the USB cable.
2. Place the button **across the middle groove** of the breadboard: 2 legs on each side.
3. Pick two **diagonally opposite legs** (e.g. top-left and bottom-right). This pair always acts as the switch, whichever way the button is turned.
4. In the **same short line** as the first diagonal leg, insert a **Male–Female** wire and connect its socket end to **ESP32 D33**.
5. In the **same short line** as the second diagonal leg, insert a **Male–Male** wire and connect its other end to the **G strip**.
6. ✅ Expected: a short press shows a green "Button works!" bar; a 3-second hold shows a red bar.

### Step 8 – Connect the Buzzer

1. Unplug the USB cable.
2. Identify the **+ leg** (longer leg / "+" mark on top). Remove the protective sticker on top for a louder sound.
3. **+ leg** → **Female–Female** wire → **ESP32 D13**
4. **− leg** → **Male–Female** wire (socket on the leg, pin into the **G strip**)
5. Make sure the two buzzer legs do not touch.
6. ✅ Expected: 1 beep at start-up, 1 beep on a button press, 3 beeps on a long press, 2 beeps when a new notice is posted. If silent, swap the two buzzer wires.

### Step 9 – Connect the Status LEDs

Each LED uses **two neighbouring short lines** on the breadboard.

```
ESP32 RX2 ──wire──► [Line 1] ── LED long leg (+)
                    [Line 2] ── LED short leg (−)
                    [Line 2] ── 1 kΩ resistor ──► G strip
```

**Green LED:**

1. Unplug the USB cable.
2. Long leg → **Line 1**, short leg → **Line 2**.
3. 1 kΩ resistor: one leg in **Line 2**, other leg in the **G strip** (bend the legs to reach; make sure the leg goes *into* a hole, not past the edge of the board).
4. **Male–Female** wire: pin in **Line 1**, socket on **ESP32 RX2 (GPIO 16)**.

**Red LED:**

5. Repeat in two new short lines a little further along (leave a gap of 1–2 lines).
6. Wire goes to **ESP32 TX2 (GPIO 17)**.

7. ✅ Expected: green LED ON at start-up; red LED ON while SOS is active. If an LED does not light, reverse its legs.

### Step 10 – Final Checks

1. Compare every connection with the **Pin Map** in Section 3.
2. Check that no bare metal legs or sockets touch each other.
3. Plug in the USB cable and verify:
   - [ ] Screen shows "NOTICE BOARD" header and the clock ticking
   - [ ] Phone can join WiFi **SmartNoticeBoard** (password `12345678`) and open **192.168.4.1**
   - [ ] Posting a notice shows it on screen and gives 2 beeps
   - [ ] Button short press / long press works
   - [ ] Green LED on normally, red LED on for SOS
4. Take a clear photo of the finished wiring for reference.

### Step 11 – Enclosure *(final stage)*

1. Cut a rectangular window in the box lid for the 3.5" screen.
2. Drill or cut small openings for the push button, the two LEDs, the buzzer sound and the USB cable.
3. Fix the display, breadboard and ESP32 inside with double-sided tape or hot glue. Keep the ESP32 away from metal.
4. Add a small dab of hot glue on jumper connectors that might loosen when the box is moved.

---

## 6. Troubleshooting

| Problem | Likely cause | Fix |
|---|---|---|
| Display stays white after upload | Wrong `User_Setup.h` or a loose data wire | Recheck the setup file and the 7 data wires (D23, D19, D18, D15, D2, D4, D5) |
| Display completely dark | No power to the display | Check red (hole 2 → VIN) and black (hole 6 → GND) |
| Clock shows "No clock" | RTC not detected | Check SDA → D21, SCL → D22, VCC → 3V3, GND → G strip |
| Clock time slightly wrong | Time was set at compile time | Use "Sync clock with my phone" on the web page |
| Button does nothing | Wires not in the same short line as the button legs | Place the button across the groove; use diagonal legs |
| Button always "pressed" | Both wires on legs that are always connected | Move to two diagonally opposite legs |
| Buzzer silent | Polarity reversed or loose wire | Swap the buzzer wires |
| LED does not light | LED reversed, or resistor not in the G strip | Flip the LED; push the resistor leg into the G strip |
| Upload stuck on "Connecting…" | ESP32 not in boot mode | Hold the **BOOT** button on the ESP32 until upload starts |
| "No such file" library error | Sketchbook path contains special characters | Move the sketchbook to a simple path such as `C:\Arduino` |
| PC does not detect the ESP32 | Charge-only USB cable or missing driver | Use a data cable; install the CP210x / CH340 driver |

---

## 7. Power Summary

| Rail | Source | Used by |
|---|---|---|
| 5V | USB → ESP32 **VIN** pin | Display (backlight and on-board regulator) |
| 3.3V | ESP32 **3V3** pin (on-board regulator) | DS3231 RTC |
| GPIO 3.3V | ESP32 pins | Buzzer, LEDs (via 1 kΩ), button (internal pull-up) |
| GND | ESP32 GND pins + breadboard G strip | All components |

Recommended supply: **5V, at least 1A** (laptop USB port or phone charger).
