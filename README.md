# Copyright and Disclaimer
Copyright: Scott Hanson

This documentation describes Open Hardware and is licensed under the CERN Open Hardware License Version 2 - Strongly Reciprocal. (CERN-OHL-S)

You may redistribute and modify this documentation under the terms of the CERN OHL-S-v2 (https://ohwr.org/cern_ohl_s_v2.txt). This documentation is distributed WITHOUT ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING OF MERCHANTABILITY, SATISFACTORY QUALITY AND FITNESS FOR A PARTICULAR PURPOSE. Please see the CERN OHL v2-S for applicable conditions

**Under CERN OHL-S-v2, derivative work must be publicly released as defined in subsection 3.3**

# RP2040 Count

RP2040 Pixel Counter is a 'pocket' Pixel Counter. It uses a Waveshare RP2040-Tiny Development Board.  All the design files are open source and available on github.

To order PCBs upload the ["GERBER-RP2040_Count.zip"](https://github.com/computergeek1507/RP2040_Count/raw/main/PCB/jlcpcb/production_files/GERBER-RP2040_Count.zip) file in the "PCB\jlcpcb\production_files" folder to jlcpcb.com. Use the "BOM-RP2040_Count.csv" for the JLC Assembly BOM and "CPL-RP2040_Count.csv" as the footprint placement file.

## Assembly

The raw PCB's from JLC will be missing the RP2040-Tiny, OLED, Push Button and PigTails. These parts need to be soldered manually. The case is designed for flat pigtails.

Print the case out of PETG with 0.1mm layer height. Use #2-56 3/8" machine screws for case.

## Board Variants

This repo tracks a few PCB/firmware revisions side by side. `Firmware/RP2040_Count/BoardConfig.h` selects the pin mapping for each via a build flag (`VERSION1`/`VERSION2`/`VERSION3`); the default is `VERSION2`.

| Variant | Folder | MCU module | Firmware | Notes |
| --- | --- | --- | --- | --- |
| Original | `PCB/`, `Case/`, `Firmware/RP2040_Count/` | Waveshare RP2040-Tiny | `VERSION1` | The board described above; also see `RP2040-Zero/` for a variant PCB built around the Waveshare RP2040-Zero module. |
| V2 | [`V2/`](https://github.com/computergeek1507/RP2040_Count/tree/main/V2) | Seeed XIAO RP2040 | `VERSION2` (default) | Standard redesign around the XIAO RP2040 module - firmware updates over USB (drag-and-drop UF2), no external programmer needed. |
| V2 Lite | [`V2 Lite/`](https://github.com/computergeek1507/RP2040_Count/tree/main/V2%20Lite) | ATtiny1614 (megaTinyCore) | [`Firmware/ATtiny1614_Count/`](https://github.com/computergeek1507/RP2040_Count/tree/main/Firmware/ATtiny1614_Count) | Cost-reduced BOM built around a bare ATtiny1614 instead of a RP2040 module; trades USB firmware updates for a UPDI programmer requirement. Uses FAB_LED/SSD1306Ascii instead of the Adafruit libraries to fit in 16KB flash / 2KB RAM. Shares the same case as V2 (`V2/Case/` and `V2 Lite/Case/` are the same design). |
| V3 Pro | [`V3 Pro/`](https://github.com/computergeek1507/RP2040_Count/tree/main/V3%20Pro) | Seeed XIAO RP2040 | `VERSION3` | Adds a second button and a microSD card slot for playing back xLights FSEQ sequences on the strip (`Firmware/RP2040_Count/FseqPlayer.*`). |

Each variant folder that has its own `PCB/` directory also has its own `jlcpcb/production_files` gerbers/BOM/CPL for ordering that specific board.

## [Video of RP2040 Count](https://youtu.be/7ThN9TBFA-g)

## [Part BOM](https://github.com/computergeek1507/RP2040_Count/raw/main/PCB/RP2040_Count_BOM.ods)

## [Interactive BOM](https://computergeek1507.github.io/RP2040_Count/PCB/bom/ibom)

![Image of RP2040 Count](https://github.com/computergeek1507/RP2040_Count/raw/main/PXL_20240302_150726946.jpg)

![Image of RP2040 Count](https://github.com/computergeek1507/RP2040_Count/raw/main/Case/Case.bmp)

![Image of RP2040 Count](https://github.com/computergeek1507/RP2040_Count/raw/main/PCB/RP2040_Count.png)
