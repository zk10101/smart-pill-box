# ESP32-C3 first flash through the STM32

The board exposes ESP GPIO9/GND on H3, but not ESP UART0 as a programming header. The onboard STM32 already connects USART1 PA9/PA10 to ESP RXD0/TXD0 through 1 kOhm resistors. This temporary firmware bridges the PC-facing H1 USART2 to ESP-facing USART1 at 115200-8N1. It sends no status text on either UART because esptool needs a transparent binary link.

## Wiring

| USB-TTL, 3.3 V logic | Board |
|---|---|
| TX | H1 pin 3, STM32 PA3 / USART2_RX |
| RX | H1 pin 2, STM32 PA2 / USART2_TX |
| GND | H1 pin 4, GND |

Leave H1 pin 1 (`5V_SW`) disconnected from the USB-TTL. Connect H3 GPIO9 to H3 GND before resetting the ESP. Leave the already wired STM32 PA9/PA10 to ESP UART0 in place. Connect ST-Link to the STM32 SWD header and power the board normally so that its 3.3 V rail is present; an ST-Link Vref wire does not itself power the board. Do not feed the board from the USB-TTL VCC pin.

ESP GPIO8 must be high when GPIO9 is sampled low for download mode. If esptool cannot sync after checking the UART wiring, verify GPIO8's level on the actual board. The existing hardware review also flags the ESP module's GND pads for continuity testing.

## Build and install the temporary bridge

1. In Keil, open `ESP_Flash_Bridge.uvprojx`, select `ESP Flash Bridge`, and build. The independent output is `BridgeObjects/ESP_Flash_Bridge.hex`. The usual `smart-pill-box.uvprojx` remains the normal application project. If Keil regenerates settings, choose **ST-Link Debugger** in both Target Options > Debug and Target Options > Utilities.
2. With H3 GPIO9 tied to GND, download the bridge `.hex` to STM32 using ST-Link. The bridge starts by holding ESP EN low for about 150 ms, then releases it. Resetting the STM32 while the bridge is installed repeats the ESP reset into download mode.
3. Confirm that Windows detects the USB-TTL COM port. Connect no camera to H1 during flashing. Keil's build success only checks the bridge code; it does not prove the board's serial wiring or boot strapping.

The generated project can be refreshed from the current normal Keil project with `./generate_project.ps1`. This does not modify the normal project.

## Build and flash the ESP image

Open `../../esp32/zhinengyaohe_esp/zhinengyaohe_esp.ino` in Arduino IDE. Select **ESP32C3 Dev Module**, install the **PubSubClient** library, and use **Sketch > Export Compiled Binary**. Find the resulting `zhinengyaohe_esp.ino.merged.bin` (4 MB full flash image). A plain `zhinengyaohe_esp.ino.bin` is only the application image and must not be written at `0x0`.

Close any serial monitor using the USB-TTL COM port. The following script first probes the ESP flash ID through the bridge and only then writes the merged image; replace `COM7` and the image path with your actual values:

```powershell
& 'C:\Users\16033\Desktop\smart-pill-box\smart-pill-box\tools\esp_flash_bridge\flash_esp.ps1' -Port COM7 -Image 'C:\path\to\zhinengyaohe_esp.ino.merged.bin'
```

The script uses the `esptool.exe` installed by the Arduino ESP32 board package, with manual reset (`--before no-reset --after no-reset`). During the flash-ID probe, a valid chip ID proves the PC, H1, STM32 bridge, ESP ROM bootloader, and return path are working. A completed `write-flash` plus esptool verification proves the image was written. If the probe fails, do not try to flash repeatedly; check H3, 3.3 V supply, H1 TX/RX crossing, and ESP GPIO8/EN.

After success, remove the H3 GPIO9-GND jumper and restart the board. Then flash the normal `smart-pill-box.uvprojx` application back into STM32 using ST-Link. On the normal application, the display's `ESP:ON` should appear after the ESP starts sending its diagnostic frames. The bridge is temporary; it has no display, motor logic, or cloud functions.
