#ifndef MAIN
#define MAIN

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "DFRobot_CCS811.h"
#include "DFRobot_BME280.h"
#include <EEPROM.h>
#include "button.h"

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

enum DisplayMode
{
  Static,
  Scroll,
  Blink
};

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
#define X_CUR 0
#define PX_PER_CHAR 6
#define BTN_PIN 3
#define EEPROM_ADDR 0
#define MAX_TIME_FOR_CALIBRATION 20
#define MIN_TIME_FOR_CALIBRATION 20

unsigned long lastMeasurement = millis();
unsigned long baselineAge = millis() / 1000 / 60 / 60;
int displayX;
int displayMinX;
String readout;
uint16_t baseline;
ModeEnum mode;
ModeEnum lastMode;
DisplayMode displayMode;
int minute = 0;
int second = 0;
int lastSecond = millis();
char *waiting = "...";
int textSize = TEXT_SIZE;
bool baselineUpdated = false;

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
void updateStaticDisplay();
void updateScrollDisplay();
void updateBlinkDisplay();
void restoreBaseline();

#endif