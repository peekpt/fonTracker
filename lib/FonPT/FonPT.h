/**
 * FonPT.h
 *
 * Created on: 20.02.2017
 *
 * Copyright (c) 2017 Paulo Bruckmann. All rights reserved.
 *
 * This library uses Fon credentials to perform authorization on FON routers
 * related to NOS ISP in Portugal using ESP8266 Arduino Library.
 * (Doesn't work with NOS accounts).
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


#ifndef fonPT_h
#define fonPT_h

#define DEBUG_FON // comment to disable debug messages on serial port 0

#ifdef DEBUG_FON
#define DBG_FON(A) Serial.print(A)
#define DBGF_FON(A) Serial.print(F(A))
#else
#define DBG_FON(A)
#define DBGF_FON(A)
#endif


#define defaultFonSSID "FON_ZON_FREE_INTERNET"
// SHA1 fingerprints
#define nosFP "B9 FE F7 F1 11 4E 4C 02 DA CE 80 90 1D E9 FA 35 31 7D 6A A3"
#define fonFP  "3F CD 8E 4E 0E B9 B6 28 6C E2 4F 63 06 FF 89 6A 05 3D 04 BD"


#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

class FonPT {
  public:

    // perform authorization and returns status
    // auth(<FON_USER>,<FON_PASS>) or auth(<FON_USER>,<FON_PASS>,<FON_SSID>)
    // default SSID is FON_ZON_FREE_INTERNET
    bool auth(String fonUser, String fonPass, String fonSSID = defaultFonSSID);
    void disconnect();
    String getIntIP();
    String getExtIP();
  private:
    HTTPClient httpClient;
    String extIP;
    String intIP;
};


#endif
