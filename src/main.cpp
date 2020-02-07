
#include <M5Stack.h>
#include <unordered_map>
#include "../lib/M5Servo.h"
#include "BLEDevice.h"
#include "sqlite.h"

uint8_t seq;  // remember number of boots in RTC Memory
const uint32_t MyManufacturerId = 0xffff;
const gpio_num_t PIN_SERVO = gpio_num_t::GPIO_NUM_5;
const gpio_num_t PIN_ROTATE_SENSOR = gpio_num_t::GPIO_NUM_35;

static BLEUUID service_uuid("0xff");
static BLEUUID char_uuid("0xff");

static BLEAddress* p_server_address;
static boolean do_connect = false;

std::unordered_map<std::string, uint8_t> child_state;
BLEScan* p_ble_scan;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void _show_logs();
void rotate_servo();

enum KeyState { OPEN, CLOSE };

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertised_device) {
    if (!advertised_device.haveServiceUUID()) return;
    if (!advertised_device.getServiceUUID().equals(BLEUUID(service_uuid)))
      return;

    advertised_device.getScan()->stop();
    p_server_address = new BLEAddress(advertised_device.getAddress());
    do_connect = true;
  }
};

void setup() {
  M5.begin();
  SPI.begin();
  SD.begin();
  M5.Lcd.setTextSize(2);

  // pinMode(BUTTON_B_PIN, INPUT);
  sqlite3_initialize();
  create_table();
  // attachInterrupt(BUTTON_B_PIN, _show_logs, CHANGE);

  BLEDevice::init("");

  p_ble_scan = BLEDevice::getScan();
  p_ble_scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  p_ble_scan->setActiveScan(false);
}
void loop() {
  M5.update();

  BLEScanResults foundDevices = p_ble_scan->start(1);  // スキャンする
  int count = foundDevices.getCount();

  for (int i = 0; i < count; i++) {
    BLEAdvertisedDevice d = foundDevices.getDevice(i);
    if (!d.haveManufacturerData()) {
      continue;
    }

    std::string data = d.getManufacturerData();
    int manu = data.at(1) << 8 | data.at(0);

    if (manu != MyManufacturerId) {  // カンパニーIDが0xFFFFで、
      continue;
    }  // シーケンス番号が新しいものを探す
    seq = data.at(2);
    time_t time = (time_t)(data.at(6) << 24 | data.at(5) << 16 |
                           data.at(4) << 8 | data.at(3));
    uint8_t str_len = data.at(7);

    std::string user_name;

    for (int i = 0; i < str_len; i++) {
      user_name += data.at(i + 8);
    }

    if (child_state.count(user_name) > 0) {
      if (child_state.at(user_name) == seq) {
        continue;
      }
    }

    child_state.insert({user_name, seq});

    struct tm* now = localtime(&time);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("len: %d,name: %s\r\n", str_len, user_name.c_str());
    M5.Lcd.printf("seq: %d\r\n", seq);
    M5.Lcd.printf("time_t : %ld\n", time);
    M5.Lcd.printf("tm: %d/%d/%d %d:%d:%d'\r\n", now->tm_year + 1900,
                  now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min,
                  now->tm_sec);
    insert_db(user_name, time, seq);
    rotate_servo();
  }

  if (M5.BtnB.wasPressed()) {
    show_logs();
  }
  if (M5.BtnC.wasPressed()) {
    drop_table();
    create_table();
  }
}

void IRAM_ATTR _show_logs() { show_logs(); }

void rotate_servo() {
  M5Servo servo;
  servo.attach(PIN_SERVO);

  static enum KeyState key_state;
  uint16_t rotate_sensor = analogRead(PIN_ROTATE_SENSOR);

  M5.Lcd.setCursor(0, 200);

  if (rotate_sensor > 1024 / 2) {
    key_state = OPEN;
    M5.Lcd.printf("OPEN -> ");
  } else {
    key_state = CLOSE;
    M5.Lcd.printf("CLOSE -> ");
  }

  switch (key_state) {
    case OPEN:
      servo.write(0);
      key_state = CLOSE;
      M5.Lcd.printf("CLOSE");
      break;
    case CLOSE:
      servo.write(90);
      key_state = OPEN;
      M5.Lcd.printf("OPEN");

      break;
  }
}
