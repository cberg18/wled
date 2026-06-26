#include "desk_protocol.h"


DeskProtocol::DeskProtocol()
    : current_height_cm(70.0),
      last_height_cm(70.0),
      min_height_cm(DEFAULT_MIN_HEIGHT_CM),
      max_height_cm(DEFAULT_MAX_HEIGHT_CM),
      desk_state("Idle"),
      seconds_idle(0),
      last_activity_ms(0),
      preset_m1_cm(72.0),
      preset_m2_cm(80.0),
      preset_m3_cm(95.0),
      preset_m4_cm(110.0),
      min_motor_steps(0),
      max_motor_steps(1000),
      is_calibrating(false),
      rx_index(0) {
    memset(rx_buffer, 0, RX_BUF_SIZE);
}

void DeskProtocol::init() {
    // Start hardware serial for communicating with the desk
    // On ESP32, Serial2 is assigned to configurable pins
    Serial2.begin(DESK_BAUD_RATE, SERIAL_8N1, DESK_RX_PIN, DESK_TX_PIN);

    // Setup status LED pin
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    Serial.println("[Desk] UART Protocol Initialized.");

    // Query current height and limits on startup
    query_height();
    delay(50);
    query_limits();
}

void DeskProtocol::process_rx() {
    // Read bytes from physical serial connection to desk
    while (Serial2.available() > 0) {
        uint8_t b = Serial2.read();

        // Push to circular/stream buffer
        if (rx_index < RX_BUF_SIZE) {
            rx_buffer[rx_index++] = b;
        } else {
            // Buffer full without matching, shift left by 1 to make space
            memmove(rx_buffer, rx_buffer + 1, RX_BUF_SIZE - 1);
            rx_buffer[RX_BUF_SIZE - 1] = b;
            rx_index = RX_BUF_SIZE;
        }

        // Align buffer: must start with 0xF2 0xF2
        while (rx_index >= 2) {
            if (rx_buffer[0] != 0xF2 || rx_buffer[1] != 0xF2) {
                // Shift left by 1
                memmove(rx_buffer, rx_buffer + 1, rx_index - 1);
                rx_index--;
            } else {
                break; // Start bytes aligned
            }
        }

        // If we have at least 3 bytes, check command byte at index 2 to determine frame size
        if (rx_index >= 3) {
            uint8_t cmd_byte = rx_buffer[2];
            size_t expected_len = 0;

            if (cmd_byte == 0x01) {
                expected_len = 9; // Height Update
            } else if (cmd_byte >= 0x25 && cmd_byte <= 0x28) {
                expected_len = 8; // Preset update feedback
            } else if (cmd_byte == 0x07) {
                expected_len = 10; // Physical Limits
            } else {
                // Unknown command byte. Discard the start bytes and scan again
                memmove(rx_buffer, rx_buffer + 2, rx_index - 2);
                rx_index -= 2;
                continue;
            }

            // Do we have the complete expected frame?
            if (rx_index >= expected_len) {
                // Verify the stop byte is 0x7E
                if (rx_buffer[expected_len - 1] == 0x7E) {
                    // Valid frame structure, parse it
                    parse_frame(rx_buffer, expected_len);

                    // Consume the frame from buffer
                    memmove(rx_buffer, rx_buffer + expected_len, rx_index - expected_len);
                    rx_index -= expected_len;
                } else {
                    // Stop byte not found, frame is corrupted or misaligned
                    // Discard start bytes to find next valid boundary
                    memmove(rx_buffer, rx_buffer + 2, rx_index - 2);
                    rx_index -= 2;
                }
            }
        }
    }
}

void DeskProtocol::parse_frame(const uint8_t* buffer, size_t len) {
    uint8_t cmd = buffer[2];

    // Activity has occurred, reset activity timestamps
    last_activity_ms = millis();

    if (cmd == 0x01 && len == 9) {
        // Height Update Frame
        // Format: F2 F2 01 00 HIGH LOW CHECKSUM UNUSED 7E
        int rawHeight = (buffer[4] * 256) + buffer[5];

        if (strcmp(DESK_VARIANT, "jarvis") == 0) {
            if (is_calibrating) {
                // Calibration logs or saves
                Serial.printf("[Desk] Calibration Steps: %d\n", rawHeight);
            } else {
                // Convert motor steps to physical height
                double countsPerCM = (double)(max_motor_steps - min_motor_steps) / (max_height_cm - min_height_cm);
                if (countsPerCM > 0) {
                    current_height_cm = ((rawHeight - min_motor_steps) / countsPerCM) + min_height_cm;
                }
            }
        } else {
            // Standard Desk Up: rawHeight is in millimeters
            current_height_cm = rawHeight / 10.0;
        }

        // Clip bounds to prevent noisy readings
        if (current_height_cm < 20.0) current_height_cm = 20.0;
        if (current_height_cm > 200.0) current_height_cm = 200.0;

        // Blink status LED on data RX
        digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));

    } else if (cmd >= 0x25 && cmd <= 0x28 && len == 8) {
        // Preset Updates
        double height = ((buffer[4] * 256) + buffer[5]) / 10.0;
        switch (cmd) {
            case 0x25: preset_m1_cm = height; break;
            case 0x26: preset_m2_cm = height; break;
            case 0x27: preset_m3_cm = height; break;
            case 0x28: preset_m4_cm = height; break;
        }
        Serial.printf("[Desk] Preset M%d update: %.1f cm\n", (cmd - 0x25 + 1), height);

    } else if (cmd == 0x07 && len == 10) {
        // Limits frame
        double max_h = ((buffer[4] * 256) + buffer[5]) / 10.0;
        double min_h = ((buffer[6] * 256) + buffer[7]) / 10.0;

        if (min_h > 20.0 && max_h > min_h && max_h < 200.0) {
            min_height_cm = min_h;
            max_height_cm = max_h;
            Serial.printf("[Desk] Limits Updated. Min: %.1f cm, Max: %.1f cm\n", min_height_cm, max_height_cm);
        }
    }
}

void DeskProtocol::tick_1s() {
    double delta = current_height_cm - last_height_cm;

    if (delta > 0.05) {
        desk_state = "Raising";
        seconds_idle = 0;
    } else if (delta < -0.05) {
        desk_state = "Lowering";
        seconds_idle = 0;
    } else {
        desk_state = "Idle";
        seconds_idle++;
    }

    last_height_cm = current_height_cm;

    // Ensure status LED is off when idle
    if (desk_state == "Idle") {
        digitalWrite(STATUS_LED_PIN, LOW);
    }
}

void DeskProtocol::wake_if_idle() {
    if (WAKE_UP_BEFORE_MOVE && seconds_idle >= IDLE_WAKE_UP_THRESHOLD_SEC) {
        Serial.println("[Desk] Wake up desk first...");
        send_simple_command(0x2B); // Send STOP command to wake controller
        delay(100);
        seconds_idle = 0; // Reset idle timer
    }
}

void DeskProtocol::send_command(const uint8_t* cmd, size_t len) {
    Serial2.write(cmd, len);
    Serial2.flush();
    seconds_idle = 0; // Reset idle tracker on TX
}

void DeskProtocol::send_simple_command(uint8_t cmd_byte) {
    uint8_t packet[6] = {0xF1, 0xF1, cmd_byte, 0x00, cmd_byte, 0x7E};
    send_command(packet, 6);
}

void DeskProtocol::nudge_up() {
    wake_if_idle();
    Serial.println("[Desk] CMD: Nudge Up");
    send_simple_command(0x01);
}

void DeskProtocol::nudge_down() {
    wake_if_idle();
    Serial.println("[Desk] CMD: Nudge Down");
    send_simple_command(0x02);
}

void DeskProtocol::stop() {
    Serial.println("[Desk] CMD: Stop");
    send_simple_command(0x2B);
}

void DeskProtocol::recall_preset(int id) {
    wake_if_idle();
    Serial.printf("[Desk] CMD: Recall Preset %d\n", id);
    switch (id) {
        case 1: send_simple_command(0x05); break;
        case 2: send_simple_command(0x06); break;
        case 3: send_simple_command(0x27); break;
        case 4: send_simple_command(0x28); break;
        default: break;
    }
}

void DeskProtocol::save_preset(int id) {
    Serial.printf("[Desk] CMD: Save Preset %d\n", id);
    switch (id) {
        case 1: send_simple_command(0x03); break;
        case 2: send_simple_command(0x04); break;
        case 3: send_simple_command(0x25); break;
        case 4: send_simple_command(0x26); break;
        default: break;
    }
}

void DeskProtocol::set_height(double target_cm) {
    wake_if_idle();

    // Safety boundaries
    if (target_cm < min_height_cm) target_cm = min_height_cm;
    if (target_cm > max_height_cm) target_cm = max_height_cm;

    int target_mm = (int)(target_cm * 10.0);

    // Dynamic off-sets applied during raise/lower
    if (target_cm > current_height_cm) {
        target_mm += GO_UP_OFFSET_MM;
    } else if (target_cm < current_height_cm) {
        target_mm -= GO_DOWN_OFFSET_MM;
    }

    uint8_t high = target_mm >> 8;
    uint8_t low = target_mm & 0xFF;
    uint8_t checksum = 0;

    if (strcmp(DESK_VARIANT, "rocka") == 0) {
        checksum = 0x1B + 0x02 + high + low;
    } else {
        checksum = (0x1B + 0x02 + high + low) % 256;
    }

    uint8_t packet[8] = {0xF1, 0xF1, 0x1B, 0x02, high, low, checksum, 0x7E};

    Serial.printf("[Desk] CMD: Set height to %.1f cm (Value: %d mm)\n", target_cm, target_mm);
    send_command(packet, 8);
}

void DeskProtocol::query_height() {
    Serial.println("[Desk] Query Current Height");
    send_simple_command(0x07);
}

void DeskProtocol::query_limits() {
    Serial.println("[Desk] Query Limits");
    send_simple_command(0x0C);
}

double DeskProtocol::get_preset_height(int id) const {
    switch (id) {
        case 1: return preset_m1_cm;
        case 2: return preset_m2_cm;
        case 3: return preset_m3_cm;
        case 4: return preset_m4_cm;
        default: return 0.0;
    }
}

String DeskProtocol::get_idle_time_str() const {
    uint32_t seconds = seconds_idle;
    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    char buf[32];
    if (days > 0) {
        snprintf(buf, sizeof(buf), "%d days, %02d:%02d:%02d", days, hours, minutes, secs);
    } else {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hours, minutes, secs);
    }
    return String(buf);
}

// Instantiate Global Desk object
DeskProtocol desk;
