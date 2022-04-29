#include "Get_Data.h"
#include <WiFiClientSecure.h>
#include "esp_deep_sleep.h"

void bme_get(){
  byte temperature = (byte)round(bme280spi.Read_Temperature());
  byte humidity = (byte)round(bme280spi.Read_Humidity());
  uint16_t pressure = (uint16_t)round(bme280spi.Read_Pressure());

  char temp_c[10], hum_c[10], pres_c[10];
  sprintf(temp_c, "%2d ℃", temperature);
  sprintf(hum_c, "%2d ％", humidity);
  sprintf(pres_c, "%4d hPa", pressure);

  Serial.println("-----------------------");
  Serial.print("Temperature = "); Serial.println(temp_c);
  Serial.print("Humidity = "); Serial.println(hum_c);
  Serial.print("Pressure = "); Serial.println(pres_c);
  Serial.println("-----------------------");
  Serial.flush();
}

void getBmeData(int bmeData[3]){
  byte temperature = (byte)round(bme280spi.Read_Temperature());
  byte humidity = (byte)round(bme280spi.Read_Humidity());
  uint16_t pressure = (uint16_t)round(bme280spi.Read_Pressure());
  bmeData[0] = temperature;
  bmeData[1] = humidity;
  bmeData[2] = pressure;
}

byte getTemerature(){
  byte temperature = (byte)round(bme280spi.Read_Temperature());
  byte humidity = (byte)round(bme280spi.Read_Humidity());
  uint16_t pressure = (uint16_t)round(bme280spi.Read_Pressure());
  int bmeData[] = {temperature,humidity,pressure};
  return temperature;
}

byte getHumidity(){
  byte humidity = (byte)round(bme280spi.Read_Humidity());
  return humidity;
}

uint16_t getAirPressure(){
  uint16_t airPressure = (uint16_t)round(bme280spi.Read_Pressure());
  return airPressure;
}

uint32_t getChipId(){
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  return chipId;
}

int readSoil(int soilPin, int soilPower) {
  delay(10);//wait 10 milliseconds
  int val = analogRead(soilPin);//Read the SIG value form sensor
  int moist = map(val, 0, 4095, 0, 99);
  moist = 100 - moist;
  return moist;
}

void WifiSet(const char *ssid,const char *password){
  //WiFi接続
  Serial.print("Connecting to");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  int wifi_count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //通信状況が悪い時リスタート
    if (++wifi_count > 30) {
      Serial.print("リスタート");
      esp_deep_sleep_enable_timer_wakeup(1 * 1000 * 1000);
      esp_deep_sleep_start();
    }
  }
  Serial.println("Connected");
}
