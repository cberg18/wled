#include "wled.h"
#include "desk_protocol.h"
#include <ESPAsyncWebServer.h>

void handle_status(AsyncWebServerRequest *request) {
    JsonObject doc = pDoc->to<JsonObject>();
    doc["height"] = desk.get_current_height();
    doc["state"] = desk.get_desk_state();

    JsonObject presets = doc["presets"].to<JsonObject>();
    presets["m1"] = desk.get_preset_height(1);
    presets["m2"] = desk.get_preset_height(2);
    presets["m3"] = desk.get_preset_height(3);
    presets["m4"] = desk.get_preset_height(4);

    JsonObject limits = doc["limits"].to<JsonObject>();
    limits["min"] = desk.get_min_height();
    limits["max"] = desk.get_max_height();

    doc["idleTime"] = desk.get_idle_time_str();

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handle_set_height(AsyncWebServerRequest *request) {
    if (request->hasArg("cm")) {
        double height = request->arg("cm").toDouble();
        desk.set_height(height);
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Missing cm parameter");
    }
}

void handle_command(AsyncWebServerRequest *request) {
    if (request->hasArg("action")) {
        String action = request->arg("action");
        if (action == "nudgeup") {
            desk.nudge_up();
        } else if (action == "nudgedown") {
            desk.nudge_down();
        } else if (action == "stop") {
            desk.stop();
        } else if (action == "m1") {
            desk.recall_preset(1);
        } else if (action == "m2") {
            desk.recall_preset(2);
        } else if (action == "m3") {
            desk.recall_preset(3);
        } else if (action == "m4") {
            desk.recall_preset(4);
        } else if (action == "setm1") {
            desk.save_preset(1);
        } else if (action == "setm2") {
            desk.save_preset(2);
        } else if (action == "setm3") {
            desk.save_preset(3);
        } else if (action == "setm4") {
            desk.save_preset(4);
        } else {
            request->send(400, "text/plain", "Unknown action: " + action);
            return;
        }
        request->send(200, "text/plain", "OK");
    } else {
        request->send(400, "text/plain", "Missing action parameter");
    }
}

class labOpsUsermod : public Usermod {
  private:
    bool initDone = false;

    void toggleState(int _buttonPressDuration, int buttonPin) {
          // Press: Set to OUTPUT and pull LOW
      Serial.println("toggleState: buttonPin = " + String(buttonPin) + " _buttonPressDuration = " + String(_buttonPressDuration));
      pinMode(buttonPin, OUTPUT);
      digitalWrite(buttonPin, LOW);
      vTaskDelay(_buttonPressDuration / portTICK_PERIOD_MS);
      // Release: Set back to INPUT
      pinMode(buttonPin, INPUT);
      Serial.println("toggleState: done");
    }

  public:
    void setup() override {
      // Initialization logic if needed
      pinMode(BUTTON_KB_PIN, INPUT);
      pinMode(BUTTON_HS_PIN, INPUT);
      desk.init();
      initDone = true;
    }
    void connected() override {
      // WLED uses a bundled version of AsyncJson that requires manual parsing
      server.addHandler(new AsyncCallbackJsonWebHandler("/api/toggle", [this](AsyncWebServerRequest *request) {
        if (request->_tempObject == NULL) {
          request->send(400, "application/json", "{\"error\":\"No body\"}");
          Serial.println("No body");
          return;
        }

        if (!requestJSONBufferLock(JSON_LOCK_SERVER)) {
          request->send(503, "application/json", "{\"error\":\"Busy\"}");
          Serial.println("Busy");
          return;
        }

        // Use WLED's global JSON buffer (pDoc) to save RAM
        DeserializationError error = deserializeJson(*pDoc,(uint8_t*)(request->_tempObject));
        if (error) {
          releaseJSONBufferLock();
          request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
          Serial.println("Invalid JSON");
          return;
        }

        JsonObject root = pDoc->as<JsonObject>();
        // Example: access a key "val"
        bool keyboard = root["keyboard"] | false;
        bool headset = root["headset"] | false;
        releaseJSONBufferLock();
        request->send(200, "application/json",
            "{\"status\":\"success\",\"keyboard\":" +
            String(keyboard) + ",\"headset\":" + String(headset) + "}");
        if (keyboard) {
            toggleState(BUTTON_PRESS_DURATION_MS, BUTTON_KB_PIN);
            Serial.println("Toggling keyboard");
        }
        if (headset) {
            toggleState(BUTTON_PRESS_DURATION_MS, BUTTON_HS_PIN);
            Serial.println("Toggling headset");
        }
      }));
      // Set up web server routes
      server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
        handle_status(request);
      });
      server.on("/api/set_height", HTTP_POST, [](AsyncWebServerRequest *request){
        handle_set_height(request);
      });
      server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request){
        handle_command(request);
      });
    }
    void loop() override {
        // Process incoming desk UART data
        desk.process_rx();
    }

    uint16_t getId() override {
      return 0xABCD; // Use a unique 16-bit ID
    }
  };
// Register the usermod so it's automatically included in the build
static labOpsUsermod labOps;
REGISTER_USERMOD(labOps);
