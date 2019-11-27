
#include <M5Stack.h>
#include "BLEDevice.h"
#include "sqlite.h"
uint8_t seq;                     // remember number of boots in RTC Memory
#define MyManufacturerId 0xffff  // test manufacturer ID
#define S_PERIOD 1               // Silent period

static BLEUUID service_uuid("");
static BLEUUID char_uuid("");

static BLEAddress* p_server_address;
static boolean do_connect = false;

BLEScan* p_ble_scan;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void _show_logs();

void write_sd_card(time_t time) {
  File file = SD.open("/log.csv", FILE_APPEND);
  file.printf("%ld\n", time);
  file.close();
}

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
    int manu = data[1] << 8 | data[0];

    if (manu != MyManufacturerId ||
        seq == data[2]) {  // カンパニーIDが0xFFFFで、
      continue;
    }  // シーケンス番号が新しいものを探す

    seq = data[2];
    time_t time =
        (time_t)(data[6] << 24 | data[5] << 16 | data[4] << 8 | data[3]);
    struct tm* now = localtime(&time);
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 0);
    M5.Lcd.printf("seq: %d\r\n", seq);
    M5.Lcd.printf("tm: %d:%d:%d'\r\n", now->tm_hour, now->tm_min, now->tm_sec);
    insert_db(time, seq);
  }
  if (M5.BtnB.wasPressed()) {
    show_logs();
  }
}

void IRAM_ATTR _show_logs() { show_logs(); }