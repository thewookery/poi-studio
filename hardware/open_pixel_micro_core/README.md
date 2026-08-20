# 🌟 Open Pixel 'Micro-Core' Universal Flow Prop Controller (v1.0.0)

The **Open Pixel Micro-Core** is an ultra-compact (**18.0mm x 38.0mm**), high-performance controller board designed to fit inside standard **3/4-inch (19mm ID) and 7/8-inch polycarbonate tubing**.

It is 100% plug-and-play compatible with **Open POI Studio**, supporting **Poi**, **Pixel Staffs**, **Levitation Wands**, **Contact Juggling Props**, **Pixel Fans**, and **Hoops**.

---

## 📐 Board Dimensions & Mechanical Fit

* **Dimensions**: 18.00 mm x 38.00 mm
* **Corner Fillet**: Radius 2.5 mm (smooth glide inside round tubes)
* **PCB Thickness**: 1.6 mm FR-4 (1.0 mm or 1.2 mm also supported for ultra-lightweight wands)
* **Mounting / Leash Holes**: Dual 2.2 mm through-holes for M2 hardware, flow cordage, or safety lanyards.

---

## ⚡ Technical Specifications

| Feature | Specification |
| :--- | :--- |
| **Microcontroller** | Seeed Studio XIAO ESP32-C3 (RISC-V 160MHz, 4MB Flash, Wi-Fi & BLE 5.0) |
| **Input Power** | USB-C 16-Pin with Dual 5.1k CC1/CC2 pulldowns (Supports USB-C to USB-C PD) |
| **Battery Chemistry** | 1S 3.7V Lithium-Ion (18650, 14500, 18350) or Lithium-Polymer (LiPo pouch cells) |
| **Battery Connectors** | Standard **JST-PH 2.0mm 2-Pin** + Parallel Direct Solder Pads (BATT+, BATT-) |
| **Battery Charger** | Linear Li-Ion Charger IC (TP4054 / MCP73831) @ 500mA fast charge |
| **Battery Protection** | Hardware Over-charge, Over-discharge (2.5V cutoff), and Short-Circuit Protection |
| **Battery Telemetry** | 100k / 100k 0.1% precision voltage divider to ADC pin A0 / GPIO2 |
| **LED Strip Outputs** | **3x 4-Pin 0.5mm FPC ZIF Flip-Lock Headers** + **4-Pin 2.54mm Heavy-Duty Solder Header** |
| **Supported LEDs** | DotStar (APA102, SK9822, HD107S) & NeoPixel (WS2812B, SK6812) |
| **Signal Integrity** | Dual 33-ohm inline series damping resistors on high-speed 20MHz SPI data/clock lines |
| **Interactive Controls** | Onboard SMD tactile switch + **2-Pin External Button Breakout** (BTN, GND) |

---

## 🔌 Pinout & Wiring Connections

```
                             +------------------------+
                     (M2) O  | [=]  USB-C (Power/Prog)|  O (M2)
                             |                        |
                   (CHG) D1  |  [Seeed XIAO ESP32-C3] |  R1, R2 (5.1k CC)
                  (FULL) D2  |                        |  
                             |  GPIO2: Batt Sense     |  
                             |  GPIO3: Button Input   |  R8/R9 (Divider)
                             |  GPIO6: LED Data Out   |  
                             |  GPIO7: LED Clock Out  |  
                             |                        |
          [FPC 1 (Left)]     | [FPC 1]  [FPC 2]  [FPC3]|     [FPC 3 (Right)]
          [FPC 2 (Center)]   |                        |
                             |   [SW1: Tact Button]   |  
       (Ext Btn) J7: [BTN]   |                        |  J2: [JST-PH 2.0mm LiPo]
                     [GND]   | [TP4054]     [DW01A]   |      (Pin 1: BATT+)
                             | [ME6211]    [FS8205A]  |      (Pin 2: BATT-)
                             |                        |
                             |  (1)   (2)   (3)   (4) |  <- J6 Heavy-Duty Solder Header
                             |  VCC   GND   DAT   CLK |  
                             +------------------------+
```

### J6 Heavy-Duty LED Solder Header (2.54mm Pitch):
* **Pin 1 (VCC)**: Direct battery power output for LED strips (3.5V - 4.2V).
* **Pin 2 (GND)**: Common system ground.
* **Pin 3 (DAT)**: High-speed SPI data line (GPIO6) with 33-ohm damping.
* **Pin 4 (CLK)**: High-speed SPI clock line (GPIO7) with 33-ohm damping (Leave disconnected for 3-wire WS2812B).

### J7 External Button Solder Header (2.54mm Pitch):
* **Pin 1 (BTN)**: Active-LOW button line tied to GPIO3 with hardware pull-up.
* **Pin 2 (GND)**: Ground.

---

## 📦 Factory Manufacturing Files

All files required for turnkey SMT fabrication on **PCBWay** or **JLCPCB** are ready in this folder:

1. **`open_pixel_micro_core_gerbers.zip`**: Complete RS-274X Gerber & Excellon drill archive.
2. **`open_pixel_micro_core_bom.csv`**: Turnkey Bill of Materials with exact LCSC part numbers.
3. **`open_pixel_micro_core_cpl.csv`**: Automated SMT Pick & Place centroid positioning data.
4. **`open_pixel_micro_core.kicad_sch`**: Editable KiCad schematic.
5. **`open_pixel_micro_core.kicad_pcb`**: Editable KiCad board layout.

---

## 🚀 How to Order on PCBWay / JLCPCB

1. Go to **PCBWay.com** (or **JLCPCB.com**).
2. Click **Quick-Order PCB** and upload `open_pixel_micro_core_gerbers.zip`.
3. Board parameters:
   * **Dimensions**: 18.0 mm x 38.0 mm
   * **Layers**: 2 Layers
   * **Solder Mask**: Matte Black (or your preferred color)
   * **Surface Finish**: ENIG (Electroless Nickel Immersion Gold) or HASL Lead-Free.
4. Select **SMT Assembly (Turnkey)**:
   * Upload `open_pixel_micro_core_bom.csv` for the BOM.
   * Upload `open_pixel_micro_core_cpl.csv` for the CPL / Pick-and-Place coordinates.
5. Submit for automated review and order!
