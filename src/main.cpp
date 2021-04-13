#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobot_CCS811.h"
#include "DFRobot_BME280.h"
#include <EEPROM.h>
#include "button.h"

#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 32    // OLED display height, in pixels
#define OLED_RESET 4        // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
#define SEA_LEVEL_PRESSURE 1015.0f
#define MEASUREMENT_INTERVAL 5000
#define GENERAL_DELAY 5000
#define BASELINE_AGE_MAX 24 // 24 hrs
#define TEXT_SIZE 2
#define Y_CUR 10
#define PX_PER_CHAR 6
#define BTN_PIN 3
#define EEPROM_ADDR 0

typedef DFRobot_BME280_IIC BME;
enum ModeEnum
{
  Calibrate,
  Standard
};

unsigned long lastMeasurement = millis();
unsigned long baselineAge = millis() / 1000 / 60 / 60;
int displayX;
int displayMinX;
String readout;
uint16_t baseline;
ModeEnum mode = Standard;
int minute = 0;
int second = 0;
int lastSecond = millis();

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobot_CCS811 CCS811(&Wire, /*IIC_ADDRESS=*/0x5A);
BME bme(&Wire, 0x76);
Button modeBtn = Button(BTN_PIN);

void writeText(String v);
void testscrolltext(void);
void printLastOperateStatus(BME::eStatus_t eStatus);
void onPress();
void onLongPress();
template <typename value>
void getSensorReading(String &readout, const char *heading, value v, String unit, bool isLast = false);
void updateTime();
void displayBaselineCalibration(String baselineValue);
void updateDisplay();

void setup()
{
  Serial.begin(9600);

  // Display Init
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 allocation failed");
    for (;;)
      ; // Don't proceed, loop forever
  }

  display.display();
  delay(2000); // Pause for 2 seconds

  display.clearDisplay();
  display.setTextSize(TEXT_SIZE);
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);
  displayX = display.width();

  // CCS811 Init
  while (CCS811.begin() != 0)
  {
    Serial.println("failed to init chip, please check if the chip connection is fine");
    delay(1000);
  }

  // BME Init
  while (bme.begin() != BME::eStatusOK)
  {
    Serial.println("bme begin faild");
    printLastOperateStatus(bme.lastOperateStatus);
    delay(2000);
  }

  // btn init
  modeBtn.OnPress(onPress);
  modeBtn.OnLongPress(onLongPress);

  // EEPROM init
  uint8_t eepromValue = EEPROM.read(EEPROM_ADDR);
  Serial.print("Stored Baseline: ");
  Serial.println(eepromValue, HEX);

  baseline = eepromValue;
}

void loop()
{
  unsigned long now = millis();
  int nowHours = now / 1000 / 60 / 60;

  if (mode == Standard && now - lastMeasurement > MEASUREMENT_INTERVAL)
    {
      lastMeasurement = now;
      readout = String();

      if (CCS811.checkDataReady())
      {
        if (mode == Standard)
        {
          float temp = bme.getTemperature();
          uint32_t press = bme.getPressure();
          float alti = bme.calAltitude(SEA_LEVEL_PRESSURE, press);
          float humi = bme.getHumidity();
          //baseline = CCS811.readBaseLine();

          getSensorReading(readout, "Temp", temp, "C");
          getSensorReading(readout, "Pressure", press, "PA");
          getSensorReading(readout, "Altitude", alti, "M");
          getSensorReading(readout, "Humidity", humi, "%");
          //getSensorReading(readout, "Baseline", String(baseline, HEX), "");
          getSensorReading(readout, "Baseline Age", nowHours - baselineAge, "HR(S)");
          getSensorReading(readout, "CO2", CCS811.getCO2PPM(), "PPM");
          getSensorReading(readout, "TVOC", CCS811.getCO2PPM(), "PPB", true);
        }
      }
      else
      {
        Serial.println("Data is not ready!");
      }

      if (nowHours - baselineAge > BASELINE_AGE_MAX)
      {
        //baselineAge = nowHours;

        //baselineAge = CCS811.readBaseLine();
        //CCS811.writeBaseLine(baseline);

        //Serial.print("New baseline: ");
        //Serial.println(String(baseline, HEX));
      }

      Serial.println(readout);
    }
    else if (mode == Calibrate)
    {
      displayBaselineCalibration(String(baseline, HEX));
    }

  // loop updates
  updateDisplay();
  modeBtn.Update();
  updateTime();
}

void updateDisplay() {
  displayMinX = -(PX_PER_CHAR * TEXT_SIZE) * readout.length();
  writeText(readout);
}

void updateTime()
{
  unsigned long now = millis();
  int nowSecond = now / 1000;

  if (nowSecond - lastSecond >= 1)
  {
    second++;
    lastSecond = nowSecond;

    if (second == 60)
    {
      minute++;
    }
  }

  if (second >= 60)
  {
    second = 0;
  }
}

void displayBaselineCalibration(String baselineValue)
{
  char formatted[256];
  char baselineChar[baselineValue.length()];
  baselineValue.toCharArray(baselineChar, baselineValue.length());

  sprintf(formatted, "%02d:%02d %s", minute, second, baselineChar);

  readout = String(formatted);
}

void onPress()
{
  minute = 0;
  second = 0;
  Serial.println("Calibrating baseline");
  displayBaselineCalibration(String(baseline, HEX));
  mode = Calibrate;
}

void onLongPress()
{
  if (CCS811.checkDataReady()) {
      baseline = CCS811.readBaseLine();
      EEPROM.write(EEPROM_ADDR, baseline);

      // write to ccs811

      readout = String("Saved!");
  }
  else {
    readout = String("Failed!");
  }
  
  displayX = 0;
  updateDisplay();
  delay(GENERAL_DELAY);
  mode = Standard;
}

template <typename value>
void getSensorReading(String &readout, const char *heading, value v, String unit, bool isLast = false)
{
  readout += heading;
  readout += ": ";
  readout += String(v);
  readout += unit;

  if (!isLast)
  {
    readout += ", ";
  }
}

void writeText(String v)
{
  display.clearDisplay();
  display.setCursor(displayX, Y_CUR);
  display.print(v);
  display.display();

  if (mode == Standard)
  {
    if (--displayX < displayMinX)
    {
      displayX = display.width();
    }
  }
  else
  {
    displayX = 0;
  }
}

// show last sensor operate status
void printLastOperateStatus(BME::eStatus_t eStatus)
{
  switch (eStatus)
  {
  case BME::eStatusOK:
    Serial.println("everything ok");
    break;
  case BME::eStatusErr:
    Serial.println("unknow error");
    break;
  case BME::eStatusErrDeviceNotDetected:
    Serial.println("device not detected");
    break;
  case BME::eStatusErrParameter:
    Serial.println("parameter error");
    break;
  default:
    Serial.println("unknow status");
    break;
  }
}

/*
Carbon Dioxide (PPM)    Effect on Human           TVOC Concentration (PPB)    Effect on Human
<500	                  Normal		                <50	                        Normal
500-1000	              A little uncomfortable		50-750	                    Anxious, uncomfortable
1000-2500	              Tired		                  750-6000	                  depressive, headache
2500-5000	              Unhealthy		              >6000	                      headache and other nerve problems
*/