# ESP32 Installation Instructions
1. You need Visual Studio Code with PlatformIO installed to build and upload the code to the ESP32.
1. **Open the Project**
   Open the `esp/wordclock/` directory in PlatformIO.
1. **Configuration**

   Make a copy of `esp/wordclock/src/config.h.example` and rename it to `esp/wordclock/src/config.h`.

   WiFi and timezone are not set here. They are entered from your phone on first boot (see below).
1. Connect your Word Clock following the [device build instructions](device_build.md).
1. Make sure that you have the ESP32 board selected in the `platformio.ini` configuration.
1. **Upload the Code**
   Connect your ESP32 board to your computer and upload the code using the PlatformIO upload button.

## First-time WiFi and timezone setup

1. Power on the clock. If it has no saved WiFi (or can't connect to it), it creates an open WiFi network called **WordClock-Setup**.
1. Connect to **WordClock-Setup** from your phone or computer. A setup page should open automatically. If it doesn't, browse to http://192.168.4.1.
1. Tap **Configure WiFi**, pick your network from the list, enter its password, and choose your timezone.
   If your timezone isn't in the list, choose **Other** and type a POSIX TZ string (find yours at https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv).
1. Tap **Save**. The clock joins your WiFi, the setup network disappears, and the clock shows the time.

WiFi and timezone are remembered across reboots and firmware uploads.

## Changing WiFi or timezone later

- Hold the **BOOT** button on the ESP32 for about 3 seconds while the clock is running. The **WordClock-Setup** network appears again; repeat the steps above.
  To change only the timezone, leave the WiFi network and password blank, pick the timezone and tap **Save**.
- If the clock can't reach your saved WiFi at startup, the setup network also appears. After 5 minutes it gives up and retries your saved WiFi, so a router that's slow to restart after a power cut won't leave the clock stuck in setup.
