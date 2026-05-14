#include <Arduino.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <Wire.h>

constexpr uint8_t BH1750_SDA_PIN = 21;
constexpr uint8_t BH1750_SCL_PIN = 22;
constexpr uint8_t BMP280_SDA_PIN = 18;
constexpr uint8_t BMP280_SCL_PIN = 19;
constexpr uint8_t BMP280_I2C_ADDRESS = 0x76;

BH1750 lightMeter;
TwoWire bmpWire = TwoWire(1);
Adafruit_BMP280 bmp280(&bmpWire);

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(BH1750_SDA_PIN, BH1750_SCL_PIN);
  bool sensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  if (sensorReady) {
    Serial.println("BH1750 initialized on SDA=21, SCL=22");
  } else {
    Serial.println("BH1750 not found. Check wiring and power.");
  }

  bmpWire.begin(BMP280_SDA_PIN, BMP280_SCL_PIN);
  bool bmpReady = bmp280.begin(BMP280_I2C_ADDRESS);
  if (bmpReady) {
    Serial.println("BMP280 initialized on SDA=18, SCL=19");
  } else {
    Serial.println("BMP280 not found. Check wiring, CS->3.3V, and address.");
  }
}

void loop() {
  float lux = lightMeter.readLightLevel();
  float temperatureC = bmp280.readTemperature();
  float pressureHpa = bmp280.readPressure() / 100.0f;

  Serial.print("Light: ");
  Serial.print(lux);
  Serial.print(" lx | Temp: ");
  Serial.print(temperatureC);
  Serial.print(" C | Pressure: ");
  Serial.print(pressureHpa);
  Serial.println(" hPa");
  delay(10);
}