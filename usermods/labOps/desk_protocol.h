#ifndef DESK_PROTOCOL_H
#define DESK_PROTOCOL_H

#include <Arduino.h>
#include "my_config.h"

class DeskProtocol {
public:
    DeskProtocol();

    // Initialize serial communication and pins
    void init();

    // Process incoming UART bytes from the desk (call this frequently in main loop)
    void process_rx();

    // Increments idle counters, calculates movement states (call this every 1 second in main loop)
    void tick_1s();

    // Actions
    void nudge_up();
    void nudge_down();
    void stop();
    void recall_preset(int id); // id from 1 to 4
    void save_preset(int id);   // id from 1 to 4
    void set_height(double target_cm);

    // Queries to desk
    void query_height();
    void query_limits();

    // Getters for status APIs
    double get_current_height() const { return current_height_cm; }
    double get_min_height() const { return min_height_cm; }
    double get_max_height() const { return max_height_cm; }
    String get_desk_state() const { return desk_state; }
    uint32_t get_idle_seconds() const { return seconds_idle; }
    String get_idle_time_str() const;

    // Preset values decoded from RX packets (in cm)
    double get_preset_height(int id) const;

private:
    // Parsing helper
    void parse_frame(const uint8_t* buffer, size_t len);

    // Wake up utility
    void wake_if_idle();

    // UART TX helper
    void send_command(const uint8_t* cmd, size_t len);
    void send_simple_command(uint8_t cmd_byte);

    // Internal states
    double current_height_cm;
    double last_height_cm;
    double min_height_cm;
    double max_height_cm;
    String desk_state; // "Idle", "Raising", "Lowering"

    uint32_t seconds_idle;
    uint32_t last_activity_ms;

    // Presets decoded from desk feedback (8-byte frames)
    double preset_m1_cm;
    double preset_m2_cm;
    double preset_m3_cm;
    double preset_m4_cm;

    // Jiecang / Fully Jarvis step calculations
    int min_motor_steps;
    int max_motor_steps;
    bool is_calibrating;

    // Circular RX Buffer for stream parsing
    static const size_t RX_BUF_SIZE = 64;
    uint8_t rx_buffer[RX_BUF_SIZE];
    size_t rx_index;
};

// Global instance of the DeskProtocol class
extern DeskProtocol desk;

#endif // DESK_PROTOCOL_H
