// Include the needed libaries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>

// Declare acces point name en password
const char *ssid = "Fly-1";
const char *passphrase = "123456789";

// Declare ip, gateway and subnet
IPAddress local_IP(10,10,10,1);
IPAddress gateway(10,10,10,1);
IPAddress subnet(255,255,255,0);

// Declare webserver
AsyncWebServer server(80);

// Declare NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Declare variables to save current time
String formattedDate;
String timeStamp;
int hour;
int minute;

// Declare empty integers for time data
int wakeuptime_hour;
int wakeuptime_minute;
int sleeptime_hour;
int sleeptime_minute;

String wifissid;
String wifipassword;

// Declare led pin
int ledPin = 2;

// Declare preference value
Preferences preferences;

// Declare processor that's being used to display vars in html view
String processor(const String& var)
{

  if(var == "WAKEUPTIMEHOUR"){
    return String(wakeuptime_hour);
  }

  else if(var == "WAKEUPTIMEMINUTE"){
      return String(wakeuptime_minute);
  }

  else if(var == "SLEEPTIMEHOUR"){
      return String(sleeptime_hour);
  }

  else if(var == "SLEEPTIMEMINUTE"){
      return String(sleeptime_minute);
  }

  return String();
}

// Declare array of chars for the HTML code
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <body>
  <h1>Welcome to Fly-1!</h1>
    <form action='/data' method='GET'>
      At what time do you want to wake up?: <br>
        <input type='number' name='wakeuptimehour' min='0' max='23' value='%WAKEUPTIMEHOUR%' required> : <input type='number' name='wakeuptimeminute' min='0' max='59' value='%WAKEUPTIMEMINUTE%' required> <br>
      How long do you want to sleep?: <br>
        <input type='number' name='sleeptimehour' min='0' max='23' value='%SLEEPTIMEHOUR%' required> : <input type='number' name='sleeptimeminute' min='0' max='59' value='%SLEEPTIMEMINUTE%' required> <br>
      <input type='submit' value='Save'>
    </form><br>
  </body>
  </html>
)rawliteral";

// Declare array of chars for the HTML code used for the setup process
const char index_html_setup[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <body>
  <h1>Welcome to Fly-1!</h1>
    <form action='/data' method='GET'>
        Enter wifi ssid:
            <input type='text' name='ssid' required><br>
        Enter wifi password:
            <input type='password' name='password' required><br>
      <input type='submit' value='Save'>
    </form><br>
  </body>
  </html>
)rawliteral";


void setup(){

    // Start serial mnitor for debugging purposes
    Serial.begin(115200);

    pinMode(ledPin, OUTPUT);

    // Start data stream named fly1 so we can save data onto the device even when power is lossed
    preferences.begin("fly1", true);

    // Check if preference already contains values, if so set var to these values else set to 0
    wakeuptime_hour = preferences.getInt("wakeuphour", 0);
    wakeuptime_minute = preferences.getInt("wakeupminute", 0);
    sleeptime_hour = preferences.getInt("sleephour", 0);
    sleeptime_minute = preferences.getInt("sleepminute", 0);

    // Check if wifi settings are already set, else set to ""
    wifissid = preferences.getString("wifissid", "");
    wifipassword = preferences.getString("wifipassword", "");

    // Close preference connection
    preferences.end();

    // Start wifi connection if wifi settings are set
    if(wifissid != "" && wifipassword != ""){
        // Start wifi connection
        WiFi.begin(wifissid.c_str(), wifipassword.c_str());

        // Wait for wifi connection
        while (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            Serial.println("Connecting to WiFi..");
        }

        // Make post request to the server to send/update the local IP address
        HTTPClient http;
        http.begin("https://fly1-server.herokuapp.com/set-ip");
        http.addHeader("Content-Type", "application/json");
        String ip = WiFi.localIP().toString();
        String id = String(WiFi.macAddress());
        int httpResponseCode = http.POST("{\"id\" : \""+id+"\",\"ip\" : \""+ip+"\"}");

        // Start time client to get accurate time
        timeClient.begin();
        timeClient.setTimeOffset(3600);
    }else{
        // Starting the Soft AP (AP = Acces Point)
        Serial.println(WiFi.softAPConfig(local_IP, gateway, subnet));
        Serial.println(WiFi.softAP(ssid,passphrase));
    }

    // Declare "/" route
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        if(wifissid == "" && wifipassword == ""){
            request->send_P(200, "text/html", index_html_setup);
        }
        else{
            request->send_P(200, "text/html", index_html, processor);
        }
    });

    // Declare "/reset" route
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
        // Begin preference connection
        preferences.begin("fly1", false);
        // Reset all preferences
        preferences.clear();
        // Close preference connection
        preferences.end();
        // Redirect to "/"
        ESP.restart();
    });

    // Declare the "/data" route
    server.on("/data", HTTP_GET, [] (AsyncWebServerRequest *request){
        String wakeup;
        String timebefore;
        // If data route contains values for setting the data in app, set these values
        if (request->hasParam("wakeuptimehour") && request->hasParam("wakeuptimeminute") && request->hasParam("sleeptimehour") && request->hasParam("sleeptimeminute")) {
            preferences.begin("fly1", false);
            preferences.putInt("wakeuphour", request->getParam("wakeuptimehour")->value().toInt());
            preferences.putInt("wakeupminute", request->getParam("wakeuptimeminute")->value().toInt());
            preferences.putInt("sleephour", request->getParam("sleeptimehour")->value().toInt());
            preferences.putInt("sleepminute", request->getParam("sleeptimeminute")->value().toInt());
            preferences.end();
            wakeuptime_hour = request->getParam("wakeuptimehour")->value().toInt();
            wakeuptime_minute = request->getParam("wakeuptimeminute")->value().toInt();
            sleeptime_hour = request->getParam("sleeptimehour")->value().toInt();
            sleeptime_minute = request->getParam("sleeptimeminute")->value().toInt();
        }
        // If data route contains values for setting the wifi settings in app, set these values
        else if(request->hasParam("ssid") && request->hasParam("password")){
            preferences.begin("fly1", false);
            preferences.putString("wifissid", request->getParam("ssid")->value());
            preferences.putString("wifipassword", request->getParam("password")->value());
            preferences.end();
            wifissid = request->getParam("ssid")->value();
            wifipassword = request->getParam("password")->value();

            delay(1000);

            request->send(200, "text/html", String(WiFi.macAddress()));

            delay(5000);

            // Restart esp to apply new wifi settings
            ESP.restart();
        }
        // Let the user know the request was successful
        request->send(200, "text/plain", "OK");
    });

    // Start the Async webserver
    server.begin();

    // Print serial message for debugging purposes
    Serial.println("HTTP server started");

    // Delay to prevent webserver from crashing
    delay(100);
}

void loop() {
    // Check if Time is still constantly updated, if not force update
    while(!timeClient.update()) {
        timeClient.forceUpdate();
    }

    // Format the current time to a readable string
    formattedDate = timeClient.getFormattedDate();
    Serial.println(formattedDate);

    // Get the timestamp from the timeclient
    int splitT = formattedDate.indexOf("T");
    timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);

    // Split the timestampt into Hour and Minute
    hour = timeStamp.substring(0, 2).toInt();
    minute = timeStamp.substring(3, 5).toInt();

    // Check if the current time is the expected wakeup time
    if(hour == wakeuptime_hour && minute == wakeuptime_minute){
        action();
    }
    // Check if the current time is the expected wakeuptime - sleeptime
    else if(hour == (wakeuptime_hour - sleeptime_hour) && minute == (wakeuptime_minute - sleeptime_minute)){
        action();
    }
    else{
        // Does nothing yet
    }

    // Wait a seccond to prevent the loop from running every milisecond
    delay(1000);
}

// Funtion that contains the action
void action(){
    // Set the ledpin to HIGH, meaning put power on it
    digitalWrite(ledPin, HIGH);
    // Wait 59 seconds
    delay(59000);
    // Set the ledpin to LOW, meaning remove power from it
    digitalWrite(ledPin, LOW);
}
