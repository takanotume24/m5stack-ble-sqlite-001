
#include <M5Stack.h>
#include <unordered_map>
#include "../lib/M5Servo.h"
#include "BLEDevice.h"
#include "sqlite.h"

enum KeyState { KEY_OPEN, KEY_CLOSE };
enum DoorState { DOOR_OPEN, DOOR_CLOSE };

uint8_t seq;  // remember number of boots in RTC Memory
const uint32_t MyManufacturerId = 0xffff;
const gpio_num_t PIN_SERVO = gpio_num_t::GPIO_NUM_5;
const gpio_num_t PIN_ROTATE_SENSOR = gpio_num_t::GPIO_NUM_35;
const gpio_num_t PIN_DOOR_SENSOR = gpio_num_t::GPIO_NUM_2;

static BLEUUID service_uuid("0xff");
static BLEUUID char_uuid("0xff");

static BLEAddress* p_server_address;
static boolean do_connect = false;

std::unordered_map<std::string, uint8_t> child_state;
BLEScan* p_ble_scan;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

enum DoorState door_state_now;
enum DoorState door_state_before;

void _show_logs();
void key_change(enum KeyState);
enum DoorState get_door_state();

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

    child_state[user_name] = seq;

    struct tm* now = localtime(&time);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.setTextSize(5);
    M5.Lcd.printf("Hello,\n%s\n",user_name.c_str());
    M5.Lcd.printf(
      "\n"
      "%d/%d/%d \n"
      "%d:%d:%d'\r\n",
     now->tm_year + 1900,
                  now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min,
                  now->tm_sec);
    M5.Lcd.setTextSize(2);

    insert_db(user_name, time, seq);
    key_change(KeyState::KEY_OPEN);

  }

  if (M5.BtnB.wasPressed()) {
    show_logs();
  }
  if (M5.BtnC.wasPressed()) {
    drop_table();
    create_table();
  }
  M5.Lcd.setCursor(0, 100);

  door_state_now = get_door_state();

  if (door_state_now != door_state_before) {
    switch (door_state_now) {
      case DoorState::DOOR_OPEN:
        break;

      case DoorState::DOOR_CLOSE:
        key_change(KeyState::KEY_CLOSE);
        break;
    }
  }

  door_state_before = door_state_now;
}

enum DoorState get_door_state(){
    if (!digitalRead(PIN_DOOR_SENSOR)) {
    return DoorState::DOOR_OPEN;
  } else {
    return  DoorState::DOOR_CLOSE;
  }
}

void key_change(enum KeyState state) {
  M5Servo servo;
  servo.attach(PIN_SERVO);

  delay(1000);

  switch (state) {
    case KeyState::KEY_OPEN:
      servo.write(0);
      break;

    case KeyState::KEY_CLOSE:
      servo.write(90);
      break;
  }
  delay(1000);
  servo.detach();
}
