#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <BleGamepad.h>
#include <PCF8574.h>
#include <Preferences.h>
#include <GTimer.h>
GTimer<millis> bat_tmr;

#define I2C_SDA 8
#define I2C_SCL 9

#define LX_PIN 0
#define LY_PIN 1
#define RX_PIN 2
#define RY_PIN 3
#define BAT_PIN 4

Preferences prefs;
struct Config {
    int lx_off, ly_off, rx_off, ry_off;
    int deadzone = 800; 
} cfg;

PCF8574 pcf1(0x20);
PCF8574 pcf2(0x24);

#define numOfButtons 13
BleGamepad bleGamepad;
BleGamepadConfiguration bleGamepadConfig;

float getBatteryVoltage() {
  long sum = 0;
  for (int i = 0; i < 128; i++) {
    sum += analogRead(BAT_PIN);
  }
  float raw = sum / 128.0;
  
  float voltage = (raw / 4095.0) * 3.3 * 2.0;

  return voltage;
}

/*
int processStick(int pin) {
  long sum = 0;
  const int samples = 16; 

  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
  }
  
  int raw = sum / samples; 
  int center = 2048; 
  int deadzone = 160; 

  if (abs(raw - center) < deadzone) {
    return 16384; 
  }
  return map(raw, 0, 4095, 32767, 0);
}
*/

int processStick(int pin, int offset) {
  long sum = 0;
  
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
  }
  
  int raw = sum / 16; 
  int deadzone = 100;

  if (abs(raw - offset) < deadzone) {
    return 16384; 
  }
  
  int result;
  if (raw < offset) {
    result = map(raw, 0, offset, 32767, 16384);
  } else {
    result = map(raw, offset, 4095, 16384, 0);
  }

  return constrain(result, 0, 32767);
}

/*
void pcf_test() {
  PCF8574::DigitalInput DI1 = pcf1.digitalReadAll();
  PCF8574::DigitalInput DI2 = pcf2.digitalReadAll();

  Serial.print("PCF1 (0x20): [");
  Serial.print(DI1.p0); Serial.print(DI1.p1); Serial.print(DI1.p2); Serial.print(DI1.p3);
  Serial.print(DI1.p4); Serial.print(DI1.p5); Serial.print(DI1.p6); Serial.print(DI1.p7);
  Serial.print("] ");

  Serial.print("PCF2 (0x24): [");
  Serial.print(DI2.p0); Serial.print(DI2.p1); Serial.print(DI2.p2); Serial.print(DI2.p3);
  Serial.print(DI2.p4); Serial.print(DI2.p5); Serial.print(DI2.p6); Serial.print(DI2.p7);
  Serial.println("]");

  delay(200); 
}

void i2c_scanner(){
    byte error, address;
    int nDevices;
 
    Serial.println("Scanning...");
 
    nDevices = 0;
    for(address = 8; address < 127; address++ ){
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
 
        if (error == 0){
            Serial.print("I2C device found at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.print(address,HEX);
            Serial.println(" !");
 
            nDevices++;
        }
        else if (error==4) {
            Serial.print("Unknow error at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.println(address,HEX);
        } 
    }
    if (nDevices == 0)
        Serial.println("No I2C devices found\n");
    else
        Serial.println("done\n");
 
    delay(5000);
}

void test_sticks() {
  int lx = processStick(LX_PIN, cfg.lx_off);
  int ly = processStick(LY_PIN, cfg.ly_off);
  int rx = processStick(RX_PIN, cfg.rx_off);
  int ry = processStick(RY_PIN, cfg.ry_off);
  Serial.print("LX = ");
  Serial.print(lx);
  Serial.print(" LY = ");
  Serial.print(ly);
  Serial.print(" RX = ");
  Serial.print(rx);
  Serial.print(" RY = ");
  Serial.println(ry);
  delay(1000);
}

*/

void checkBattery() {
  float v = getBatteryVoltage();
  int batteryLevel = map(v * 100, 340, 420, 0, 100);
  batteryLevel = constrain(batteryLevel, 0, 100);
  bleGamepad.setBatteryLevel(batteryLevel);
  bleGamepad.sendReport();
  //Serial.print("Battery: ");
  //Serial.print(v);
  //Serial.print("V (");
  //Serial.print(batteryLevel);
  //Serial.println("%)");
}

void calibrateSticks() {
    Serial.println("Calibrating...");
    long lx = 0, ly = 0, rx = 0, ry = 0;
    
    for (int i = 0; i < 100; i++) {
        lx += analogRead(LX_PIN);
        ly += analogRead(LY_PIN);
        rx += analogRead(RX_PIN);
        ry += analogRead(RY_PIN);
        delay(10);
    }
    
    cfg.lx_off = lx / 100;
    cfg.ly_off = ly / 100;
    cfg.rx_off = rx / 100;
    cfg.ry_off = ry / 100;
    
    prefs.putInt("lx", cfg.lx_off);
    prefs.putInt("ly", cfg.ly_off);
    prefs.putInt("rx", cfg.rx_off);
    prefs.putInt("ry", cfg.ry_off);
    
    Serial.println("Saved! Rebooting...");
    ESP.restart();
}

void gamepad() {
  if (bleGamepad.isConnected())
  {
    static int prev_lx = -1, prev_ly = -1, prev_rx = -1, prev_ry = -1;
    static uint8_t prev_pcf1 = 0xFF, prev_pcf2 = 0xFF;
    static byte prev_btn13 = HIGH;

    // 2. Считываем текущие значения стиков
  int lx = processStick(LX_PIN, cfg.lx_off);
  int ly = processStick(LY_PIN, cfg.ly_off);
  int rx = processStick(RX_PIN, cfg.rx_off);
  int ry = processStick(RY_PIN, cfg.ry_off);
    
    PCF8574::DigitalInput DI1 = pcf1.digitalReadAll();
    PCF8574::DigitalInput DI2 = pcf2.digitalReadAll();
    byte current_btn13 = digitalRead(10);

    uint8_t current_pcf1 = (DI1.p7 << 7) | (DI1.p6 << 6) | (DI1.p5 << 5) | (DI1.p4 << 4) | (DI1.p3 << 3) | (DI1.p2 << 2) | (DI1.p1 << 1) | DI1.p0;
    uint8_t current_pcf2 = (DI2.p7 << 7) | (DI2.p6 << 6) | (DI2.p5 << 5) | (DI2.p4 << 4) | (DI2.p3 << 3) | (DI2.p2 << 2) | (DI2.p1 << 1) | DI2.p0;

    if (lx != prev_lx || ly != prev_ly || rx != prev_rx || ry != prev_ry || 
        current_pcf1 != prev_pcf1 || current_pcf2 != prev_pcf2 || current_btn13 != prev_btn13) 
    {

      bleGamepad.setAxes(lx, ly, 0, 0, rx, ry, 0, 0);
      //bleGamepad.setLeftThumb(lx, ly);
      //bleGamepad.setRightThumb(rx, ry);
      
      bool up    = (DI1.p0 == LOW);
      bool left  = (DI1.p1 == LOW);
      bool down  = (DI1.p2 == LOW);
      bool right = (DI1.p3 == LOW);

      // Логика хатки
      if (up && right) bleGamepad.setHat1(HAT_UP_RIGHT);
      else if (up && left) bleGamepad.setHat1(HAT_UP_LEFT);
      else if (down && right) bleGamepad.setHat1(HAT_DOWN_RIGHT);
      else if (down && left) bleGamepad.setHat1(HAT_DOWN_LEFT);
      else if (up) bleGamepad.setHat1(HAT_UP);
      else if (down) bleGamepad.setHat1(HAT_DOWN);
      else if (left) bleGamepad.setHat1(HAT_LEFT);
      else if (right) bleGamepad.setHat1(HAT_RIGHT);
      else bleGamepad.setHat1(HAT_CENTERED); 

      // Логика кнопок
      if (DI1.p4 == LOW) bleGamepad.press(BUTTON_9); else bleGamepad.release(BUTTON_9);
      if (DI1.p5 == LOW) bleGamepad.press(BUTTON_12); else bleGamepad.release(BUTTON_12);
      if (DI1.p6 == LOW) bleGamepad.press(BUTTON_7); else bleGamepad.release(BUTTON_7);
      if (DI1.p7 == LOW) bleGamepad.press(BUTTON_5); else bleGamepad.release(BUTTON_5);
      
      if (DI2.p0 == LOW) bleGamepad.press(BUTTON_6); else bleGamepad.release(BUTTON_6);
      if (DI2.p1 == LOW) bleGamepad.press(BUTTON_8); else bleGamepad.release(BUTTON_8);
      if (DI2.p2 == LOW) bleGamepad.press(BUTTON_11); else bleGamepad.release(BUTTON_11);
      if (DI2.p3 == LOW) bleGamepad.press(BUTTON_10); else bleGamepad.release(BUTTON_10);
      if (DI2.p4 == LOW) bleGamepad.press(BUTTON_4); else bleGamepad.release(BUTTON_4);
      if (DI2.p5 == LOW) bleGamepad.press(BUTTON_3); else bleGamepad.release(BUTTON_3);
      if (DI2.p6 == LOW) bleGamepad.press(BUTTON_1); else bleGamepad.release(BUTTON_1);
      if (DI2.p7 == LOW) bleGamepad.press(BUTTON_2); else bleGamepad.release(BUTTON_2);
      if (current_btn13 == LOW) bleGamepad.press(BUTTON_13); else bleGamepad.release(BUTTON_13);
      
      bleGamepad.sendReport();
      //Serial.println("sendReport");

      prev_lx = lx; prev_ly = ly; prev_rx = rx; prev_ry = ry;
      prev_pcf1 = current_pcf1;
      prev_pcf2 = current_pcf2;
      prev_btn13 = current_btn13;
      bat_tmr.start();
    }

    delay(10); 
  }
}

void setup()
{
  setCpuFrequencyMhz(80);
  delay(500);
  Serial.begin(9600);
  delay(500);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(4, INPUT);
  pinMode(10, INPUT_PULLUP);
  delay(100);
  Wire.begin(I2C_SDA, I2C_SCL);
  for(int i=0; i<8; i++) {
    pcf1.pinMode(i, INPUT_PULLUP);
    pcf2.pinMode(i, INPUT_PULLUP);
  }
  pcf1.begin();
  pcf2.begin();
  delay(100);
  prefs.begin("settings", false);
  
  cfg.lx_off = prefs.getInt("lx", 0);
  cfg.ly_off = prefs.getInt("ly", 0);
  cfg.rx_off = prefs.getInt("rx", 0);
  cfg.ry_off = prefs.getInt("ry", 0);

  if (digitalRead(10) == LOW) {
      calibrateSticks();
  }
  bat_tmr.setMode(GTMode::Interval);
  bat_tmr.setTime(5000);
  WiFi.mode(WIFI_OFF);
  Serial.println("Starting BLE work!");
  bleGamepadConfig.setAutoReport(false);
  bleGamepadConfig.setHatSwitchCount(1);
  bleGamepadConfig.setButtonCount(numOfButtons);
  bleGamepad.begin(&bleGamepadConfig);
  bat_tmr.start();
}

void loop()
{
  gamepad();
  if (bat_tmr) checkBattery();
  //i2c_scanner();
  //pcf_test();
  //test_sticks();
}