#pragma once

/*
 * Welcome!
 * You can use the file "my_config.h" to make changes to the way WLED is compiled!
 * It is possible to enable and disable certain features as well as set defaults for some runtime changeable settings.
 *
 * How to use:
 * PlatformIO: Just compile the unmodified code once! The file "my_config.h" will be generated automatically and now you can make your changes.
 *
 * ArduinoIDE: Make a copy of this file and name it "my_config.h". Go to wled.h and uncomment "#define WLED_USE_MY_CONFIG" in the top of the file.
 *
 * DO NOT make changes to the "my_config_sample.h" file directly! Your changes will not be applied.
 */

// uncomment to force the compiler to show a warning to confirm that this file is included
//#warning **** my_config.h: Settings from this file are honored ****

/* Uncomment to use your WIFI settings as defaults
  //WARNING: this will hardcode these as the default even after a factory reset
*/
#define CLIENT_SSID "Get_Off_My_LAN"
#define CLIENT_PASS

// ==========================================
// Hardware / Pins
// ==========================================
// Standard ESP32 HardwareSerial 2 Pins
#define DESK_RX_PIN 16 // Connect to Desk TX line
#define DESK_TX_PIN 17 // Connect to Desk RX line
#define DESK_BAUD_RATE 9600

#define BUTTON_KB_PIN 34
#define BUTTON_HS_PIN 35
#define BUTTON_PRESS_DURATION_MS 250

// Optional on-board LED status indicator
#define STATUS_LED_PIN 2 // Default blue LED on DevKit boards

// ==========================================
// Desk Controller Protocol Variant
// ==========================================
// Select your standing desk controller variant:
// - "default" (e.g., Maidesite, Boho Office, Vari Desk - most common)
// - "rocka"   (Rocka specific checksum style)
// - "jarvis"  (Fully Jarvis / Jiecang motor steps style)
#define DESK_VARIANT "default"

// Wake Up Command: If enabled, sends a Stop (wake) command if desk has been idle
// for more than 4 seconds prior to sending movement commands.
#define WAKE_UP_BEFORE_MOVE true
#define IDLE_WAKE_UP_THRESHOLD_SEC 4

// Offsets in millimeters applied when commanding target heights (cm * 10)
#define GO_UP_OFFSET_MM 0
#define GO_DOWN_OFFSET_MM 0

// Default physical height limits (in cm)
#define DEFAULT_MIN_HEIGHT_CM 62.0
#define DEFAULT_MAX_HEIGHT_CM 127.0


//#define MAX_LEDS 1500       // Maximum total LEDs. More than 1500 might create a low memory situation on ESP8266.
#define MDNS_NAME "desk"    // mDNS hostname, ie: *.local
