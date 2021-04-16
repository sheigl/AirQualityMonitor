#include "main.h"

#ifndef MAIN_DEBUG
//#define MAIN_DEBUG
#endif

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
  display.setTextSize(textSize);
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

  // Program init
  setMode(Temperature);
}

void loop()
{
  // loop updates
  updateSensorReading();
  updateDisplay();
  modeBtn.Update();
  updateTime();
}

void restoreBaseline()
{
  /* uint16_t eepromValue = readEEPROM();
  displayMode = Static;
  display.setTextSize(1);
  readout = String("Using saved baseline: \r\n") + String(eepromValue, HEX);
  updateDisplay();
  delay(GENERAL_DELAY);
  display.setTextSize(TEXT_SIZE);

  baseline = eepromValue;
  CCS811.writeBaseLine(baseline); */
}

void updateSensorReading()
{
  unsigned long now = millis();
  int nowHours = now / 1000 / 60 / 60;

  if (mode != Calibrate && now - lastMeasurement > MEASUREMENT_INTERVAL)
  {
    readout = String();
    /* #ifdef MAIN_DEBUG
    Serial.print("displayX: ");
    Serial.println(displayX);
    Serial.print("displayMinX: ");
    Serial.println(displayMinX);
    Serial.print("displayMode: ");
    Serial.println(displayMode);
    #endif */

    lastMeasurement = now;

    if (minute >= MIN_TIME_FOR_CALIBRATION && !baselineUpdated)
    {
      if (CCS811.checkDataReady())
      {
        CCS811.getCO2PPM();
        CCS811.getTVOCPPB();
      }

      restoreBaseline();
      baselineUpdated = true;      
    }
    else if (minute < MIN_TIME_FOR_CALIBRATION && !baselineUpdated)
    {
      readout = String("Waiting ") + String(MIN_TIME_FOR_CALIBRATION - minute) + String(" minute(s) ") + String("for resistance to stabilize...");
    }
    else
    {
      if (CCS811.checkDataReady())
      {
        float tempC = bme.getTemperature();
        float hum = bme.getHumidity();

        switch (mode)
        {
        case Temperature:
          /* #ifdef MAIN_DEBUG
        Serial.print("Temp: ");
        Serial.print(tempC);
        Serial.println("C");
        #endif */
          readout = formatSensorReading("Temp", (tempC * 9 / 5) + 32, "F");
          break;
        case Pressure:
          readout = formatSensorReading("Pressure", bme.getPressure() / 100, "MB");
          break;
        case Humidity:
          readout = formatSensorReading("Humidity", hum, "%");
          break;
        case Altitude:
          readout = formatSensorReading("Altitude", bme.calAltitude(SEA_LEVEL_PRESSURE, bme.getPressure()), "M");
          break;
        case CO2:
          CCS811.setInTempHum(tempC, hum);
          readout = formatSensorReading("CO2", CCS811.getCO2PPM(), "PPM");
          break;
        case VOC:
          CCS811.setInTempHum(tempC, hum);
          readout = formatSensorReading("TVOC", CCS811.getTVOCPPB(), "PPB");
          break;
        case BaselineAge:

          if (nowHours - baselineAge > BASELINE_AGE_MAX)
          {
            readout = "Please calibrate sensor...";
          }
          else
          {
#ifdef MAIN_DEBUG
            readout = formatSensorReading("Baseline", String(CCS811.readBaseLine(), HEX), "");
#else
            readout = formatSensorReading("BAge", nowHours - baselineAge, "HR(S)");
#endif
          }

          break;
        case Calibrate:
          break;
        }

#ifdef MAIN_DEBUG
        Serial.println(readout);
#endif
      }
    }
  }
}

void updateStaticDisplay()
{
  displayX = X_CUR;
}

void updateScrollDisplay()
{
  displayMinX = -(PX_PER_CHAR * TEXT_SIZE) * readout.length();

  if (--displayX < displayMinX)
  {
    displayX = display.width();
  }
}

void updateBlinkDisplay()
{
  updateStaticDisplay();
}

void updateDisplay()
{
  writeText(readout);

  switch (displayMode)
  {
  case Static:
    updateStaticDisplay();
    break;
  case Scroll:
    updateScrollDisplay();
    break;
  case Blink:
    updateBlinkDisplay();
    break;
  }
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
  const char fmt[] = "%02d:%02d %s %s";
  char baselineChar[baselineValue.length()];
  baselineValue.toCharArray(baselineChar, baselineValue.length());

  char *formatted = (char *)malloc(sizeof(char) * (strlen(fmt) + strlen(baselineChar)));

  sprintf(formatted, fmt, minute, second, "Baseline", baselineChar);

  readout = "Calibrating";
  readout += String(waiting);
  readout += "\r\n";
  readout += String(formatted);
#ifdef MAIN_DEBUG
  Serial.println(readout);
#endif
  updateDisplay();
  display.setTextSize(1);

  free(formatted);
}

uint16_t readEEPROM()
{
#ifdef MAIN_DEBUG
  Serial.println("Getting EEPROM value...");
#endif
  uint16_t baseline = (EEPROM.read(EEPROM_ADDR)) * 256;
#ifdef MAIN_DEBUG
  Serial.print("EEPROM highByte: ");
  Serial.println(baseline, HEX);
#endif

  uint8_t low = EEPROM.read(EEPROM_ADDR + 1);
  baseline += low;

#ifdef MAIN_DEBUG
  Serial.print("EEPROM lowByte: ");
  Serial.println(low, HEX);

  Serial.print("EEPROM fullByte: ");
  Serial.println(baseline, HEX);
#endif

  return baseline;
}

void saveBaselineToEEPROM()
{
  int secondsWaited = 0;

  while (!CCS811.checkDataReady())
  {
#ifdef MAIN_DEBUG
    Serial.println("Waiting for sensor...");
#endif

    delay(1000);
    secondsWaited++;

    if (secondsWaited >= 30)
    {
      readout = String("Failed to read baseline!");
#ifdef MAIN_DEBUG
      Serial.println(readout);
#endif

      updateDisplay();
      delay(GENERAL_DELAY);
      return;
    }
  }

  if (CCS811.checkDataReady())
  {
#ifdef MAIN_DEBUG
    Serial.println(baseline, HEX);
#endif

    baseline = CCS811.readBaseLine();
    EEPROM.write(EEPROM_ADDR, highByte(baseline));
    EEPROM.write(EEPROM_ADDR + 1, lowByte(baseline));

    uint16_t savedBaseline = readEEPROM();

#ifdef MAIN_DEBUG
    Serial.println(savedBaseline, HEX);
#endif

    if (baseline == savedBaseline)
    {
      readout = String("Saved!");
    }
    else
    {
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
    display.setTextSize(TEXT_SIZE);
    minute = 0;
    second = 0;
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
    display.setTextSize(TEXT_SIZE);
    delay(GENERAL_DELAY);
    return;
  }

  minute = 0;
  second = 0;
#ifdef MAIN_DEBUG
  Serial.println("Calibrating baseline");
#endif

  onSecondTickCallbacks = (onSecondTick *)malloc(sizeof(onSecondTick) * 2);
  onSecondTickCallbacks[0] = displayBaselineCalibrationAndTime;
  onSecondTickCallbacks[1] = []() {
    if (minute == MAX_TIME_FOR_CALIBRATION)
    {
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

#ifdef MAIN_DEBUG
  Serial.print("Next Mode: ");
  Serial.println(modeNumber);
#endif

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
  readout += ": ";
  readout += String(v);
  readout += unit;

  return readout;
}

void writeText(String v)
{
  display.clearDisplay();
  display.setCursor(displayX, Y_CUR);
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
  displayX = X_CUR + SCREEN_WIDTH / 2;

  if (mode != Calibrate)
  {
    displayMode = Scroll;
  }
  else
  {
    displayMode = Static;
  }
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