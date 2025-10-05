#ifndef COMMON_H_
#define COMMON_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "forecast_record.h"
#include "common_functions.h"

//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = hPa_to_inHg(WxConditions[0].Pressure);
  WxForecast[1].Rainfall   = mm_to_inches(WxForecast[1].Rainfall);
  WxForecast[1].Snowfall   = mm_to_inches(WxForecast[1].Snowfall);
}

//#########################################################################################
// Problems with stucturing JSON decodes, see here: https://arduinojson.org/assistant/
bool DecodeWeather(WiFiClient& json, String Type) {
  Serial.print(F("\nCreating object...and "));
  // allocate the JsonDocument
  JsonDocument doc;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  Serial.println("Decoding " + Type + " data");
  if (Type == "weather") {
    // All Serial.println statements are for diagnostic purposes and not required, remove if not needed
    WxConditions[0].lon         = root["lon"].as<float>();                                        Serial.println(" Lon: "+String(WxConditions[0].lon));
    WxConditions[0].lat         = root["lat"].as<float>();                                        Serial.println(" Lat: "+String(WxConditions[0].lat));
    WxConditions[0].Main0       = root["current"]["weather"][0]["main"].as<const char*>();        Serial.println("Main: "+String(WxConditions[0].Main0));
    WxConditions[0].Forecast0   = root["current"]["weather"][0]["description"].as<const char*>(); Serial.println("For0: "+String(WxConditions[0].Forecast0));
    WxConditions[0].Forecast1   = root["current"]["weather"][1]["description"].as<const char*>(); Serial.println("For1: "+String(WxConditions[0].Forecast1));
    WxConditions[0].Forecast2   = root["current"]["weather"][2]["description"].as<const char*>(); Serial.println("For2: "+String(WxConditions[0].Forecast2));
    WxConditions[0].Icon        = root["current"]["weather"][0]["icon"].as<const char*>();        Serial.println("Icon: "+String(WxConditions[0].Icon));
    WxConditions[0].Temperature = root["current"]["temp"].as<float>();                            Serial.println("Temp: "+String(WxConditions[0].Temperature));
    WxConditions[0].Pressure    = root["current"]["pressure"].as<float>();                        Serial.println("Pres: "+String(WxConditions[0].Pressure));
    WxConditions[0].Humidity    = root["current"]["humidity"].as<float>();                        Serial.println("Humi: "+String(WxConditions[0].Humidity));
    WxConditions[0].Low         = root["daily"][0]["temp"]["min"].as<float>();                    Serial.println("TLow: "+String(WxConditions[0].Low));
    WxConditions[0].High        = root["daily"][0]["temp"]["max"].as<float>();                    Serial.println("THig: "+String(WxConditions[0].High));
    WxConditions[0].Windspeed   = root["current"]["wind_speed"].as<float>();                      Serial.println("WSpd: "+String(WxConditions[0].Windspeed));
    WxConditions[0].Winddir     = root["current"]["wind_deg"].as<float>();                        Serial.println("WDir: "+String(WxConditions[0].Winddir));
    WxConditions[0].Cloudcover  = root["current"]["clouds"].as<int>();                            Serial.println("CCov: "+String(WxConditions[0].Cloudcover)); // in % of cloud cover
    WxConditions[0].Visibility  = root["current"]["visibility"].as<int>();                        Serial.println("Visi: "+String(WxConditions[0].Visibility)); // in metres
    WxConditions[0].Rainfall    = root["hourly"][1]["rain"]["1h"].as<float>();                    Serial.println("Rain: "+String(WxConditions[0].Rainfall));
    WxConditions[0].Snowfall    = root["hourly"][1]["snow"]["1h"].as<float>();                    Serial.println("Snow: "+String(WxConditions[0].Snowfall));
    //WxConditions[0].Country     = root["sys"]["country"].as<const char*>();                     Serial.println("Ctry: "+String(WxConditions[0].Country));
    WxConditions[0].Sunrise     = root["current"]["sunrise"].as<int>();                           Serial.println("SRis: "+String(WxConditions[0].Sunrise));
    WxConditions[0].Sunset      = root["current"]["sunset"].as<int>();                            Serial.println("SSet: "+String(WxConditions[0].Sunset));
    WxConditions[0].Timezone    = root["timezone_offset"].as<int>();                              Serial.println("TZon: "+String(WxConditions[0].Timezone));
    
  }
  if (Type == "forecast") {
    //Serial.println(json);
    Serial.print(F("\nReceiving Forecast period - ")); //------------------------------------------------
    JsonArray list                    = root["hourly"];
    for (byte r = 0; r < max_readings; r++) {
      Serial.println("\nPeriod-" + String(r) + "--------------");
      WxForecast[r].Dt                = list[r]["dt"].as<int>();                                //Serial.println("DTim: "+String(WxForecast[r].Dt));
      WxForecast[r].Temperature       = list[r]["temp"].as<float>();                            //Serial.println("Temp: "+String(WxForecast[r].Temperature));
      WxForecast[r].Low               = list[r]["temp_min"].as<float>();                        //Serial.println("TLow: "+String(WxForecast[r].Low));
      WxForecast[r].High              = list[r]["temp_max"].as<float>();                        //Serial.println("THig: "+String(WxForecast[r].High));
      WxForecast[r].Pressure          = list[r]["pressure"].as<float>();                        //Serial.println("Pres: "+String(WxForecast[r].Pressure));
      WxForecast[r].Humidity          = list[r]["humidity"].as<float>();                        //Serial.println("Humi: "+String(WxForecast[r].Humidity));
      WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<const char*>();        //Serial.println("For0: "+String(WxForecast[r].Forecast0));
      WxForecast[r].Forecast1         = list[r]["weather"][1]["main"].as<const char*>();        //Serial.println("For1: "+String(WxForecast[r].Forecast1));
      WxForecast[r].Forecast2         = list[r]["weather"][2]["main"].as<const char*>();        //Serial.println("For2: "+String(WxForecast[r].Forecast2));
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<const char*>();        //Serial.println("Icon: "+String(WxForecast[r].Icon));
      WxForecast[r].Description       = list[r]["weather"][0]["description"].as<const char*>(); //Serial.println("Desc: "+String(WxForecast[r].Description));
      WxForecast[r].Cloudcover        = list[r]["clouds"].as<int>();                            //Serial.println("CCov: "+String(WxForecast[r].Cloudcover)); // in % of cloud cover
      WxForecast[r].Windspeed         = list[r]["wind_speed"].as<float>();                      //Serial.println("WSpd: "+String(WxForecast[r].Windspeed));
      WxForecast[r].Winddir           = list[r]["wind_deg"].as<float>();                        //Serial.println("WDir: "+String(WxForecast[r].Winddir));
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();                      //Serial.println("Rain: "+String(WxForecast[r].Rainfall));
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();                      //Serial.println("Snow: "+String(WxForecast[r].Snowfall));
      WxForecast[r].PoP               = list[r]["pop"].as<float>();                             //Serial.println("PoP:  "+String(WxForecast[r].PoP));
      WxForecast[r].Period            = list[r]["dt_txt"].as<const char*>();                    //Serial.println("Peri: "+String(WxForecast[r].Period));
    }
    //------------------------------------------
    float pressure_trend = WxForecast[2].Pressure - WxForecast[0].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  // Returns either '21:12  ' or ' 09:12pm' depending on Units mode
  time_t tm = unix_time;
  struct tm *now_tm = gmtime(&tm);
  char output[40];
  if (Units == "M") {
    strftime(output, sizeof(output), "%H:%M %d/%m/%y", now_tm);
  }
  else {
    strftime(output, sizeof(output), "%I:%M%P %m/%d/%y", now_tm);
  }
  return output;
}
//#########################################################################################
//WiFiClient client; // wifi client object

bool DecodeOneCallWeather(WiFiClient& json, bool print);

bool obtain_wx_data_onecall(WiFiClient& client, bool print) {
  const String units = (Units == "M" ? "metric" : "imperial");
  client.stop(); // close connection before sending a new request
  HTTPClient http;    
  // Update for API 3.0 June '24
  String uri = "/data/3.0/onecall?lat=" + LAT + "&lon=" + LON + "&appid=" + apikey + "&mode=json&units=" + units + "&lang=" + Language;
  http.begin(client, server, 80, uri);
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    if (!DecodeOneCallWeather(http.getStream(), print)) return false;
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}

bool DecodeOneCallWeather(WiFiClient& json, bool print) {
  //Serial.println(json);
  JsonDocument doc;                                        // allocate the JsonDocument
  DeserializationError error = deserializeJson(doc, json); // Deserialize the JSON document
  if (error) {                                             // Test if parsing succeeds.
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return false;
  }
  // convert it to a JsonObject
  JsonObject root = doc.as<JsonObject>();
  const char* TempString;
  Serial.println("Decoding data...");
  WxConditions[0].Timezone    = doc["timezone_offset"];      if (print) Serial.println("TZon: " + String(WxConditions[0].Timezone));
  JsonObject current = doc["current"];
  JsonObject weather = current["weather"][0];
  TempString = weather["Description"];
  WxConditions[0].Description = TempString;                   if (print) Serial.println("Fore: " + String(WxConditions[0].Description));
  TempString = weather["icon"];
  WxConditions[0].Icon        = TempString;                   if (print) Serial.println("Icon: " + String(WxConditions[0].Icon));
  WxConditions[0].Sunrise     = current["sunrise"];           if (print) Serial.println("SRis: " + String(WxConditions[0].Sunrise));
  WxConditions[0].Sunset      = current["sunset"];            if (print) Serial.println("SSet: " + String(WxConditions[0].Sunset));
  WxConditions[0].Temperature = current["temp"];              if (print) Serial.println("Temp: " + String(WxConditions[0].Temperature));
  WxConditions[0].FeelsLike   = current["feels_like"];        if (print) Serial.println("FLik: " + String(WxConditions[0].FeelsLike));
  WxConditions[0].Pressure    = current["pressure"];          if (print) Serial.println("Pres: " + String(WxConditions[0].Pressure));
  WxConditions[0].Humidity    = current["humidity"];          if (print) Serial.println("Humi: " + String(WxConditions[0].Humidity));
  WxConditions[0].DewPoint    = current["dew_point"];         if (print) Serial.println("DewP: " + String(WxConditions[0].DewPoint));
  WxConditions[0].UVI         = current["uvi"];               if (print) Serial.println("UVin: " + String(WxConditions[0].UVI));
  WxConditions[0].Cloudcover  = current["clouds"];            if (print) Serial.println("CCov: " + String(WxConditions[0].Cloudcover));
  WxConditions[0].Visibility  = current["visibility"];        if (print) Serial.println("Visi: " + String(WxConditions[0].Visibility));
  WxConditions[0].Windspeed   = current["wind_speed"];        if (print) Serial.println("WSpd: " + String(WxConditions[0].Windspeed));
  WxConditions[0].Winddir     = current["wind_deg"];          if (print) Serial.println("WDir: " + String(WxConditions[0].Winddir));

  JsonArray hourlyArray = doc["hourly"];

  for (int r = 0; r < max_readings; r++) {
    JsonObject hourly = hourlyArray[r];
    if (print) Serial.println("\nPeriod-" + String(r) + "--------------");
    WxForecast[r].Dt          = hourly["dt"];                 if (print) Serial.println(ConvertUnixTime(WxForecast[r].Dt));
    WxForecast[r].Temperature = hourly["temp"];               if (print) Serial.println("Temp: " + String(WxForecast[r].Temperature));
    WxForecast[r].FeelsLike   = hourly["feels_like"];         if (print) Serial.println("FLik: " + String(WxForecast[r].FeelsLike));
    WxForecast[r].Pressure    = hourly["pressure"];           if (print) Serial.println("Pres: " + String(WxForecast[r].Pressure));
    WxForecast[r].Humidity    = hourly["humidity"];           if (print) Serial.println("Humi: " + String(WxForecast[r].Humidity));
    WxForecast[r].DewPoint    = hourly["dew_point"];          if (print) Serial.println("DewP: " + String(WxForecast[r].DewPoint));
    WxForecast[r].Rainfall    = hourly["rain"]["1h"];         if (print) Serial.println("Rain: " + String(WxForecast[r].Rainfall));
    WxForecast[r].Snowfall    = hourly["snow"]["1h"];         if (print) Serial.println("Snow: " + String(WxForecast[r].Snowfall));
    WxForecast[r].Rainfall    = hourly["rain"]["1h"];         if (print) Serial.println("Rain: " + String(WxForecast[r].Rainfall));
    WxForecast[r].Snowfall    = hourly["snow"]["1h"];         if (print) Serial.println("Snow: " + String(WxForecast[r].Snowfall));
    JsonObject hourly_weather = hourly["weather"][0];
    WxForecast[r].Icon        = hourly["weather"][0]["icon"].as<const char*>(); if (print) Serial.println("Icon: " + String(WxForecast[r].Icon));
  }

  JsonArray daily = doc["daily"];
  if (print) Serial.println("\nHigh/Low Temperatures --------------");
  for (int r = 0; r < 8; r++) { // Maximum of 8-days!
    if (print) Serial.println("\nDay-" + String(r) + "--------------");
    JsonObject daily_values = daily[r];
    Daily[r].Dt          = daily_values["dt"];                                   if (print) Serial.println(ConvertUnixTime(Daily[r].Dt));
    Daily[r].Description = daily_values["summary"].as<const char*>();            if (print) Serial.println("Summary: " + Daily[r].Description);
    Daily[r].Temperature = daily_values["day"];                                  if (print) Serial.println("Temp   : " + String(Daily[r].Temperature));
    Daily[r].High        = daily_values["temp"]["max"];                          if (print) Serial.println("High   : " + String(Daily[r].High));
    Daily[r].Low         = daily_values["temp"]["min"];                          if (print) Serial.println("Low    : " + String(Daily[r].Low));
    Daily[r].Humidity    = daily_values["humidity"];                             if (print) Serial.println("Humi   : " + String(Daily[r].Humidity));
    Daily[r].PoP         = daily_values["pop"];                                  if (print) Serial.println("PoP    : " + String(Daily[r].PoP*100, 0) + "%");
    Daily[r].UVI         = daily_values["uvi"];                                  if (print) Serial.println("UVI    : " + String(Daily[r].UVI, 1));
    Daily[r].Rainfall    = daily_values["rain"];                                 if (print) Serial.println("Rain   : " + String(Daily[r].Rainfall));
    Daily[r].Snowfall    = daily_values["snow"];                                 if (print) Serial.println("Snow   : " + String(Daily[r].Snowfall));
    Daily[r].Icon        = daily_values["weather"][0]["icon"].as<const char*>(); if (print) Serial.println("Icon   : " + String(Daily[r].Icon));
  }
  //------------------------------------------
  float pressure_trend = WxForecast[0].Pressure - WxForecast[2].Pressure; // Measure pressure slope between ~now and later
  pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations of less than 0.1
  WxConditions[0].Trend = "=";
  if (pressure_trend > 0)  WxConditions[0].Trend = "+";
  if (pressure_trend < 0)  WxConditions[0].Trend = "-";
  if (pressure_trend == 0) WxConditions[0].Trend = "0";
  if (Units == "I") Convert_Readings_to_Imperial();
  return true;
}


#endif /* ifndef COMMON_H_ */
