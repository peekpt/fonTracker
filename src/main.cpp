/**
 * Fon Tracker
 *
 * Created on: 06.03.2017
 *
 * Copyright (c) 2017 Paulo Bruckmann. All rights reserved.
 *
 * All product names brands are property of their respective owners.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


 // google api server sha-1 fingerprint ( may change overtime )
#define GOOGLE_APIS_SERVER_FINGERPRINT "22 37 4D 58 43 F4 A1 24 12 71 2B 74 7A FC 36 FC 24 A0 F0 9D"

#include <Arduino.h>
#include <EEPROM.h>
#include <FonPT.h> // https://github.com/peekpt/FonPT
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <Ticker.h>

#define LEDPIN 2

float watchdog = millis(); // prevent to be on for a long time

// global vars store config values
String _user_,_pass_,_apikey_,_dweet_;
int _interval_,_firstBoot_;

// eeprom config structure
const int buff = 64;
struct config_struct {
        int firstBoot;
        char user[buff];
        char pass[buff];
        char dweet[buff];
        char apikey[buff];
        int interval;
};

config_struct config;

bool dnsLoop = false;
bool isBlinking = false;

// initiate objects and vars
FonPT fon;                        // fon library
Ticker ticker;                    // led ticker
DNSServer dnsServer;              // servers
ESP8266WebServer webServer(80);
String html_home = "";            // html payload for the  config page
const IPAddress apIP(192, 168, 1, 1);
const char* apSSID = "TRACKER";
unsigned int flips = 0;
bool foundFON = false; // flag if a FON_ZON_FREE_INTERNET network was found

#define NOT_FIRST_BOOT 12345 // just a random int to flag first boot

String urlDecode(String input);
void sleepNow(bool status = true);
void blink(int blinks);
String makePage(String title, String headerTitle, String contents);
void loadConfig();
void saveConfig();
// enable battery readings
ADC_MODE(ADC_VCC);
#define BATT_OFFSET 0.4 // fine tune the vcc readings
float readVCC();

void resetConfig();
void setup(){
        pinMode(0,INPUT); // program mode pin
        pinMode(2,OUTPUT); // led pin
        digitalWrite(LEDPIN,LOW);
        Serial.begin(115200);
        delay(10);

        loadConfig(); // load eeprom vars

        // this will prevent crazy chars on the textfields on first use
        if (_firstBoot_ != NOT_FIRST_BOOT) { // save first boot values
              resetConfig();
        }

        //prepare html config page
        html_home += "<form method=\"get\" action=\"config\">";
        html_home += "<label>Fon Credentials: </label><br>";
        html_home += "Email:<br> <input name=\"user\" maxlength=\"64\" type=\"text\" value=\"" + _user_ + "\"><br>";
        html_home += "Password:<br> <input name=\"pass\" maxlength=\"64\" type=\"password\" value=\"" + _pass_ + "\"><br><br><br>";
        html_home += "dweet.io thing name:<br> <input name=\"dweet\" maxlength=\"64\" size=\"64\" type=\"text\" value=\"" + _dweet_ + "\"><br>";
        html_home += "Google geolocate API key<br> <input name=\"apikey\" maxlength=\"64\" size=\"64\" type=\"text\" value=\"" + _apikey_ + "\"><br>";
        html_home += "Beacon Interval:<br> <input name=\"interval\" maxlength=\"4\" type=\"text\" size=\"4\" value=\"" + String(_interval_) + "\"><br>";
        html_home+= "<input type=\"submit\" name=\"save\"></form>";

        // flags if the process was successful
        bool success = false;

/* PROGRAM MODE WIFI SSID "TRACKER" Browse IP->192.168.1.1

  After reset Program mode can be activated with connecting GND to GPIO0
  Single click - Enter program mode;
  Double click - Reset Config;

 */
        int tout = 100;
        while (tout != 0) {
                if (digitalRead(0) == HIGH && _pass_.length() > 0) {
                        tout--;
                        delay (40);
                } else  {
                    digitalWrite(LEDPIN, HIGH);
                        delay (100);
                        float buttonMillis = millis();

                        while(millis() - buttonMillis < 750 ) {
                            delay(100);
                            if (digitalRead(0) == LOW) {
                              // reset config
                              resetConfig();
                              digitalWrite(LEDPIN, LOW);
                              for (int l=0; l<50; l++){
                                delay(50);
                                digitalWrite(LEDPIN,!digitalRead(LEDPIN));
                              }
                              delay(1000);
                              ESP.restart();
                            }
                        }

                        // ** PROGRAM MODE **
                        digitalWrite(LEDPIN,HIGH);
                        Serial.println(F("\n\n\nProgram Mode!"));
                        blink(999);
                        // make shure to kill all connections
                        WiFi.disconnect();
                        delay(100);
                        WiFi.mode(WIFI_AP);
                        WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
                        WiFi.softAP(apSSID);
                        dnsServer.start(53, "*", apIP);
                        dnsLoop = true; // make sure dnsserver starts before the loop

                        // webserver callbacks
                        webServer.on("/", []() { // main page
                                webServer.send(200, "text/html", makePage("FON WiFi Tracker", "FON WiFi Tracker", html_home));
                        });

                        webServer.on("/config", []() { // confirm page
                                _user_ = urlDecode(webServer.arg("user"));
                                _pass_ = urlDecode(webServer.arg("pass"));
                                _apikey_ = urlDecode(webServer.arg("apikey"));
                                _dweet_ =urlDecode(webServer.arg("dweet"));
                                _interval_ = atoi(urlDecode(webServer.arg("interval")).c_str());
                                String s = "<h1>Wifi Fon Tracker Configuration completed.</h1><br>rebooting...";
                                webServer.send(200, "text/html", makePage("Configuration", "Configuration", s));
                                saveConfig(); // save and reboot
                                ESP.restart();
                        });

                        // start webserver and quit setup function
                        webServer.begin();
                        return;

                }
        }

        // ** TRACKER MODE **

        blink(1);
        // make sure to disconnect from other networks before scan
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true);
        delay(100);

        // scan wifi - google needs 2 or more networks to geolocate
        int nNetworks = WiFi.scanNetworks();
        if (nNetworks  < 2) {
                Serial.println(F("FEW NETWORKS TO GEOLOCATE"));
                sleepNow(false);
        }  else  {
                blink(1);
                // if more than 2 networks continue
                Serial.println();
                Serial.print(nNetworks);
                Serial.println(F(" networks found"));
                String wifiNames = ""; // A string of WiFi SSIDs found

                // print the networks found
                for (int i = 0; i < nNetworks; ++i)
                {
                        // Print SSID and RSSI for each network found
                        Serial.print(i + 1);
                        Serial.print(F(": "));
                        Serial.print(WiFi.SSID(i));
                        Serial.print(F(" ("));
                        Serial.print(WiFi.RSSI(i));
                        Serial.print(F(")"));
                        Serial.print((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
                        Serial.print(F(" BSSID-> "));
                        Serial.println(WiFi.BSSIDstr(i));
                        // contruct a wifi networks string urlencoded de space
                        wifiNames += WiFi.SSID(i);
                        wifiNames += ",%20";
                        // check for  at least on fon netwwork
                        if (WiFi.SSID(i) == "FON_ZON_FREE_INTERNET") foundFON = true;

                }
                Serial.println(F("NO Foneras around! :("));
                if (!!!foundFON) sleepNow(false); // no FON, no fun :(
                // contruct the JSON data
                // google needs a POST with a json body with all networks in a array

                String jsonData = "{\"wifiAccessPoints\":[";
                int ncap = (nNetworks > 20) ? 20 : nNetworks; // caps the networks to json max 20
                for (int x = 0; x < ncap; ++x) {
                        long rssi = WiFi.RSSI(x);
                        unsigned int channel =  WiFi.channel(nNetworks);
                        char channel_str[10];
                        sprintf( channel_str, "%d", WiFi.channel(x) );
                        jsonData += String("{");
                        jsonData += "\"macAddress\":\"" + WiFi.BSSIDstr(x) + "\",";
                        jsonData += "\"signalStrength\":" + String(rssi)  + ",";
                        jsonData += "\"age\":0,";
                        jsonData += "\"channel\":" + String(channel_str) + ",";
                        jsonData += "\"signalToNoiseRatio\":0}";
                        jsonData += (x == ncap-1) ? "" : ",";
                }
                jsonData += "]}";

                // getting fon internet 3 retries
                int retries_conn = 3;
                bool internet = false;
                do {
                        if (fon.auth(String(_user_), String(_pass_))) {
                                retries_conn = 0;
                                internet = true;
                        }else {
                                blink(2);
                                retries_conn--;
                        }
                } while (retries_conn != 0);

                if ( internet ) {
                        // I haz internet .... :)
                        blink(1);
                        HTTPClient http;
                        http.setTimeout(2000); // 3 seconds...
                                               // send data to google
                        int googleRetries = 10;
                        do {
                                blink(1);
                                String googleUrl = "https://www.googleapis.com/geolocation/v1/geolocate?key=";
                                googleUrl += _apikey_;
                                Serial.println(googleUrl);
                                Serial.println(jsonData);
                                Serial.println(F("Requesting coordinates from google"));
                                http.begin(googleUrl, GOOGLE_APIS_SERVER_FINGERPRINT);
                                //http.begin(googleUrl);
                                http.addHeader("Content-Type", "application/json");
                                //http.addHeader("Content-Length",String(jsonData.length()));

                                const char * hKeys[] = {"Date"};
                                size_t hKeyssize = sizeof(hKeys)/sizeof(char*);
                                http.collectHeaders(hKeys,hKeyssize);

                                int status = http.POST(jsonData);
                                if (status == HTTP_CODE_OK) {
                                        // parse payload json
                                        String payloadJson =  http.getString();
                                        Serial.println(payloadJson);

                                        String date = http.header("Date");
                                        date.replace(" ","%20"); // remove spaces
                                        Serial.println(date);
                                        http.end();
                                        payloadJson.replace(" ", ""); // remove white spaces
                                        Serial.println(F("coordinates:"));
                                        String latitude = payloadJson.substring( payloadJson.indexOf("\"lat\":")+6, payloadJson.indexOf(",") );
                                        String longitude = payloadJson.substring( payloadJson.indexOf("\"lng\":")+6, payloadJson.indexOf("}")-2 );
                                        String accuracy = payloadJson.substring( payloadJson.indexOf("\"accuracy\":")+11, payloadJson.length()-3 );
                                        Serial.println(latitude);
                                        Serial.println(longitude);
                                        Serial.println(accuracy);


                                        // dweeting to dweet.io
                                        int dweetRetries = 10;
                                        do {
                                                blink(1);
                                                wifiNames.replace(" ", "%20"); // be sure to be no spaces in url
                                                Serial.print("Publishing to dweet.io -> ");
                                                String dweet = "https://dweet.io/dweet/for/";
                                                dweet += _dweet_;
                                                dweet += "?";
                                                dweet += "lt=" + latitude + "&";
                                                dweet += "ln=" + longitude + "&";
                                                dweet += "ac=" + accuracy + "&";
                                                dweet += "bt=" + String(readVCC()) + "&";
                                                dweet += "nt=" + String(nNetworks) + "&";
                                                dweet += "dt=" + String(date) + "&";
                                                dweet += "wf=" + wifiNames;
                                                Serial.println(dweet);
                                                http.begin(dweet,"27 6F AA EF 5D 8E CE F8 8E 6E 1E 48 04 A1 58 E2 65 E8 C9 34");
                                                if (http.GET() == HTTP_CODE_OK) {
                                                        Serial.println(http.getString());
                                                        dweetRetries = 0;
                                                        success = true;

                                                } else {
                                                        Serial.println(F("Retry to dweet"));
                                                        dweetRetries--;
                                                        blink(2);

                                                }

                                        } while (dweetRetries != 0);


                                        googleRetries = 0;
                                } else {
                                        googleRetries--;
                                        Serial.print(F("retry google server... "));
                                        Serial.println(status);
                                        blink(2);

                                }
                                http.end();
                        } while (googleRetries != 0);

                }
        }
        sleepNow(success);
}

void loop() {
        if (dnsLoop) {
                dnsServer.processNextRequest();
        }
        webServer.handleClient();
        if (millis()-watchdog > 600000) {
          ESP.deepSleep(0);
        }
}

String urlDecode(String input) {
        String s = input;
        s.replace("%20", " "); s.replace("+", " "); s.replace("%21", "!"); s.replace("%22", "\"");
        s.replace("%23", "#"); s.replace("%24", "$"); s.replace("%25", "%"); s.replace("%26", "&");
        s.replace("%27", "\'"); s.replace("%28", "("); s.replace("%29", ")"); s.replace("%30", "*");
        s.replace("%31", "+"); s.replace("%2C", ","); s.replace("%2E", "."); s.replace("%2F", "/");
        s.replace("%2C", ","); s.replace("%3A", ":"); s.replace("%3A", ";"); s.replace("%3C", "<");
        s.replace("%3D", "="); s.replace("%3E", ">"); s.replace("%3F", "?"); s.replace("%40", "@");
        s.replace("%5B", "["); s.replace("%5C", "\\"); s.replace("%5D", "]"); s.replace("%5E", "^");
        s.replace("%5F", "-"); s.replace("%60", "`");
        return s;
}

void sleepNow(bool status){
// blinks  4x for error 10x for success
        blink((status) ? 10 : 4);
        while(isBlinking){ // don't sleep before blinking
          yield();
        }
        Serial.println(F("ESP8266 in sleep mode"));
        ESP.deepSleep(_interval_ * 60000000);
}

void blink(int blinks) {
        if(flips == 0) {
                while(isBlinking){
                  yield(); // wait before blink again
                }
                digitalWrite(LEDPIN,HIGH);
                ticker.attach(0.2, blink, blinks);
                isBlinking = true;
        }
        flips++;
        if (flips > blinks * 2) {
                ticker.detach();
                flips = 0;
                digitalWrite(LEDPIN,HIGH);
                isBlinking = false;
        }else{
                digitalWrite(LEDPIN,!digitalRead(2));
        }
}

float readVCC(){
        float volts = 0.00f;
        volts = ESP.getVcc() / 1024.00f + BATT_OFFSET;
        return volts;
}

String makePage(String title, String headerTitle, String contents) {
        String s = "<!DOCTYPE html><html><head>";
        s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
        s += "<style>";
        // Simple Reset CSS
        s += "*,*:before,*:after{-webkit-box-sizing:border-box;-moz-box-sizing:border-box;box-sizing:border-box}html{font-size:100%;-ms-text-size-adjust:100%;-webkit-text-size-adjust:100%}html,button,input,select,textarea{font-family:sans-serif}article,aside,details,figcaption,figure,footer,header,hgroup,main,nav,section,summary{display:block}body,form,fieldset,legend,input,select,textarea,button{margin:0}audio,canvas,progress,video{display:inline-block;vertical-align:baseline}audio:not([controls]){display:none;height:0}[hidden],template{display:none}img{border:0}svg:not(:root){overflow:hidden}body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}";
        // Basic CSS Styles
        s += "body{font-family:sans-serif;font-size:16px;font-size:1rem;line-height:22px;line-height:1.375rem;color:#585858;font-weight:400;background:#fff}p{margin:0 0 1em 0}a{color:#cd5c5c;background:transparent;text-decoration:underline}a:active,a:hover{outline:0;text-decoration:none}strong{font-weight:700}em{font-style:italic}h1{font-size:32px;font-size:2rem;line-height:38px;line-height:2.375rem;margin-top:0.7em;margin-bottom:0.5em;color:#343434;font-weight:400}fieldset,legend{border:0;margin:0;padding:0}legend{font-size:18px;font-size:1.125rem;line-height:24px;line-height:1.5rem;font-weight:700}label,button,input,optgroup,select,textarea{color:inherit;font:inherit;margin:0}input{line-height:normal}.input{width:100%}input[type='text'],input[type='email'],input[type='tel'],input[type='date']{height:36px;padding:0 0.4em}input[type='checkbox'],input[type='radio']{box-sizing:border-box;padding:0}";
        // Custom CSS
        s += "header{width:100%;background-color: #2c3e50;top: 0;min-height:60px;margin-bottom:21px;font-size:15px;color: #fff}.content-body{padding:0 1em 0 1em}header p{font-size: 1.25rem;float: left;position: relative;z-index: 1000;line-height: normal; margin: 15px 0 0 10px}";
        s += "</style>";
        s += "<title>";
        s += title;
        s += "</title></head><body>";
        s += "<header><p>" + headerTitle + "</p></header>";
        s += "<div class=\"content-body\">";
        s += contents;
        s += "</div>";
        s += "</body></html>";
        return s;
}

void loadConfig(){
        EEPROM.begin(512);
        EEPROM.get(10,config);
        _firstBoot_  = config.firstBoot;
        _user_ = String(config.user);
        _pass_ = String(config.pass);
        _apikey_ = String(config.apikey);
        _dweet_ = String(config.dweet);
        _interval_ = config.interval;
        EEPROM.end();

}

void saveConfig(){
        EEPROM.begin(512);
        strcpy_P(config.user, _user_.c_str());
        strcpy_P(config.pass, _pass_.c_str());
        strcpy_P(config.dweet,_dweet_.c_str());
        strcpy_P(config.apikey,_apikey_.c_str());
        config.interval = _interval_;
        config.firstBoot = _firstBoot_;
        EEPROM.put(10,config);
        EEPROM.commit();
        EEPROM.end();
}

void resetConfig(){
  _firstBoot_  = NOT_FIRST_BOOT;
  _user_ = "your fon email";
  _pass_ = "";
  _apikey_ = "google geolocate api key";
  _dweet_ = "your tracker unique name";
  _interval_ = 5;
  saveConfig();
}
