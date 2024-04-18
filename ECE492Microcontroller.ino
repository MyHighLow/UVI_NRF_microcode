#include <bluefruit.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "TimerInterrupt_Generic.h"
#include "ISR_Timer_Generic.h"


BLEService environmentalSensingService = BLEService(UUID16_SVC_ENVIRONMENTAL_SENSING);
BLECharacteristic uvIndexSensor = BLECharacteristic(UUID16_CHR_UV_INDEX);
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ SCL, /* data=*/ SDA, /* reset=*/ U8X8_PIN_NONE);
BLEDis bledis;  // DIS (Device Information Service) helper class instance

uint8_t outputrounded = 0;
uint8_t takeMeasNum = 0;
uint8_t timeOutNum = 0;
uint8_t sleep = 0;

NRF52Timer myTimer(NRF_TIMER_1);

ISR_Timer NRF52_ISR_Timer;

#define HW_TIMER_INTERVAL_MS 50L

#define TIMER_INTERVAL_1M    60000L
#define TIMER_INTERVAL_30S   30000L
#define TIMER_INTERVAL_15S   15000L

void myHandler() {
  NRF52_ISR_Timer.run();
}

void takeMeas() {
  float total = 0;
  float uv_value = 0;
  float output = 0.0;

  for (int i = 0; i < 100; i++){
    uv_value = analogRead(A1);
    uv_value = (uv_value / 1023.0) * 3.3;
    //Serial.println(uv_value);
    total = total + uv_value;
  }

  output = total / 100;

  output = output / 0.1;
                                                             
  outputrounded = round(output);
}


void timeOut() {
  //NRF_POWER->SYSTEMOFF = 1;
  sleep = 1;
  u8g2.setPowerSave(sleep);
  outputrounded = 5;
}

void wakeUp() {
  NRF52_ISR_Timer.restartTimer(timeOutNum);
  takeMeas();
  sleep = !sleep;
  u8g2.setPowerSave(sleep);


}


void setup() {
  // put your setup code here, to run once:
  Bluefruit.begin();
  Bluefruit.Periph.setConnectCallback(connect_callback);
  Bluefruit.Periph.setDisconnectCallback(disconnect_callback);
  bledis.setManufacturer("Seeed Studio");
  bledis.setModel("UV Index Sensor");
  bledis.begin();
  environmentalSensingService.begin();
  uvIndexSensor.setProperties(CHR_PROPS_NOTIFY);
  uvIndexSensor.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  uvIndexSensor.setFixedLen(1);
  uvIndexSensor.begin();
  uint8_t uvIndexData[1] = { 0x04 };  // Set the characteristic to use 4-bit values, with the sensor connected and detected
  uvIndexSensor.write(uvIndexData, 1);
  startAdv();
  u8g2.begin();
  pinMode(D7,INPUT);
  attachInterrupt(digitalPinToInterrupt(D7),wakeUp,FALLING);

  if (myTimer.attachInterruptInterval(HW_TIMER_INTERVAL_MS*1000, myHandler)){
    Serial.println("myTimer launched");
  }
  else {
    Serial.println("Cannot set the timer");
  }

  takeMeasNum = NRF52_ISR_Timer.setInterval(TIMER_INTERVAL_15S, takeMeas);
  timeOutNum = NRF52_ISR_Timer.setInterval(TIMER_INTERVAL_30S, timeOut);
}


void startAdv(void) {
  // Advertising packet
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);

  // Include HRM Service UUID
  Bluefruit.Advertising.addService(environmentalSensingService);

  Bluefruit.setName("SafeSun");

  // Include Name
  Bluefruit.Advertising.addName();

  /* Start Advertising
   * - Enable auto advertising if disconnected
   * - Interval:  fast mode = 20 ms, slow mode = 152.5 ms
   * - Timeout for fast mode is 30 seconds
   * - Start(timeout) with timeout = 0 will advertise forever (until connected)
   * 
   * For recommended advertising interval
   * https://developer.apple.com/library/content/qa/qa1931/_index.html   
   */
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);  // in unit of 0.625 ms
  Bluefruit.Advertising.setFastTimeout(30);    // number of seconds in fast mode
  Bluefruit.Advertising.start(0);              // 0 = Don't stop advertising after n seconds
}


void connect_callback(uint16_t conn_handle) {
  // Get the reference to current connection
  BLEConnection* connection = Bluefruit.Connection(conn_handle);

  char central_name[32] = { 0 };
  connection->getPeerName(central_name, sizeof(central_name));

  //Serial.print("Connected to ");
  //Serial.println(central_name);
}

/**
 * Callback invoked when a connection is dropped
 * @param conn_handle connection where this event happens
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason) {
  (void)conn_handle;
  (void)reason;

  //Serial.print("Disconnected, reason = 0x");
  //Serial.println(reason, HEX);
  //Serial.println("Advertising!");
}

void loop() {
  // put your main code here, to run repeatedly:
  /*
    float total = 0;
    float uv_value = 0;
    float output = 0.0;

    for (int i = 0; i < 100; i++){
        uv_value = analogRead(A1);
        uv_value = (uv_value / 1023.0) * 3.3;
        //Serial.println(uv_value);
        total = total + uv_value;
    }

    output = total / 100;

    output = output / 0.1;

    uint8_t outputrounded = round(output);
*/

  u8g2.clearBuffer();
  u8g2.drawFrame(32, 16, 64, 48);
  u8g2.setFont(u8g2_font_unifont_tr);
  u8g2.setCursor(59, 50);
  u8g2.print(outputrounded);
  u8g2.setFont(u8g2_font_u8glib_4_tf);
  u8g2.setCursor(48, 60);
  u8g2.print(F("UV INDEX"));
  u8g2.sendBuffer();


  uint8_t appValue = outputrounded; // use round() to get nearest whole integer value so app can handle it (this variable needs to be uint8_t for app to handle formating correctly)
  uint8_t hrmdata[1] = { appValue }; // sends uint8_t value over BLE  
  uvIndexSensor.notify(hrmdata, sizeof(hrmdata));
  //Serial.println(appValue);


  delay(5000);  
}
