#include <WiFi.h>
#include "Get_Data.h"
#include "esp_deep_sleep.h"
#include "ESP32_BME280_SPI.h"
#include <WiFiClientSecure.h>
#include <Arduino_JSON.h>
#include <HTTPClient.h>
#define JST 3600*9 //日本標準時子午線
#define uS_TO_S_FACTOR 1000000ULL //マイクロ->秒に変換係数
#define TIME_TO_SLEEP 5 //ディープスリープ時間()

HTTPClient http;
WiFiClientSecure client;

//WiFi設定
const char ssid[] = "SSID";
const char password[] = "パスワード";

int soilPin = A0; //土壌センサー制御ピン
int soilPower = 7; //土壌水分用変数
int motPin = 16; //モーター制御ピン
int ledPin = 4; //LEDピン
int swPin = 15; //スイッチピン
bool flag = false; //チャタリング対策
bool buttonOnOff = false; //ボタンの状態
bool deepSleepFlag = false; //ディープスリープフラグ

//SPI通信用変数設定
const uint8_t SCLK_bme280 = 14; //クロック入力
const uint8_t MOSI_bme280 = 13; //マスター(ESP32):出力,スレーブ(BME280):入力
const uint8_t MISO_bme280 = 12; //マスター(ESP32):入力、スレーブ(BME280):出力
const uint8_t CS_bme280 = 26; //チップ選択

ESP32_BME280_SPI bme280spi(SCLK_bme280, MOSI_bme280, MISO_bme280, CS_bme280, 10000000);

//各種BME280測定モード設定用変数設定
uint8_t t_sb = 5; //測定間でのスタンバイ時間 (5==1000ms)
uint8_t filter = 0; //測定精度(0==off)
uint8_t osrs_t = 4; //温度のオーバーサンプリング値 Temperature x4 ノイズ影響を抑える
uint8_t osrs_p = 4; //気圧のオーバーサンプリング値 Pressure x4
uint8_t osrs_h = 4; //湿度のオーバーサンプリング値 Humidity x4
uint8_t Mode = 3; //計測=ノーマルモード

JSONVar myThreshold;

const char* root_ca = "証明書";
                    

const char host[] = "webページ";

void setup()
{
  Serial.begin(115200);
  pinMode(soilPower, OUTPUT);//設定
  pinMode(motPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(swPin, INPUT);
  digitalWrite(soilPower, LOW);//センサーに電流が流れないようにする

  bme280spi.ESP32_BME280_SPI_Init(t_sb, filter, osrs_t, osrs_p, osrs_h, Mode);

  //WiFi接続
  Serial.print("Connecting to");
  Serial.print(ssid);
  WiFi.begin(ssid, password);
  int i = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //通信状況が悪い時リスタート
    if (++i > 30) {
      Serial.print("リスタート");
      esp_deep_sleep_enable_timer_wakeup(1 * 1000 * 1000);
      esp_deep_sleep_start();
    }
  }

  Serial.println("Connected");
  //SSH証明書の設定
  client.setCACert(root_ca);
  //日本時間の設定
  configTime( JST, 0, "ntp.nict.jp","time.google.com","ntp.jst.mfeed.ad.jp");
  configTzTime("JST-9", "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");
  //取得データの送信
  sendPostMoisture(); 
}

//データ送信
void sendMoisture() {
  String url = "/kainouji/hard/DataRecive.php";
  if (client.connect(host, 443)) {
    Serial.println("Kainoji Connect");
    int bmeData[3];
    getBmeData(bmeData);
    String data;
    data += "moisture=";
    data += readSoil(soilPin, soilPower);
    data += "&temerature=";
    data += bmeData[0];
    data += "&humidity=";
    data += bmeData[1];
    data += "&air_pressure=";
    data += bmeData[2];
    data += "&chip_id=";
    data += getChipId();

    //ヘッダー情報
    client.println("GET " + url + "?" + data + " HTTP/1.1");
    client.println("Host: " + (String)host);
    client.println("User-Agent: ESP8266/1.0");
    client.println("Connection: close");
    client.println();
    client.println(data);
    delay(10);
  } else {
    Serial.println("Client Connect 失敗");
  }
}

void sendPostMoisture() {
  String url = "/kainouji/hard/DataRecive.php";
  if (client.connect(host, 443)) {
    Serial.println("Kainoji Connect");
    int bmeData[3];
    getBmeData(bmeData);
    String data;
    data += "moisture=";
    data += readSoil(soilPin, soilPower);
    data += "&temperature=";
    data += bmeData[0];
    data += "&humidity=";
    data += bmeData[1];
    data += "&air_pressure=";
    data += bmeData[2];
    data += "&chip_id=";
    data += getChipId();
    client.println("POST " + url + "?" + data + " HTTP/1.1");
    client.println("Host: " + (String)host);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.println("User-Agent: ESP8266/1.0");
    client.print("Content-Length: ");
    client.println(data.length());
    client.println("Connection: close");
    client.println();
    client.println(data);

    client.stop();
    delay(1000);
  } else {
    Serial.println("Client Connect 失敗");
  }
}

//データの閾値を取得
void getThresholdData() {
  String url = "webページ/kainouji/hard/GetThresholdData.php?chip_id="+String(getChipId());
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.printf("[HTTP] GET...Code:%d", http.GET());
  if (httpCode > 0) {
    myThreshold = JSON.parse(http.getString());
  }
  http.end();
}

void loop()
{
  time_t t;
  struct tm *tm;
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
  t = time(NULL);
  tm = localtime(&t);
  Serial.printf(" %04d/%02d/%02d(%s) %02d:%02d:%02d\n",
                tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                wd[tm->tm_wday],
                tm->tm_hour, tm->tm_min, tm->tm_sec);

  unsigned long startTime = 6;
  unsigned long endTime = 13;
  unsigned long currentTime = tm->tm_hour;
  unsigned long compareTime = endTime - currentTime;
  if (compareTime == 0) {
    Serial.println("水をやるタイミングです");
  } else {
    Serial.println("水をあげないタイミングです");
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                   " Seconds");
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start();
  }
//  delay(1000);

  getThresholdData();

  int buttonState = digitalRead(swPin);
  int moist = readSoil(soilPin, soilPower);
  Serial.println(myThreshold["moisture"]);
  int thresholdMoisture = (int)myThreshold["moisture"];

  //ONOFFプルアップ(0:押す,1:離す)
  if (buttonState == 0 && !flag) {
    if (!buttonOnOff)buttonOnOff = true;
    else buttonOnOff = false;
    flag = true;
  } else if (buttonState == 1 && flag) {
    flag = false;
  }

  if (buttonOnOff) Serial.println("true");
  else Serial.println("false");

  //  //モーターを回す(閾値以下かボタンの状態)
  if (buttonOnOff || moist < thresholdMoisture) {
    digitalWrite(motPin, HIGH);
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(motPin, LOW);
    digitalWrite(ledPin, LOW);
    deepSleepFlag = true;
  }
  //
  Serial.println(moist);
  delay(1000);
  if (deepSleepFlag) {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) +
                   " Seconds");
    Serial.println("Going to sleep now");
    Serial.flush();
    esp_deep_sleep_start();
  }
}
