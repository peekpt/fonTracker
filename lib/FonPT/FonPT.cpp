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

#include "FonPT.h"

void FonPT::disconnect(){
  DBGF_FON("DISCONNECTED");
  httpClient.end();
  WiFi.disconnect(1);
}

bool FonPT::auth(String fonUser, String fonPass, String fonSSID) {

        DBGF_FON("\n\nFON AUTH\n");
        DBGF_FON("\nCONNECTING TO ");
        DBG_FON(fonSSID);
        DBGF_FON("\n");
        const char * headerkeys[] = {"Location"};
        WiFi.mode(WIFI_STA);
        WiFi.begin(fonSSID.c_str());

        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED) {
                delay(500);
                DBGF_FON(".");
                timeout++;
                if (timeout > 20) {
                        WiFi.disconnect(true);
                        DBGF_FON("FAIL");
                        return 0;
                }
        }
        DBGF_FON("\nCONNECTED INT IP: ");
        DBG_FON(WiFi.localIP());
        DBGF_FON("\nCHECKING FOR AUTH\n");


        String captiveUrl = "";
        httpClient.begin("http://api.ipify.org/?format=txt");
        httpClient.collectHeaders(headerkeys,sizeof(headerkeys)/sizeof(headerkeys[0]));
        if (httpClient.GET() == HTTP_CODE_OK) {
                DBGF_FON("AUTH OK EXT IP: ");
                extIP = httpClient.getString();
                DBG_FON(extIP);
                DBGF_FON("\n");
                return 1;
        }else {
                DBGF_FON("NO AUTH\n\n");

                if (httpClient.hasHeader("Location")) {
                        DBGF_FON("REDIRECTING\n");
                        captiveUrl = httpClient.header("Location");

                }else {
                        DBGF_FON("FAIL");
                        return 0;
                }
        }
        extIP = ""; // reset ip
        httpClient.end();

//__________________________________________________________________________
        DBGF_FON("OPENING CAPTIVE\nGET > ");
        DBG_FON(captiveUrl);
        DBGF_FON("\n");
        httpClient.begin(captiveUrl,nosFP);
        if (httpClient.GET() == HTTP_CODE_OK) {
                DBGF_FON("OK\n");
        }else{
                DBGF_FON("FAIL");
                return 0;
        };
        httpClient.end();


//__________________________________________________________________________
        DBG_FON("PARSING URL GET >  ");
        String nasid = captiveUrl.substring(captiveUrl.indexOf("&nasid=")+7, captiveUrl.indexOf("&mac="));
        DBGF_FON("NASID: ");
        DBG_FON(nasid);
        String mac = captiveUrl.substring(captiveUrl.indexOf("&mac=")+5);
        DBGF_FON(" | MAC: ");
        DBG_FON(mac);
        DBGF_FON("\n");
        String wispUrlHomeEncoded = captiveUrl;
        wispUrlHomeEncoded.replace(":","%3A");
        wispUrlHomeEncoded.replace("/","%2F");
        wispUrlHomeEncoded.replace("?","%3F");
        wispUrlHomeEncoded.replace("=","%3D");
        wispUrlHomeEncoded.replace("&", "%26");

        String fonURL = String("https://nos2.portal.fon.com/jcp?res=hsp-login&VNPNAME=PT%3Apt&LOCATIONNAME=PT%3Apt&WISPURL=https%3A%2F%2Fcaptiveportal.nos.pt%2Ffonlogin&WISPURLHOME=") +
                        wispUrlHomeEncoded +
                        "&HSPNAME=FonZON%3APT&NASID=" + nasid +
                        "&MAC=" + mac +
                        "&lang=pt_PT&LANGUAGE=pt_PT";


//----
        DBGF_FON("TRY LOGIN\n");
        String loggedInURL ="";
        DBGF_FON("POST > ");
        DBG_FON(fonURL);
        DBG_FON("\n");
        httpClient.begin(fonURL, fonFP);
        httpClient.collectHeaders(headerkeys,sizeof(headerkeys)/sizeof(headerkeys[0]));
        httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");

        if(httpClient.POST(String("userName=")+ fonUser + "&Password=" + fonPass)) {

                if (httpClient.hasHeader("Location")) {
                        loggedInURL = httpClient.header("Location");
                        DBGF_FON("REDIRECTING\n");
                }else{
                        DBGF_FON("FAIL");
                        return 0;
                }
        }
        httpClient.end();


//----
        DBGF_FON("GET > ");
        DBG_FON(loggedInURL);
        DBG_FON("\n");
        String routerActivateURL ="";

        httpClient.begin(loggedInURL,nosFP);

        httpClient.collectHeaders(headerkeys,sizeof(headerkeys)/sizeof(headerkeys[0]));
        httpClient.GET();
        if (httpClient.hasHeader("Location")) {
                routerActivateURL = httpClient.header("Location");
                DBGF_FON("REDIRECTING\n");
        }else{
                DBGF_FON("FAIL");
                return 0;
        }
        httpClient.end();
        DBGF_FON("GET > ");
        DBG_FON(routerActivateURL);
        DBG_FON("\n");
        String finalURL ="";
        httpClient.begin(routerActivateURL);

        httpClient.collectHeaders(headerkeys,sizeof(headerkeys)/sizeof(headerkeys[0]));
        httpClient.GET();
        if (httpClient.hasHeader("Location")) {
                finalURL = httpClient.header("Location");
                DBGF_FON("REDIRECTING\n");
        }else{
                DBGF_FON("FAIL");
                return 0;
        }

        httpClient.end();
        DBGF_FON("GET > ");
        DBG_FON(finalURL);
        DBG_FON("\n");
        httpClient.begin(finalURL,nosFP);
        if (httpClient.GET() == HTTP_CODE_OK) {
                DBGF_FON("DONE\n");

        }else {
                DBGF_FON("FAIL");
                return 0;
        }
        httpClient.end();

        httpClient.begin("http://api.ipify.org/?format=txt");
        if (httpClient.GET() == HTTP_CODE_OK) {
                DBGF_FON("AUTH OK EXT IP: ");
                extIP = httpClient.getString();
                DBG_FON(extIP);
                DBGF_FON("\n");
                httpClient.end();
                return 1;
        }else{
                DBGF_FON("FAIL");
                return 0;
        }
        httpClient.end();
}

String FonPT::getIntIP() {
        return intIP;
}
String FonPT::getExtIP() {
        return extIP;
}
