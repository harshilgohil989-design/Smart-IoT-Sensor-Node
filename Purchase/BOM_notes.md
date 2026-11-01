# BOM notes: fixes needed in the schematic

BOM.csv shows the corrected target design. Your KiCad export had vague values and some wrong footprints. Update the schematic so it matches, then re-export. **Verify every MPN against its datasheet and the supplier page before submitting.**

## Value and footprint fixes
| Ref | Export said | Problem | Fix in BOM |
|---|---|---|---|
| C1-C8 | "10", "220", "2 *22", "1microf", "680MICF" | No units | Units added. C1 10uF 50V, C5 is 2x 22uF (qty 2) |
| C2, C3 | 0805 footprint | Electrolytics (680uF, 220uF) cannot be 0805 | Radial footprints |
| C6 | 10nF | AP63203 boot cap is normally 100nF | 100nF (check the datasheet) |
| D3 | "D", 0805 | No value | 1N4148W relay flyback diode, SOD-123 |
| D6 | Zener SOD-123 | The PDF asks for a TVS | SMBJ28A, D_SMB footprint. 28V standoff sits above 24V + ripple, and clamps about 45V, which is under the LM2596 40V... see below |
| F1 | 0603 | Too small for a 24V supply | 1206 PTC 0.5A hold, 60V |
| L2 | 0201 | A 0201 inductor does not exist as a power part | 4.7uH, 4x4mm |
| R4 | "R" | No value | 4.7k 0.25W. (24-1.2V)/4.7k = about 4.9mA, so 0.11W: 0805 is too tight, use 1206 |
| Q1 | Q_NPN_BCE | BC337 pinout is CBE | Make sure the symbol pinout matches the real part |
| U5 | 0.91" module | That is a 128x32 display, but the PDF requires 128x64 | Use a 0.96" 128x64 I2C module and change the footprint (4-pin header) |

## Things still missing (the PDF requires them)
- **Reverse-polarity protection** on the 24V input: P-MOSFET or Schottky/diode in series, which also needs a footprint and MPN.
- **TVS clamping limit:** the LM2596 is rated 40V max input, and SMBJ28A clamps near 45V at high surge current. Use a TVS rated to clamp under 40V, e.g. SMBJ24A, or check the LM2596HV variant.
- **Relay indicator LED and its resistor** (the PDF requires an LED indicator).
- **24V DC power input terminal.** J2 is labelled INPUT, so confirm whether it is the digital input or the supply. You need a separate 2-pin screw terminal for the 24V supply.
- **OLED I2C pull-ups** (4.7k x2) and a **BOOT/EN** circuit. Confirm that R1-R7 cover these, and use 4.7k for the I2C pull-ups.
- **Power chain:** 24V -> LM2596 (5V) -> AP63203 (3.3V) works, but AP63203 accepts 24V directly. Consider dropping U3's input from 5V to save one stage, or keep it and justify it in the design notes.
- Strapping pins: GPIO2, GPIO8 and GPIO9 need the right pull-ups. Check the ESP32-C3 datasheet.

## Next step
Once the schematic is updated I can build purchase_sheet.xlsx (Indian suppliers, prices, alternates, totals for 1 and 5 boards, PCB quote). That needs live product pages checked on the day, so tell me when you're ready.
