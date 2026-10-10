/*
 * Microcontroller-based Voltage Protector Device
 * FSMB Recruitment 2026 - Phase 1
 * 
 * Features:
 * - Monitors voltage (simulated via potentiometer on A0)
 * - Monitors current (simulated via potentiometer on A1)
 * - Disconnects load via relay on pin 8 when over-voltage, under-voltage or over-current
 * - 16x2 LCD for local display and settings
 * - Buttons for local threshold adjustment
 * - Serial communication for computer-based settings and monitoring
 * 
 * Safety: All sensing is low-voltage simulation. Do NOT connect to mains.
 * 
 * Threshold defaults (adjustable):
 *   Over-voltage:  250 V
 *   Under-voltage: 180 V
 *   Over-current:  5.0 A
 */

#include <LiquidCrystal.h>

// LCD pins: RS, E, D4, D5, D6, D7
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

// Pins
const int VOLTAGE_PIN = A0;   // Potentiometer simulating scaled AC voltage
const int CURRENT_PIN = A1;   // Potentiometer simulating current
const int RELAY_PIN   = 8;    // Relay control (HIGH = connected / safe)
const int BUZZER_PIN  = 9;    // Optional alarm
const int BTN_UP      = 6;    // Increase selected threshold
const int BTN_DOWN    = 7;    // Decrease selected threshold
const int BTN_MODE    = 10;   // Cycle through settings mode

// Scaling factors for simulation
// Voltage pot: 0-5V ADC maps to 0-300V AC
const float VOLTAGE_SCALE = 300.0 / 1023.0;
// Current pot: 0-5V ADC maps to 0-10A
const float CURRENT_SCALE = 10.0 / 1023.0;

// Adjustable thresholds
float overVoltage  = 250.0;
float underVoltage = 180.0;
float overCurrent  = 5.0;

// State
bool loadConnected = true;
int mode = 0;  // 0=normal display, 1=set OV, 2=set UV, 3=set OC
unsigned long lastDisplay = 0;
unsigned long lastSerial = 0;
unsigned long lastBtn = 0;
const unsigned long DEBOUNCE = 250;

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);

  digitalWrite(RELAY_PIN, HIGH); // Start with load connected
  digitalWrite(BUZZER_PIN, LOW);

  lcd.begin(16, 2);
  lcd.print("Voltage Protector");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1500);
  lcd.clear();

  Serial.begin(9600);
  Serial.println(F("=== Voltage Protector Ready ==="));
  Serial.println(F("Commands:"));
  Serial.println(F("  STATUS          - show readings & thresholds"));
  Serial.println(F("  SET_OV <val>    - set over-voltage threshold"));
  Serial.println(F("  SET_UV <val>    - set under-voltage threshold"));
  Serial.println(F("  SET_OC <val>    - set over-current threshold"));
  Serial.println(F("  CONNECT         - force reconnect load"));
  Serial.println(F("  DISCONNECT      - force disconnect load"));
  Serial.println(F("==============================="));
}

void loop() {
  // Read sensors
  int rawV = analogRead(VOLTAGE_PIN);
  int rawI = analogRead(CURRENT_PIN);
  float voltage = rawV * VOLTAGE_SCALE;
  float current = rawI * CURRENT_SCALE;

  // Protection logic
  bool fault = false;
  String faultMsg = "";

  if (voltage > overVoltage) {
    fault = true;
    faultMsg = "OVER VOLTAGE";
  } else if (voltage < underVoltage) {
    fault = true;
    faultMsg = "UNDER VOLTAGE";
  } else if (current > overCurrent) {
    fault = true;
    faultMsg = "OVER CURRENT";
  }

  if (fault) {
    if (loadConnected) {
      loadConnected = false;
      digitalWrite(RELAY_PIN, LOW);  // Disconnect
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.print(F("FAULT: "));
      Serial.println(faultMsg);
    }
  } else {
    // Auto-reconnect only if previously faulted and now safe
    // (simple hysteresis: reconnect when clearly safe)
    if (!loadConnected && voltage > underVoltage + 5 && voltage < overVoltage - 5 && current < overCurrent - 0.2) {
      loadConnected = true;
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println(F("Load reconnected - voltage/current normal"));
    }
  }

  // Local button handling
  handleButtons();

  // Display update
  if (millis() - lastDisplay > 300) {
    updateLCD(voltage, current, fault, faultMsg);
    lastDisplay = millis();
  }

  // Serial monitoring / commands
  if (millis() - lastSerial > 1000) {
    // Periodic status can be enabled if desired
    lastSerial = millis();
  }
  handleSerial();
}

void handleButtons() {
  if (millis() - lastBtn < DEBOUNCE) return;

  if (digitalRead(BTN_MODE) == LOW) {
    mode = (mode + 1) % 4;
    lastBtn = millis();
    lcd.clear();
  }

  if (mode == 0) return; // normal view, ignore up/down

  float step = (mode == 3) ? 0.1 : 1.0; // finer for current

  if (digitalRead(BTN_UP) == LOW) {
    if (mode == 1) overVoltage += step;
    if (mode == 2) underVoltage += step;
    if (mode == 3) overCurrent += step;
    lastBtn = millis();
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (mode == 1) overVoltage = max(100.0, overVoltage - step);
    if (mode == 2) underVoltage = max(50.0, underVoltage - step);
    if (mode == 3) overCurrent = max(0.5, overCurrent - step);
    lastBtn = millis();
  }
}

void updateLCD(float v, float i, bool fault, String msg) {
  lcd.setCursor(0, 0);
  if (mode == 0) {
    // Normal monitoring
    lcd.print("V:");
    lcd.print(v, 0);
    lcd.print(" I:");
    lcd.print(i, 1);
    lcd.print("A   ");
    lcd.setCursor(0, 1);
    if (fault) {
      lcd.print(msg);
      lcd.print(" !");
    } else if (loadConnected) {
      lcd.print("Status: SAFE   ");
    } else {
      lcd.print("Status: OFF    ");
    }
  } else if (mode == 1) {
    lcd.print("Set Over-Volt: ");
    lcd.setCursor(0, 1);
    lcd.print(overVoltage, 0);
    lcd.print(" V          ");
  } else if (mode == 2) {
    lcd.print("Set Under-Volt:");
    lcd.setCursor(0, 1);
    lcd.print(underVoltage, 0);
    lcd.print(" V          ");
  } else if (mode == 3) {
    lcd.print("Set Over-Curr: ");
    lcd.setCursor(0, 1);
    lcd.print(overCurrent, 1);
    lcd.print(" A          ");
  }
}

void handleSerial() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "STATUS") {
      int rawV = analogRead(VOLTAGE_PIN);
      int rawI = analogRead(CURRENT_PIN);
      float voltage = rawV * VOLTAGE_SCALE;
      float current = rawI * CURRENT_SCALE;
      Serial.print(F("Voltage: "));
      Serial.print(voltage, 1);
      Serial.print(F(" V | Current: "));
      Serial.print(current, 2);
      Serial.print(F(" A | Load: "));
      Serial.println(loadConnected ? F("CONNECTED") : F("DISCONNECTED"));
      Serial.print(F("Thresholds -> OV: "));
      Serial.print(overVoltage, 0);
      Serial.print(F(" UV: "));
      Serial.print(underVoltage, 0);
      Serial.print(F(" OC: "));
      Serial.println(overCurrent, 1);
    }
    else if (cmd.startsWith("SET_OV ")) {
      float val = cmd.substring(7).toFloat();
      if (val > 100 && val < 400) {
        overVoltage = val;
        Serial.print(F("Over-voltage set to "));
        Serial.println(overVoltage);
      } else {
        Serial.println(F("Invalid value (100-400)"));
      }
    }
    else if (cmd.startsWith("SET_UV ")) {
      float val = cmd.substring(7).toFloat();
      if (val > 50 && val < 250) {
        underVoltage = val;
        Serial.print(F("Under-voltage set to "));
        Serial.println(underVoltage);
      } else {
        Serial.println(F("Invalid value (50-250)"));
      }
    }
    else if (cmd.startsWith("SET_OC ")) {
      float val = cmd.substring(7).toFloat();
      if (val > 0.1 && val < 20) {
        overCurrent = val;
        Serial.print(F("Over-current set to "));
        Serial.println(overCurrent);
      } else {
        Serial.println(F("Invalid value (0.1-20)"));
      }
    }
    else if (cmd == "CONNECT") {
      loadConnected = true;
      digitalWrite(RELAY_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println(F("Load force-connected"));
    }
    else if (cmd == "DISCONNECT") {
      loadConnected = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println(F("Load force-disconnected"));
    }
    else {
      Serial.println(F("Unknown command. Type STATUS for help."));
    }
  }
}
