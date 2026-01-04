# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**ZB433 Router** is an ESP32-C6 Zigbee router that controls CAME-24 gates via 433MHz transmission. It bridges Zigbee networks (Zigbee2MQTT/Home Assistant) with existing CAME-24 433MHz gate controllers.

### Key Features
- Zigbee 3.0 router functionality
- 433MHz CAME-24 protocol transmission (ASK/OOK modulation)
- 2 endpoints for controlling 2 different gates
- WS2812 LED feedback
- Device identification via Zigbee Identify cluster

## Build Commands

### Environment Setup
```bash
# Initialize ESP-IDF environment (required before any build)
source ~/esp-idf/export.sh

# Set target (required once, or after fullclean)
idf.py set-target esp32c6
```

### Building and Flashing
```bash
# Build the project
idf.py build

# Flash to device
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Flash and monitor in one command
idf.py -p /dev/ttyUSB0 flash monitor

# Clean build
idf.py fullclean
```

### Configuration
```bash
# Open menuconfig for ESP-IDF settings
idf.py menuconfig

# Project-specific settings are in sdkconfig.defaults
# Wi-Fi and OTA settings are in Kconfig.projbuild (ZB433 Configuration menu)
```

## Architecture

### Component Structure

The firmware is organized into 5 main modules:

1. **main.c** - Application entry point
   - Initializes NVS, LED, CAME433, and Zigbee stack
   - Handles network joining/commissioning
   - Main event loop

2. **zigbee.c/h** - Zigbee stack management
   - Configures ESP32-C6 native Zigbee radio
   - **CRITICAL**: ALL Zigbee initialization happens in zigbee_task(), NOT in zigbee_init()
   - Uses blocking esp_zb_stack_main_loop() (official ESP-Zigbee pattern)
   - Scans all Zigbee channels (ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK)
   - Handles ZCL command callbacks (On/Off, Identify)
   - Implements 5-second auto-reset timers for on_off attributes (debounce)
   - Complete signal handler with ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP
   - Auto-retry on steering failures via bdb_start_top_level_commissioning_cb

3. **endpoints.c/h** - Zigbee endpoint definitions
   - Creates 2 endpoints (EP1: Portail Principal, EP2: Portail Parking)
   - EP1: Uses esp_zb_on_off_switch_ep_create() helper + custom manufacturer info
   - EP2: Manual cluster creation (Basic + On/Off + Identify)
   - Both endpoints support Identify cluster
   - Custom manufacturer: "Cesar RICHARD EI", model: "ZB433-Router"
   - Maps button clicks to CAME433 transmissions
   - Inline add_manufacturer_info() helper (no external dependencies)

4. **came433.c/h** - 433MHz CAME protocol implementation
   - Uses RMT peripheral for precise timing
   - Implements CAME-24 protocol (24-bit codes)
   - High-side NPN+PNP driver control (GPIO4, idle LOW)
   - Protocol timing: bit 0 = 320µs LOW + 640µs HIGH, bit 1 = 640µs LOW + 320µs HIGH
   - Header: 24320µs LOW + 320µs start bit HIGH

5. **led.c/h** - WS2812 LED control
   - Visual feedback using RMT-based led_strip driver
   - Different colors for each endpoint (EP1: blue-cyan, EP2: magenta)
   - Identify effects: breathe, blink, okay

### Hardware Configuration

**GPIO Assignments:**
- GPIO4: 433MHz TX (high-side driver NPN+PNP)
- GPIO8: WS2812 LED

**433MHz Circuit:**
- Driver: 2N2222 (NPN) + pull-up on base for PNP control
- Firmware keeps GPIO4 LOW at idle, HIGH during transmission
- FS1000A module powered at +5V with 17.3cm antenna (λ/4)

### Zigbee Architecture

**Device Type:** Router (ESP_ZB_DEVICE_TYPE_ROUTER)
- Channel: Scans all channels (ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK)
- Profile: Home Automation (0x0104)
- Max children: 10
- Install code policy: disabled

**Endpoints:**
- EP1 (Portail Principal): On/Off Switch with Identify cluster
- EP2 (Portail Parking): On/Off Switch with Identify cluster

**Manufacturer Info:**
- Manufacturer: "Cesar RICHARD EI" (16 chars)
- Model: "ZB433-Router" (12 chars)

**Control Flow:**
1. Zigbee2MQTT sends On/Off command to endpoint
2. `zb_action_handler()` receives command in `ESP_ZB_CORE_CMD_CUSTOM_CLUSTER_REQ_CB_ID` or `ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID`
3. `handle_button_click()` triggered with endpoint ID
4. LED color set based on endpoint
5. `came433_send_portailX()` transmits 433MHz code (5 repetitions)
6. Timer started to reset on_off attribute after 5 seconds (debounce)
7. LED turned off

### CAME Protocol Details

**Key Codes:**
- KEY_A (0x0003B29B): Portail Principal
- KEY_B (0x0003B29A): Portail Parking

**Transmission Structure:**
- Header: 24320µs LOW
- Start bit: 320µs HIGH
- 24 data bits (MSB first)
- Bit encoding: Manchester-style (inverted levels for 0/1)
- 5 repetitions per command

### Zigbee2MQTT Integration

External converter file `zb433_external_converter.js` provides:
- Device fingerprint matching
- Multi-endpoint configuration
- Custom exposes as enum (multistate choice) for pushbutton behavior
- Hides automatic On/Off switches, exposes dropdown selectors instead
- Custom toZigbee converter that sends genOnOff.on command

## Configuration Files

**sdkconfig.defaults:**
- Target: ESP32-C6, 4MB flash
- Custom partition table (partitions.csv)
- Zigbee router role, max 8 children
- Wi-Fi and Bluetooth DISABLED (Zigbee-only)
- Optimized for size, assertions disabled

**Kconfig.projbuild:**
- Wi-Fi SSID/password (for future OTA, currently unused)
- Default OTA URL (currently unused)

## Important Notes

### Zigbee Stack Behavior

**CRITICAL Architecture Pattern (from ESP-Zigbee HA_color_dimmable_light example):**
```c
// zigbee_init() - Called from app_main()
void zigbee_init(void) {
    esp_zb_platform_config(&config);      // Platform setup ONLY
    xTaskCreate(zigbee_task, ...);        // Create task
}

// zigbee_task() - ALL initialization happens HERE
static void zigbee_task(void *pvParameters) {
    esp_zb_init(&zb_cfg);                 // Init stack
    create_endpoints();                   // Create endpoints
    esp_zb_core_action_handler_register(); // Register handlers
    esp_zb_set_primary_network_channel_set(); // Set channels
    esp_zb_start(false);                  // Start stack
    esp_zb_stack_main_loop();             // BLOCKING - never returns
}
```

**Signal Handler Flow:**
1. ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP → esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION)
2. ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START → Start network steering if factory new
3. ESP_ZB_BDB_SIGNAL_STEERING → Join successful or retry after 1 second
4. Auto-retry via bdb_start_top_level_commissioning_cb on failures

**Other Behaviors:**
- The on_off attribute resets automatically after 5 seconds to prevent "sticky" state
- Factory reset: send On command to EP14 (not currently implemented)

### CAME433 Hardware Requirements
- High-side driver requires firmware to output LOW when idle
- RMT is configured with 1µs resolution for precise timing
- GPIO must return to LOW state after transmission to turn off the RF module

### LED Feedback
- Red at startup
- Orange after 1 second
- Off when joined to network
- Specific colors during gate commands (blue-cyan/magenta)
- Identify effects use white/blue-white breathing pattern

### Debugging
- Use `idf.py monitor` to view serial logs
- Log level: DEBUG (configured in sdkconfig.defaults for troubleshooting)
- Tags: "ZB433" (main), "ZIGBEE", "BUTTONS", "CAME433", "LED"
- Zigbee signals are logged with detailed status information
- Network join shows: Extended PAN ID, PAN ID, Channel, Short Address

## Troubleshooting

### Network Join Failures

**Symptoms:**
- Device shows "Network steering was not successful" repeatedly
- Never gets past ESP_ZB_BDB_SIGNAL_STEERING
- Continuous retry attempts

**Common Causes & Solutions:**

1. **Missing ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP handler**
   - Without this signal handler, BDB initialization never starts
   - Add case in esp_zb_app_signal_handler() to call esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION)

2. **Wrong initialization order**
   - ALL Zigbee init must happen INSIDE zigbee_task(), not in zigbee_init()
   - Follow the official ESP-Zigbee example pattern (see "Zigbee Stack Behavior" section)
   - Use blocking esp_zb_stack_main_loop(), NOT esp_zb_stack_main_loop_iteration() in a loop

3. **Orphaned routers on network**
   - Other powered routers from old network can interfere
   - Power down all other Zigbee devices during troubleshooting
   - Clear coordinator permit-join list if device keeps getting rejected

4. **Channel mismatch**
   - Use ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK to scan all channels
   - Coordinator will assign the device to the correct channel automatically

5. **Verify with known-good example**
   - Flash the official HA_color_dimmable_light example to verify hardware/network
   - If example works but custom code doesn't, compare initialization patterns

## Common Development Patterns

### Adding New Gate Codes
1. Define new KEY_X in `came433.h` (24-bit hex value)
2. Add `came433_send_portailX()` function in `came433.c`
3. Create new endpoint in `endpoints.c` with appropriate clusters
4. Add case in `handle_button_click()` to map endpoint to transmission
5. Update Zigbee2MQTT converter to expose new endpoint

### Modifying Timing Parameters
CAME protocol timings are in `came433.h`:
- Test with real hardware - values are calibrated for actual gate receivers
- Flipper Zero timings have been validated to work
- Adjust CAME_REPEATS if transmission reliability is poor

### Testing Zigbee Commands
```bash
# Main gate (EP1)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"state":"ON"}'

# Parking gate (EP2)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/2/set" -m '{"state":"ON"}'

# Identify effect (EP1)
mosquitto_pub -h localhost -t "zigbee2mqtt/ZB433 Router/1/set" -m '{"write":{"cluster":"genIdentify","payload":{"identify_time":5}}}'
```
