/* ESP32 Weather Display using an WS-EPD 7.5" 800x480 Display, obtains data from Open Weather Map, decodes and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2025. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk
*/
#include "owm_credentials.h"  // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>      // https://github.com/bblanchon/ArduinoJson needs version v6 or above
#include <WiFi.h>             // Built-in
#include "time.h"             // Built-in
#include <SPI.h>              // Built-in
#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "imagedata.h"
#include <stdlib.h>

#define max_readings 48

#include "forecast_record.h"
#include "lang.h"  // Localisation (English)

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 480

//Create a new image cache
UBYTE *BlackImage;
UDOUBLE Imagesize = ((EPD_7IN5_V2_WIDTH % 8 == 0) ? (EPD_7IN5_V2_WIDTH / 8) : (EPD_7IN5_V2_WIDTH / 8 + 1)) * EPD_7IN5_V2_HEIGHT;

//################  VERSION  #####################################################
String version = "1.0 (04/10/25)";  // Programme version
// NOTE WITH MODIFIED FONTS for DEGREE SYMBOL replacing the char '
//################ VARIABLES #####################################################

boolean LargeIcon = true, SmallIcon = false;
#define Large 17                          // For icon drawing, needs to be odd number for best effect
#define Small 6                           // For icon drawing, needs to be odd number for best effect
String Time_str, Date_str, Forecast_day;  // strings to hold time and received weather data
int wifi_signal, CurrentHour = 0, CurrentMin = 0, CurrentSec = 0;
long StartTime = 0;
int bootCount;

//################ PROGRAM VARIABLES and OBJECTS ################

Forecast_record_type WxConditions[1];
Forecast_record_type WxForecast[max_readings];
Forecast_record_type Daily[8];  // 7-days maximum from the API

#include "common.h"

#define autoscale_on true
#define autoscale_off false
#define barchart_on true
#define barchart_off false

float pressure_readings[max_readings] = { 0 };
float temperature_readings[max_readings] = { 0 };
float humidity_readings[max_readings] = { 0 };
float rain_readings[max_readings] = { 0 };
float snow_readings[max_readings] = { 0 };

long SleepDuration = 60;  // Sleep time in minutes, aligned to the nearest minute boundary, so if 30 will always update at 00 or 30 past the hour
int WakeupTime = 7;       // Don't wakeup until after 07:00 to save battery power
int SleepTime = 23;       // Sleep after (23+1) 00:00 to save battery power

//#########################################################################################
void setup() {
  StartTime = millis();
  Serial.begin(115200);
  delay(200);
  Serial.println(__FILE__);
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
    Serial.println("Failed to apply for black memory...\r\n");
    while (1)
      ;
  }
  if (StartWiFi() == WL_CONNECTED && SetupTime() == true) {
    InitialiseDisplay();  // Give screen time to initialise by getting weather data!
    Serial.println("started screen...");
    byte Attempts = 1;
    bool RxWeather = false, RxForecast = false;
    WiFiClient client;                             // wifi client object
    while (RxWeather == false && Attempts <= 2) {  // Try up-to 2 time for Weather and Forecast data
      Serial.println("Getting Weather Data");
      RxWeather = obtain_wx_data_onecall(client, true);  // true= print the data, false won't
    }
    if (RxWeather) {  // Only if received Weather
      Serial.println("Obtained Weather Data");
      StopWiFi();  // Reduces power consumption
      DisplayWeather();
    }
  }
  Paint_NewImage(BlackImage, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  DEV_Delay_ms(3000);
  Serial.println("Done...");
  BeginSleep();
}
//#########################################################################################
void loop() {  // this will never run!
}

void BeginSleep() {  // Wake up with a Touch pin to refresh the weather data, just needs a wire on the chosen pin
  //powerOff();
  long SleepTimer = (SleepDuration * 60 - ((CurrentMin % SleepDuration) * 60 + CurrentSec) + 60);  //Some ESP32 are too fast to maintain accurate time
  EPD_7IN5_V2_Sleep();
  DEV_Delay_ms(2000);                                     //important, at least 2s
  esp_sleep_enable_timer_wakeup((SleepTimer)*1000000LL);  // Added +20 seconds to cover ESP32 RTC timer source inaccuracies
#ifdef BUILTIN_LED
  pinMode(BUILTIN_LED, INPUT);  // If it's On, turn it off and some boards use GPIO-5 for SPI-SS, which remains low after screen use
  digitalWrite(BUILTIN_LED, HIGH);
#endif
  Serial.println("Entering " + String(SleepTimer) + "-secs of sleep time");
  Serial.println("Awake for : " + String((millis() - StartTime) / 1000.0, 3) + "-secs");
  Serial.println("Starting deep-sleep period...");
  esp_deep_sleep_start();  // Sleep for e.g. 60 minutes
}
//#########################################################################################
void DisplayWeather() {         // 7.5" e-paper display is 800x480 resolution
  DisplayGeneralInfoSection();  // Top line of the display
  DisplayWindSection(108, 146, WxConditions[0].Winddir, WxConditions[0].Windspeed, 81);
  DisplayMainWeatherSection(300, 100);          // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  DisplayForecastSection(217, 245);             // 1-day forecast boxes
  DisplayAstronomySection(0, 245);              // Astronomy section Sun rise/set, Moon phase and Moon icon
  DisplayStatusSection(690, 215, wifi_signal);  // Wi-Fi signal strength and Battery voltage
}
//#########################################################################################
void DisplayGeneralInfoSection() {
  drawString16(6, 2, "[Version: " + version + "] ");  // Programme version
  drawString16(SCREEN_WIDTH / 2, 3, City + "       (" + String(bootCount) + ")");
  drawRect(391, 183, 257, 64);  // Box around time and date
  drawString16(420, 195, Date_str);
  drawString16(415, 225, Time_str);
  drawLineB(0, 18, SCREEN_WIDTH - 8, 18);
}
//#########################################################################################
void DisplayMainWeatherSection(int x, int y) {
  drawLineB(0, 38, SCREEN_WIDTH - 8, 38);
  DisplayConditionsSection(x + 3, y + 49, WxConditions[0].Icon, LargeIcon);
  DisplayTemperatureSection(x + 154, y - 81, 137, 100);
  DisplayPressureSection(x + 281, y - 81, WxConditions[0].Pressure, WxConditions[0].Trend, 137, 100);
  DisplayPrecipitationSection(x + 411, y - 81, 130, 100);
  drawRect(x + 91, y + 20, 401, 62);
  Serial.println(Daily[0].Description);
  // Test line = Daily[0].Description = "The day will start with clear sky through the late morning hours, transitioning to partly cloudy";
  // 123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.1
  // The day will start with clear sky through the late morning hours, transitioning to partly cloudy
  // The day will start with partly cloudy through the late morning hours, transitioning to rain
  // The day will start with partly cloudy through the late morning hours, transitioning to clearing
  // You can expect clear sky in the morning, with partly cloudy in the afternoon
  // You can expect partly cloudy in the morning, with clearing in the afternoon
  // Expect a day of partly cloudy with rain
  // Expect a day that is partly cloudy with rain
  // There will be clear sky today
  // There will be partly cloudy today
  // There will be partly cloudy until morning, then rain
  // There will be clear sky until morning, then partly cloudy
  // There will be partly cloudy until morning, then clearing
  // There will be rain until morning, then partly cloudy
  // Expect a day of partly cloudy with clear spells
  Daily[0].Description.replace("There", "It");
  Daily[0].Description.replace("of", "that is");
  Daily[0].Description.replace("with partly", "partly");
  Daily[0].Description.replace("with clearing", "then clearing");
  String Line1, Line2, Line3;
  WordWrap(Daily[0].Description, Line1, Line2, Line3, 35);
  if (Line2.length() == 0) drawString16(x + 95, y + 45, Line1);
  else {
    drawString16(x + 95, y + 27, Line1);
    drawString16(x + 95, y + 45, Line2);
    drawString16(x + 95, y + 63, Line3);
  }
}
//#########################################################################################
void WordWrap(String line, String &line1, String &line2, String &line3, int length) {
  Serial.println(line1 + " " + line2);
  String words[100];
  int ptr = 0;
  int ArrayLen = line.length() + 1;  //The +1 is for the 0x00h Terminator
  char str[ArrayLen];
  line.toCharArray(str, ArrayLen);
  char *pch;                // a pointer variable
  pch = strtok(str, " -");  // Find words seperated by SPACE
  while (pch != NULL) {
    String word = String(pch);
    words[ptr] = word;
    ptr++;
    pch = strtok(NULL, " -");
  }
  int numWords = ptr;  // - 1;
  ptr = 0;
  line1 = "";
  line2 = "";
  line3 = "";
  line1 = formatline(line1, length, numWords, ptr, words);
  line2 = formatline(line2, length, numWords, ptr, words);
  line3 = formatline(line3, length, numWords, ptr, words);
}
//#########################################################################################
String formatline(String &line, int &length, int numWords, int &ptr, String words[]) {
  while (line.length() < length && ptr <= numWords) {
    if (line.length() + words[ptr].length() < length) {
      line += words[ptr] + " ";
    } else break;
    ptr++;
  }
  return line;
}
//#########################################################################################
void DisplayWindSection(int x, int y, float angle, float windspeed, int Cradius) {
  drawString16(x - 90, y - Cradius - 41, TXT_WIND_SPEED_DIRECTION);
  drawRect(x - 108, y - 128, 216, 229);
  y -= 5;
  arrow(x, y, Cradius - 20, angle, 18, 32);  // Show wind direction on outer circle of width and length
  int dxo, dyo, dxi, dyi;
  drawLineB(0, 18, 0, y + Cradius + 37);
  drawCircleB(x, y, Cradius);        // Draw compass circle
  drawCircleB(x, y, Cradius + 1);    // Draw compass circle
  drawCircleB(x, y, Cradius * 0.7);  // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45) drawString16(dxo + x + 5, dyo + y - 12, TXT_NE);
    if (a == 135) drawString16(dxo + x, dyo + y + 6, TXT_SE);
    if (a == 225) drawString16(dxo + x - 25, dyo + y, TXT_SW);
    if (a == 315) drawString16(dxo + x - 25, dyo + y - 12, TXT_NW);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLineB(dxo + x, dyo + y, dxi + x, dyi + y);
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    drawLineB(dxo + x, dyo + y, dxi + x, dyi + y);
  }
  drawString16(x - 5, y - Cradius - 15, TXT_N);
  drawString16(x - 5, y + Cradius + 2, TXT_S);
  drawString16(x - Cradius - 15, y - 8, TXT_W);
  drawString16(x + Cradius + 5, y - 8, TXT_E);
  drawString16(x - 10, y - 33, WindDegToDirection(angle));
  drawString16(x - 18, y - 3, String(windspeed, 1) + "m/s");
  drawString16(x - 18, y + 30, String(angle, 0) + "'");
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = { TXT_N, TXT_NNE, TXT_NE, TXT_ENE, TXT_E, TXT_ESE, TXT_SE, TXT_SSE, TXT_S, TXT_SSW, TXT_SW, TXT_WSW, TXT_W, TXT_WNW, TXT_NW, TXT_NNW };
  return Ord_direction[(dir % 16)];
}
//#########################################################################################
void DisplayTemperatureSection(int x, int y, int twidth, int tdepth) {
  drawRect(x - 63, y - 1, twidth, tdepth);  // temp outline
  drawString16(x - 50, y + 5, TXT_TEMPERATURE);
  drawString16(x - 35, y + 82, String(Daily[0].High, 0) + "' | " + String(Daily[0].Low, 0) + "'");  // Show forecast high and Low
  drawString24(x - 35, y + 40, String(WxConditions[0].Temperature, 1) + "'C");                      // Show current Temperature
}
//#########################################################################################
void DisplayForecastWeather(int x, int y) {
  for (int Forecast = 0; Forecast < 8; Forecast++) {
    DisplayForecastDailyWeather(x, y, Forecast);
  }
}
//#########################################################################################
void DisplayForecastDailyWeather(int x, int y, int Forecast) {
  int Fwidth = 72, FDepth = 85, offset = 20;
  x = x + Fwidth * Forecast;
  drawRect(x, y + 50, Fwidth - 1, FDepth);
  DisplayConditionsSection(x + Fwidth / 2 - 5, y + 95, Daily[Forecast].Icon, SmallIcon);
  ConvertUnixTimeToDay(Daily[Forecast].Dt);
  if (Forecast == 0) {
    Forecast_day = TXT_TODAY;
    offset = 27;
  }
  drawString16(x + Fwidth / 2 - offset, y + 55, Forecast_day);
  drawString12(x + Fwidth / 2 - 23, y + 120, String(Daily[Forecast].High, 0) + "'/" + String(Daily[Forecast].Low, 0) + "'");
}
//#########################################################################################
void DisplayPressureSection(int x, int y, float pressure, String slope, int pwidth, int pdepth) {
  drawRect(x - 56, y - 1, pwidth, pdepth);  // pressure outline
  drawString16(x - 25, y + 5, TXT_PRESSURE);
  String slope_direction = TXT_PRESSURE_STEADY;
  if (slope == "+") slope_direction = TXT_PRESSURE_RISING;
  if (slope == "-") slope_direction = TXT_PRESSURE_FALLING;
  drawRect(x + 40, y + 78, 41, 21);
  drawString24(x - 22, y + 40, String(pressure, 0));  // "Metric"
  drawString12(x + 52, y + 83, (Units == "M" ? "hPa" : "in"));
  drawString16(x - 35, y + 83, slope_direction);
}
//#########################################################################################
void DisplayPrecipitationSection(int x, int y, int pwidth, int pdepth) {
  drawRect(x - 48, y - 1, pwidth, pdepth);  // precipitation outline
  drawString16(x - 30, y + 5, TXT_PRECIPITATION);
  if (WxForecast[1].Rainfall >= 0.001) {                                                             // Ignore small amounts
    drawString16(x - 30, y + 35, String(WxForecast[1].Rainfall, 2) + (Units == "M" ? "mm" : "in"));  // Only display rainfall total today if > 0
    addraindrop(x + 55, y + 40, 5);
  }
  if (WxForecast[1].Snowfall >= 0.001)                                                                       // Ignore small amounts
    drawString16(x - 30, y + 60, String(WxForecast[1].Snowfall, 2) + (Units == "M" ? "mm" : "in") + " **");  // Only display snowfall total today if > 0
  if (Daily[0].PoP >= 0.0)                                                                                   // Ignore small amounts
    drawString16(x - 25, y + 80, String(Daily[0].PoP * 100, 0) + "% PoP");                                   // Only display pop if > 0
}
//#########################################################################################
void DisplayAstronomySection(int x, int y) {
  drawRect(x, y + 5, 216, 85);
  drawString16(x + 6, y + 24, ConvertUnixTime(WxConditions[0].Sunrise + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNRISE);
  drawString16(x + 6, y + 45, ConvertUnixTime(WxConditions[0].Sunset + WxConditions[0].Timezone).substring(0, 5) + " " + TXT_SUNSET);
  time_t now = time(NULL);
  struct tm *now_utc = gmtime(&now);
  const int day_utc = now_utc->tm_mday;
  const int month_utc = now_utc->tm_mon + 1;
  const int year_utc = now_utc->tm_year + 1900;
  drawString16(x + 6, y + 70, MoonPhase(day_utc, month_utc, year_utc, Hemisphere));
  DrawMoon(x + 137, y, day_utc, month_utc, year_utc, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  const int diameter = 47;
  double Phase = NormalizedMoonPhase(dd, mm, yy);
  hemisphere.toLowerCase();
  if (hemisphere == "south") Phase = 1 - Phase;
  // Draw dark part of moon
  fillCircleB(x + diameter - 1, y + diameter, diameter / 2 + 1);
  const int number_of_lines = 90;
  for (double Ypos = 0; Ypos <= number_of_lines / 2; Ypos++) {
    double Xpos = sqrt(number_of_lines / 2 * number_of_lines / 2 - Ypos * Ypos);
    // Determine the edges of the lighted part of the moon
    double Rpos = 2 * Xpos;
    double Xpos1, Xpos2;
    if (Phase < 0.5) {
      Xpos1 = -Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    } else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW1y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW2x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW2y = (number_of_lines - Ypos) / number_of_lines * diameter + y;
    double pW3x = (Xpos1 + number_of_lines) / number_of_lines * diameter + x;
    double pW3y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    double pW4x = (Xpos2 + number_of_lines) / number_of_lines * diameter + x;
    double pW4y = (Ypos + number_of_lines) / number_of_lines * diameter + y;
    drawLineW(pW1x, pW1y, pW2x, pW2y);
    drawLineW(pW3x, pW3y, pW4x, pW4y);
  }
  drawCircleB(x + diameter - 1, y + diameter, diameter / 2);
}
//#########################################################################################
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c = 365.25 * y;
  e = 30.6 * m;
  jd = c + e + d - 694039.09; /* jd is total days elapsed */
  jd /= 29.53059;             /* divide by the moon cycle (29.53 days) */
  b = jd;                     /* int(jd) -> b, take integer part of jd */
  jd -= b;                    /* subtract integer part to leave fractional part of original jd */
  b = jd * 8 + 0.5;           /* scale fraction from 0-8 and round by adding 0.5 */
  b = b & 7;                  /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return TXT_MOON_NEW;              // New;              0%  illuminated
  if (b == 1) return TXT_MOON_WAXING_CRESCENT;  // Waxing crescent; 25%  illuminated
  if (b == 2) return TXT_MOON_FIRST_QUARTER;    // First quarter;   50%  illuminated
  if (b == 3) return TXT_MOON_WAXING_GIBBOUS;   // Waxing gibbous;  75%  illuminated
  if (b == 4) return TXT_MOON_FULL;             // Full;            100% illuminated
  if (b == 5) return TXT_MOON_WANING_GIBBOUS;   // Waning gibbous;  75%  illuminated
  if (b == 6) return TXT_MOON_THIRD_QUARTER;    // Third quarter;   50%  illuminated
  if (b == 7) return TXT_MOON_WANING_CRESCENT;  // Waning crescent; 25%  illuminated
  return "";
}
//#########################################################################################
void DisplayForecastSection(int x, int y) {
  DisplayForecastWeather(x, y - 45);
  // Pre-load temporary arrays with with data - because C parses by reference
  int r = 0;
  do {
    if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;
    else pressure_readings[r] = WxForecast[r].Pressure;
    if (Units == "I") rain_readings[r] = WxForecast[r].Rainfall * 0.0393701;
    else rain_readings[r] = WxForecast[r].Rainfall;
    if (Units == "I") snow_readings[r] = WxForecast[r].Snowfall * 0.0393701;
    else snow_readings[r] = WxForecast[r].Snowfall;
    temperature_readings[r] = WxForecast[r].Temperature;
    humidity_readings[r] = WxForecast[r].Humidity;
    r++;
  } while (r < max_readings);
  int gwidth = 150, gheight = 72;
  int gx = (SCREEN_WIDTH - gwidth * 4) / 5 + 5;
  int gy = 380;
  int gap = gwidth + gx;
  drawString16(SCREEN_WIDTH / 2 - 100, gy - 40, TXT_FORECAST_VALUES);  // Based on a graph height of 60
    // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  DrawGraph(gx + 0 * gap, gy, gwidth, gheight, 900, 1050, Units == "M" ? TXT_PRESSURE_HPA : TXT_PRESSURE_IN, pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 1 * gap, gy, gwidth, gheight, 10, 30, Units == "M" ? TXT_TEMPERATURE_C : TXT_TEMPERATURE_F, temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx + 2 * gap, gy, gwidth, gheight, 0, 100, TXT_HUMIDITY_PERCENT, humidity_readings, max_readings, autoscale_off, barchart_off);
  const int Rain_array_size = sizeof(rain_readings) / sizeof(float);
  const int Snow_array_size = sizeof(snow_readings) / sizeof(float);
  if (SumOfPrecip(rain_readings, Rain_array_size) >= SumOfPrecip(snow_readings, Snow_array_size))
    DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_RAINFALL_MM : TXT_RAINFALL_IN, rain_readings, Rain_array_size, autoscale_on, barchart_on);
  else DrawGraph(gx + 3 * gap + 5, gy, gwidth, gheight, 0, 30, Units == "M" ? TXT_SNOWFALL_MM : TXT_SNOWFALL_IN, snow_readings, Snow_array_size, autoscale_on, barchart_on);
}
//#########################################################################################
void DisplayConditionsSection(int x, int y, String IconName, bool IconSize) {
  Serial.println("Icon name: " + IconName);
  if (IconName == "01d" || IconName == "01n") Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n") MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n") Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n") MostlyCloudy(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n") ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n") Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n") Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n") Snow(x, y, IconSize, IconName);
  else if (IconName == "50d") Haze(x, y, IconSize, IconName);
  else if (IconName == "50n") Fog(x, y, IconSize, IconName);
  else Nodata(x, y, IconSize, IconName);
  if (IconSize == LargeIcon) {
    drawRect(x - 86, y - 131, 173, 229);
    drawString16(x - 50, y - 125, TXT_CONDITIONS);
    DisplayVisiCCoverUVISection(x - 5, y - 5);
    drawString16(x - 30, y + 80, String(WxConditions[0].Humidity, 0) + "% RH");
  }
}
//#########################################################################################
void DisplayVisiCCoverUVISection(int x, int y) {
  Visibility(x - 62, y - 90, String(WxConditions[0].Visibility) + "M");
  if (WxConditions[0].Cloudcover > 0) CloudCover(x + 35, y - 87, WxConditions[0].Cloudcover);
  if (WxConditions[0].UVI >= 0) Display_UVIndexLevel(x + 10, y + 52, WxConditions[0].UVI);
}
//#########################################################################################
void Display_UVIndexLevel(int x, int y, float UVI) {
  String Level = "";
  if (UVI < 2) Level = " (L)";
  if (UVI >= 2 && UVI < 5) Level = " (M)";
  if (UVI >= 5 && UVI < 7) Level = " (H)";
  if (UVI >= 7 && UVI < 10) Level = " (VH)";
  if (UVI >= 10) Level = " (EX)";
  drawString16(x - 55, y + 10, "UVI: " + String(UVI, (UVI < 1 ? 1 : 0)) + Level);
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;  // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;  // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  fillTriangleB(xx1, yy1, xx3, yy3, xx2, yy2);
}
//#########################################################################################
uint8_t StartWiFi() {
  Serial.print("\r\nConnecting to: ");
  Serial.println(String(ssid));
  IPAddress dns(8, 8, 8, 8);  // Google DNS
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);  // switch off AP
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  int status = WiFi.waitForConnectResult(10000);  // Wait for 5 seconds
  if (status == WL_CONNECTED) {
    wifi_signal = WiFi.RSSI();  // Get Wifi Signal strength now, because the WiFi will be turned off to save power!
    Serial.println("WiFi connected at: " + WiFi.localIP().toString());
  } else Serial.println("WiFi connection *** FAILED ***");
  return status;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
}
//#########################################################################################
void DisplayStatusSection(int x, int y, int rssi) {
  drawRect(x - 41, y - 32, 143, 65);
  drawRect(x - 41, y - 32, 143 / 2, 17);
  drawRect(x - 41 + (143 / 2) - 1, y - 32, 143 / 2 + 2, 17);
  drawString12(x - 20, y - 28, TXT_WIFI);
  drawString12(x + 50, y - 28, TXT_POWER);
  DrawRSSI(x - 15, y + 6, rssi);
  DrawBattery(x + 58, y + 6);
}
//#########################################################################################
void DrawRSSI(int x, int y, int rssi) {
  int WIFIsignal = 0;
  int xpos = 1;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20) WIFIsignal = 20;  //            <-20dbm displays 5-bars
    if (_rssi <= -40) WIFIsignal = 16;  //  -40dbm to  -21dbm displays 4-bars
    if (_rssi <= -60) WIFIsignal = 12;  //  -60dbm to  -41dbm displays 3-bars
    if (_rssi <= -80) WIFIsignal = 8;   //  -80dbm to  -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 4;  // -100dbm to  -81dbm displays 1-bar
    fillRectB(x + xpos * 6, y - WIFIsignal, 5, WIFIsignal);
    xpos++;
  }
  fillRectB(x, y - 1, 5, 1);
  drawString12(x - 15, y + 6, String(rssi) + "dBm");
}
//#########################################################################################
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(100);
  Serial.println("starting Time...");
  bool TimeStatus = UpdateLocalTime();
  Serial.println("started Time...");
  return TimeStatus;
}
//#########################################################################################
boolean UpdateLocalTime() {
  struct tm timeinfo;
  char time_output[40], day_output[30], update_time[30], forecast_day[30];
  while (!getLocalTime(&timeinfo, 15000)) {  // Wait for 15-sec for time to synchronise
    Serial.println("Failed to obtain time");
    return false;
  }
  CurrentHour = timeinfo.tm_hour;
  CurrentMin = timeinfo.tm_min;
  CurrentSec = timeinfo.tm_sec;
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");  // Displays: Saturday, June 24 2017 14:05:49
  if (Units == "M") {
    if ((Language == "CZ") || (Language == "DE") || (Language == "PL") || (Language == "NL")) {
      sprintf(day_output, "%s, %02u. %s %04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);  // day_output >> So., 23. Juni 2019 <<
    } else {
      sprintf(day_output, "%s %02u-%s-%04u", weekday_D[timeinfo.tm_wday], timeinfo.tm_mday, month_M[timeinfo.tm_mon], (timeinfo.tm_year) + 1900);
    }
    strftime(update_time, sizeof(update_time), "%H:%M:%S", &timeinfo);  // Creates: '14:05:49'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  } else {
    strftime(day_output, sizeof(day_output), "%a %b-%d-%Y", &timeinfo);  // Creates  'Sat May-31-2019'
    strftime(update_time, sizeof(update_time), "%r", &timeinfo);         // Creates: '02:05:49pm'
    sprintf(time_output, "%s %s", TXT_UPDATED, update_time);
  }
  strftime(forecast_day, sizeof(forecast_day), "%w", &timeinfo);
  Forecast_day = forecast_day;
  Date_str = day_output;
  Time_str = time_output;
  return true;
}
//#########################################################################################
String ConvertUnixTimeToDay(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char FDay[40];
  strftime(FDay, sizeof(FDay), "%w", now_tm);
  Forecast_day = weekday_D[String(FDay).toInt()];
  return Forecast_day;
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(34) / 4096.0 * 7.46;
  if (voltage > 1) {  // Only display if there is a valid reading
    Serial.println("Voltage = " + String(voltage));
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.50) percentage = 0;
    int BatteryWidth = 19;
    int BatteryHeight = 10;
    int BatteryBarLen = BatteryWidth - 4;
    int BatteryBarHeight = BatteryHeight - 4;
    drawRect(x + 15, y - 12, BatteryWidth, BatteryHeight);  // Body
    fillRectB(x + 34, y - 10, 2, 5);                        // Connector button
    fillRectB(x + 17, y - 10, BatteryBarLen * percentage / 100.0, BatteryBarHeight);
    drawString12(x + 10, y - 11, String(percentage) + "%");
    drawString12(x + 13, y + 5, String(voltage, 2) + "v");
  }
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  fillCircleB(x - scale * 3, y, scale);                               // Left most circle
  fillCircleB(x + scale * 3, y, scale);                               // Right most circle
  fillCircleB(x - scale, y - scale, scale * 1.4);                     // left middle upper circle
  fillCircleB(x + scale * 1.5, y - scale * 1.3, scale * 1.75);        // Right middle upper circle
  fillRectB(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1);  // Upper and lower lines
  //Clear cloud inner
  fillCircleW(x - scale * 3, y, scale - linesize);                                                    // Clear left most circle
  fillCircleW(x + scale * 3, y, scale - linesize);                                                    // Clear right most circle
  fillCircleW(x - scale, y - scale, scale * 1.4 - linesize);                                          // left middle upper circle
  fillCircleW(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize);                             // Right middle upper circle
  fillRectW(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2);  // Upper and lower lines
}
//#########################################################################################
void addraindrop(int x, int y, int scale) {
  fillCircleB(x, y, scale / 2);
  fillTriangleB(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y);
  x = x + scale * 1.6;
  y = y + scale / 3;
  fillCircleB(x, y, scale / 2);
  fillTriangleB(x - scale / 2, y, x, y - scale * 1.2, x + scale / 2, y);
}
//#########################################################################################
void addrain(int x, int y, int scale, bool IconSize) {
  if (IconSize == SmallIcon) scale *= 1.34;
  for (int d = 0; d < 3; d++) {
    addraindrop(x + scale * (6.8 - d * 1.95) - scale * 5.2, y + scale * 2.1 - scale / 6, scale / 1.6);
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale, bool IconSize) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180);
      dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180);
      dyi = dyo * 0.1;
      drawLineB(dxo + x + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    drawLineB(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale);
    if (scale != Small) {
      drawLineB(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale);
      drawLineB(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale);
    }
    drawLineB(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0);
    if (scale != Small) {
      drawLineB(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1);
      drawLineB(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2);
    }
    drawLineB(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5);
    if (scale != Small) {
      drawLineB(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5);
      drawLineB(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale, bool IconSize) {
  int linesize = 3;
  if (IconSize == SmallIcon) linesize = 1;
  fillRectB(x - scale * 2, y, scale * 4, linesize);
  fillRectB(x, y - scale * 2, linesize, scale * 4);
  drawLineB(x - scale * 1.3, y - scale * 1.3, x + scale * 1.3, y + scale * 1.3);
  drawLineB(x - scale * 1.3, y + scale * 1.3, x + scale * 1.3, y - scale * 1.3);
  if (IconSize == LargeIcon) {
    drawLineB(1 + x - scale * 1.3, y - scale * 1.3, 1 + x + scale * 1.3, y + scale * 1.3);
    drawLineB(2 + x - scale * 1.3, y - scale * 1.3, 2 + x + scale * 1.3, y + scale * 1.3);
    drawLineB(3 + x - scale * 1.3, y - scale * 1.3, 3 + x + scale * 1.3, y + scale * 1.3);
    drawLineB(1 + x - scale * 1.3, y + scale * 1.3, 1 + x + scale * 1.3, y - scale * 1.3);
    drawLineB(2 + x - scale * 1.3, y + scale * 1.3, 2 + x + scale * 1.3, y - scale * 1.3);
    drawLineB(3 + x - scale * 1.3, y + scale * 1.3, 3 + x + scale * 1.3, y - scale * 1.3);
  }
  fillCircleW(x, y, scale * 1.3);
  fillCircleB(x, y, scale);
  fillCircleW(x, y, scale - linesize);
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize, bool IconSize) {
  if (IconSize == SmallIcon) {
    y -= 10;
    linesize = 1;
  }
  for (int i = 0; i < 6; i++) {
    fillRectB(x - scale * 3, y + scale * 1.5, scale * 6, linesize);
    fillRectB(x - scale * 3, y + scale * 2.0, scale * 6, linesize);
    fillRectB(x - scale * 3, y + scale * 2.5, scale * 6, linesize);
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  else y = y - 3;  // Shift up small sun icon
  if (IconName.endsWith("n")) addmoon(x, y + 3, scale, IconSize);
  scale = scale * 1.6;
  addsun(x, y, scale, IconSize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small, linesize = 3, offset = 5;
  if (IconSize == LargeIcon) {
    scale = Large;
    offset = 10;
  }
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x, y + offset, scale, IconSize);
  addcloud(x, y + offset, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8 + offset, scale, IconSize);
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  } else {
    y += 10;
    if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
    addcloud(x + 30, y - 45, 5, linesize);  // Cloud top right
    addcloud(x - 20, y - 30, 7, linesize);  // Cloud top left
    addcloud(x, y, scale, linesize);        // Main cloud
  }
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale, IconSize);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Large, linesize = 3;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale, IconSize);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addcloud(x, y - 5, scale, linesize);
  addfog(x, y - 5, scale, linesize, IconSize);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int linesize = 3, scale = Large;
  if (IconSize == SmallIcon) {
    scale = Small;
    linesize = 1;
  }
  if (IconName.endsWith("n")) addmoon(x, y, scale, IconSize);
  addsun(x, y - 5, scale * 1.4, IconSize);
  addfog(x, y - 5, scale * 1.4, linesize, IconSize);
}
//#########################################################################################
void CloudCover(int x, int y, int CCover) {
  addcloud(x - 9, y - 3, Small * 0.5, 2);  // Cloud top left
  addcloud(x + 3, y - 3, Small * 0.5, 2);  // Cloud top right
  addcloud(x, y, Small * 0.5, 2);          // Main cloud
  drawString16(x + 15, y - 10, String(CCover) + "%");
}
//#########################################################################################
void Visibility(int x, int y, String Visi) {
  y = y - 3;
  x = x - 3;
  float start_angle = 0.52, end_angle = 2.61;
  int r = 10;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y - r / 2 + r * sin(i));
    drawPixel(x + r * cos(i), 1 + y - r / 2 + r * sin(i));
  }
  start_angle = 3.61;
  end_angle = 5.78;
  for (float i = start_angle; i < end_angle; i = i + 0.05) {
    drawPixel(x + r * cos(i), y + r / 2 + r * sin(i));
    drawPixel(x + r * cos(i), 1 + y + r / 2 + r * sin(i));
  }
  fillCircleB(x, y, r / 4);
  drawString16(x + 8, y - 5, Visi);
}
//#########################################################################################
void addmoon(int x, int y, int scale, bool IconSize) {
  if (IconSize == LargeIcon) {
    fillCircleB(x - 62, y - 68, scale);
    fillCircleW(x - 43, y - 68, scale * 1.6);
  } else {
    fillCircleW(x - 25, y - 15, scale);
    fillCircleW(x - 18, y - 15, scale * 1.6);
  }
}
//#########################################################################################
void Nodata(int x, int y, bool IconSize, String IconName) {
  if (IconSize == LargeIcon) drawString24(x - 3, y - 10, "?");
  else drawString12(x - 3, y - 10, "?");
}
//#########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos-the x axis top-left position of the graph
    y_pos-the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width-the width of the graph in pixels
    height-height of the graph in pixels
    Y1_Max-sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale-a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on-a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour-a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
#define auto_scale_margin 0  // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5       // 5 y-axis division markers
  float maxYscale = -10000;
  float minYscale = 10000;
  int last_x, last_y;
  float x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin);  // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale + 0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin);  // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  drawRect(x_pos, y_pos, gwidth + 3, gheight + 2);
  drawString16(x_pos + 25, y_pos - 18, title);
  // Draw the data
  for (int gx = 0; gx < readings; gx++) {
    y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
    if (barchart_mode) {
      x2 = x_pos + gx * (gwidth / readings) + 2;
      fillRectB(x2, y2, (gwidth / readings) - 2, y_pos + gheight - y2 + 2);
    } else {
      x2 = x_pos + gx * gwidth / (readings - 1) + 1;  // max_readings is the global variable that sets the maximum data that can be plotted
      drawLineB(last_x, last_y, x2, y2);
    }
    last_x = x2;
    last_y = y2;
  }
  //Draw the Y-axis scale
#define number_of_dashes 20
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
    for (int j = 0; j < number_of_dashes; j++) {  // Draw dashed graph grid lines
      if (spacing < y_minor_axis) drawFastLineB((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes));
    }
    int characterWidth = 9;  // for 12-point font
    float fieldWidth = 5;
    int displayFormat = 0;
    if (Y1Max <= 0 || Y1Max > -1 && Y1Max < 10) displayFormat = 1;
    if (Y1Max >= 1000 && Y1Max < 10000) fieldWidth = 4;  // 0000
    if (Y1Max >= 100 && Y1Max < 1000) fieldWidth = 3;    // 000
    if (Y1Max >= 10 && Y1Max < 100) fieldWidth = 2.5;    // 00
    if (Y1Max >= 0 && Y1Max < 10) fieldWidth = 3;        // 0.0
    if (Y1Max < 0 || Y1Min < 0) fieldWidth += 1;         // -0.0
    drawString12(x_pos - fieldWidth * characterWidth, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), displayFormat));
  }
  int Days = 2;
  drawString12(x_pos + gwidth / (Days * 2) * 1, y_pos + gheight + 3, "1");
  drawString12(x_pos + gwidth / (Days * 2) * 3, y_pos + gheight + 3, "2");
  drawString12(x_pos + gwidth / 3, y_pos + gheight + 14, TXT_DAYS);
}
//#########################################################################################
void drawString8(int x, int y, String text) {
  int16_t x1, y1;                                                 //the bounds of x,y and w and h of the variable 'text' in pixels.
  Paint_DrawString_EN(x, y, text.c_str(), &Font8, WHITE, BLACK);  // x, y, text, foreground, background
}

//#########################################################################################
void drawString12(int x, int y, String text) {
  int16_t x1, y1;                                                  //the bounds of x,y and w and h of the variable 'text' in pixels.
  Paint_DrawString_EN(x, y, text.c_str(), &Font12, WHITE, BLACK);  // x, y, text, foreground, background
}
//#########################################################################################
void drawString16(int x, int y, String text) {
  int16_t x1, y1;                                                  //the bounds of x,y and w and h of the variable 'text' in pixels.
  Paint_DrawString_EN(x, y, text.c_str(), &Font16, WHITE, BLACK);  // x, y, text, foreground, background
}
//#########################################################################################
void drawString20(int x, int y, String text) {
  int16_t x1, y1;                                                  //the bounds of x,y and w and h of the variable 'text' in pixels.
  Paint_DrawString_EN(x, y, text.c_str(), &Font20, WHITE, BLACK);  // x, y, text, foreground, background
}
//#########################################################################################
void drawString24(int x, int y, String text) {
  int16_t x1, y1;                                                  //the bounds of x,y and w and h of the variable 'text' in pixels.
  Paint_DrawString_EN(x, y, text.c_str(), &Font24, WHITE, BLACK);  // x, y, text, foreground, background
}

//#########################################################################################
void InitialiseDisplay() {
  Serial.println("Wx Display\r\n");
  DEV_Module_Init();
  Serial.println("e-Paper Init and Clear...\r\n");
  EPD_7IN5_V2_Init();
  EPD_7IN5_V2_Clear();
  Serial.println("New Image Create\r\n");
  Paint_NewImage(BlackImage, EPD_7IN5_V2_WIDTH, EPD_7IN5_V2_HEIGHT, 0, WHITE);
  Paint_Clear(0xff);
  Serial.println("started screen...");
}

void drawRect(int x, int y, int w, int d) {
  Paint_DrawRectangle(x, y, x + w, y + d, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void drawCircleB(int x, int y, int d) {
  Paint_DrawCircle(x, y, d, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void drawCircleW(int x, int y, int d) {
  Paint_DrawCircle(x, y, d, WHITE, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void fillCircleB(int x, int y, int d) {
  Paint_DrawCircle(x, y, d, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void fillCircleW(int x, int y, int d) {
  Paint_DrawCircle(x, y, d, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void fillRectB(int x, int y, int w, int d) {
  Paint_DrawRectangle(x, y, x + w, y + d, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void fillRectW(int x, int y, int w, int d) {
  Paint_DrawRectangle(x, y, x + w, y + d, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void drawLineB(int x, int y, int x1, int y1) {
  Paint_DrawLine(x, y, x1, y1, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

void drawFastLineB(int x, int y, int x1) {
  Paint_DrawLine(x, y, x + x1, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}


void drawLineW(int x, int y, int x1, int y1) {
  Paint_DrawLine(x, y, x1, y1, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

void drawPixel(int x, int y) {
  Paint_DrawPoint(x, y, BLACK, DOT_PIXEL_1X1, DOT_STYLE_DFT);
}

void SfillTriangleB(int xx1, int yy1, int xx2, int yy2, int xx3, int yy3) {
  Paint_DrawLine(xx1, yy1, xx2, yy2, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawLine(xx2, yy2, xx3, yy3, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawLine(xx3, yy3, xx1, yy1, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

void swap(int16_t &a, int16_t &b) {
  int16_t c = a;
  a = b;
  b = c;
}

void fillTriangleB(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  int16_t a, b, y, last;
  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
    swap(y0, y1);
    swap(x0, x1);
  }
  if (y1 > y2) {
    swap(y2, y1);
    swap(x2, x1);
  }
  if (y0 > y1) {
    swap(y0, y1);
    swap(x0, x1);
  }

  if (y0 == y2) {  // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a)
      a = x1;
    else if (x1 > b)
      b = x1;
    if (x2 < a)
      a = x2;
    else if (x2 > b)
      b = x2;
    Paint_DrawLine(a, y0, a + b - a + 1, BLACK, y0, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
    return;
  }

  int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
          dx12 = x2 - x1, dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2)
    last = y1;  // Include y1 scanline
  else
    last = y1 - 1;  // Skip it

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b) swap(a, b);
    Paint_DrawLine(a, y, a + b - a + 1, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b) swap(a, b);
    Paint_DrawLine(a, y, a + b - a + 1, y, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  }
}

void fillTriangleW(int xx1, int yy1, int xx2, int yy2, int xx3, int yy3) {
  Paint_DrawLine(xx1, yy1, xx2, yy2, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawLine(xx2, yy2, xx3, yy3, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
  Paint_DrawLine(xx3, yy3, xx1, yy1, WHITE, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

//Paint_DrawLine(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
//Paint_DrawLine(70, 70, 20, 120, BLACK, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
//Paint_DrawRectangle(20, 70, 70, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
//Paint_DrawRectangle(80, 70, 130, 120, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
//Paint_DrawCircle(45, 95, 20, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
//Paint_DrawCircle(105, 95, 20, WHITE, DOT_PIXEL_1X1, DRAW_FILL_FULL);
