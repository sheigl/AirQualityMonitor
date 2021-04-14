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
#define Y_CUR 0
#define X_CUR 0
#define PX_PER_CHAR 6
#define BTN_PIN 3
#define EEPROM_ADDR 0
#define MAX_TIME_FOR_CALIBRATION 20

typedef DFRobot_BME280_IIC BME;
typedef void (*onSecondTick)();

enum ModeEnum
{
  Temperature,
  Pressure,
  Humidity,
  Altitude,
  CO2,
  VOC,
  BaselineAge,
  Calibrate
};

unsigned long lastMeasurement = millis();
unsigned long baselineAge = millis() / 1000 / 60 / 60;
//int displayX;
//int displayMinX;
String readout;
uint16_t baseline;
ModeEnum mode;
ModeEnum lastMode;
int minute = 0;
int second = 0;
int lastSecond = millis();
char *waiting = "...";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DFRobot_CCS811 CCS811(&Wire, /*IIC_ADDRESS=*/0x5A);
BME bme(&Wire, 0x76);
Button modeBtn = Button(BTN_PIN);
onSecondTick *onSecondTickCallbacks = nullptr;

void writeText(String v);
void testscrolltext(void);
void printLastOperateStatus(BME::eStatus_t eStatus);
void onPress();
void onLongPress();
template <typename value>
String formatSensorReading(const char *heading, value v, String unit);
void updateTime();
void displayBaselineCalibrationAndTime();
void updateDisplay();
void saveBaselineToEEPROM();
uint16_t readEEPROM();
void updateSensorReading();
void incrementMode();
void setMode(ModeEnum modeEnum);
void updateWaiting();

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
  //displayX = display.width();

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
  uint16_t eepromValue = readEEPROM();
  writeText("EEPROM: \r\n" + String(eepromValue, HEX));
  delay(2000);

  baseline = eepromValue;
  CCS811.writeBaseLine(baseline);
}

void loop()
{
  // loop updates
  updateSensorReading();
  updateDisplay();
  modeBtn.Update();
  updateTime();
}

void updateSensorReading()
{
  unsigned long now = millis();
  int nowHours = now / 1000 / 60 / 60;

  if (mode != Calibrate && now - lastMeasurement > MEASUREMENT_INTERVAL)
  {
    lastMeasurement = now;
    readout = String();

    if (CCS811.checkDataReady())
    {
      switch (mode)
      {
      case Temperature:
        readout = formatSensorReading("Temp", (bme.getTemperature() * (9/5) + 32), "F");
        break;
      case Pressure:
        readout = formatSensorReading("Pressure", bme.getPressure() / 100, "MB");
        break;
      case Humidity:
        readout = formatSensorReading("Humidity", bme.getHumidity(), "%");
        break;
      case Altitude:
        readout = formatSensorReading("Altitude", bme.calAltitude(SEA_LEVEL_PRESSURE, bme.getPressure()), "M");
        break;
      case CO2:
        readout = formatSensorReading("CO2", CCS811.getCO2PPM(), "PPM");
        break;
      case VOC:
        readout = formatSensorReading("TVOC", CCS811.getCO2PPM(), "PPB");
        break;
      case BaselineAge:
        readout = formatSensorReading("Baseline Age", nowHours - baselineAge, "HR(S)");
        break;
      case Calibrate:
        break;
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
      CCS811.writeBaseLine(baseline);
    }
  }
}

void updateDisplay()
{
  writeText(readout);

  /* displayMinX = -(PX_PER_CHAR * TEXT_SIZE) * readout.length();

  if (displayMinX > SCREEN_WIDTH)
  {
    if (--displayX < displayMinX)
    {
      displayX = display.width();
    }
  }
  else
  {
    displayX = 0;
  } */
}

void updateTime()
{
  unsigned long now = millis();
  int nowSecond = now / 1000;

  if (nowSecond - lastSecond >= 1)
  {
    second++;
    lastSecond = nowSecond;
    if (onSecondTickCallbacks != nullptr)
    {
      for (size_t i = 0; i < sizeof(onSecondTickCallbacks); i++)
      {
        onSecondTickCallbacks[i]();
      }
    }

    updateWaiting();

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

void displayBaselineCalibrationAndTime()
{
  String baselineValue = String(CCS811.readBaseLine(), HEX);
  char *fmt = "%02d:%02d %s %s";
  char baselineChar[baselineValue.length()];
  baselineValue.toCharArray(baselineChar, baselineValue.length());

  char *formatted = (char*)malloc(sizeof(char) * (strlen(fmt) + strlen(baselineChar)));

  sprintf(formatted, fmt, minute, second, "Baseline", baselineChar);

  readout = "Calibrating"; 
  readout += String(waiting);
  readout += "\r\n";
  readout += String(formatted);
  Serial.println(readout);
  updateDisplay();

  free(formatted);
}

uint16_t readEEPROM()
{
  Serial.println("Getting EEPROM value...");
  uint16_t baseline = (EEPROM.read(EEPROM_ADDR)) * 256;
  Serial.print("EEPROM highByte: ");
  Serial.println(baseline, HEX);

  uint8_t low = EEPROM.read(EEPROM_ADDR + 1); 
  baseline += low;

  Serial.print("EEPROM lowByte: ");
  Serial.println(low, HEX);

  Serial.print("EEPROM full: ");
  Serial.println(baseline, HEX);

  return baseline;
}

void saveBaselineToEEPROM()
{
  if (CCS811.checkDataReady())
  {
    Serial.println(baseline, HEX);

    baseline = CCS811.readBaseLine();
    EEPROM.write(EEPROM_ADDR, highByte(baseline));
    EEPROM.write(EEPROM_ADDR + 1, lowByte(baseline));

    uint16_t savedBaseline = readEEPROM();

    Serial.println(savedBaseline, HEX);

    if (baseline == savedBaseline) {
      readout = String("Saved!");
    }
    else {
      readout = String("Saving to EEPROM failed!");
    }
  }
  else
  {
    readout = String("Failed!");
  }

  updateDisplay();
  delay(GENERAL_DELAY);
}

void onPress()
{
  if (mode == Calibrate)
  {
    saveBaselineToEEPROM();
    free(onSecondTickCallbacks);
    onSecondTickCallbacks = nullptr;
    setMode(static_cast<ModeEnum>(0));
  }
  else
  {
    incrementMode();
    lastMeasurement = millis() - MEASUREMENT_INTERVAL;
  }
}

void onLongPress()
{
  if (mode == Calibrate)
  {
    setMode(static_cast<ModeEnum>(0));
    readout = String("Canceled!");
    updateDisplay();
    free(onSecondTickCallbacks);
    onSecondTickCallbacks = nullptr;
    delay(GENERAL_DELAY);
    return;
  }

  minute = 0;
  second = 0;
  Serial.println("Calibrating baseline");

  onSecondTickCallbacks = (onSecondTick*)malloc(sizeof(onSecondTick) * 2);
  onSecondTickCallbacks[0] = displayBaselineCalibrationAndTime;
  onSecondTickCallbacks[1] = []() {
    if (minute == MAX_TIME_FOR_CALIBRATION) {
      onPress();
    }
  };
  
  setMode(Calibrate);
}

void incrementMode()
{
  int modeNumber = mode;
  modeNumber++;
  ModeEnum nextMode = static_cast<ModeEnum>(modeNumber);

  Serial.print("Next Mode: ");
  Serial.println(modeNumber);

  if (nextMode == Calibrate)
  {
    setMode(static_cast<ModeEnum>(0));
  }
  else
  {
    setMode(nextMode);
  }
}

template <typename value>
String formatSensorReading(const char *heading, value v, String unit)
{
  String readout;
  readout += heading;
  readout += ": \r\n";
  readout += String(v);
  readout += unit;

  return readout;
}

void writeText(String v)
{
  display.clearDisplay();
  display.setCursor(X_CUR, Y_CUR);
  display.print(v);
  display.display();
}

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

void setMode(ModeEnum modeEnum)
{
  lastMode = mode;
  mode = modeEnum;
}

void updateWaiting()
{
  switch (strlen(waiting))
  {
  case 0:
    waiting = ".";
    break;
  case 1:
    waiting = "..";
    break;
  case 2:
    waiting = "...";
    break;
  case 3:
    waiting = "";
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