

/* WiFi RGB laser controller*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <string>

#ifndef APSSID
#define APSSID "laser" 
#define APPSK "thisismylaser"
#endif

/* Set these to your desired credentials. ESP8266 acts as the wifi AP */
const char *ssid = APSSID;
const char *password = APPSK;

const int pinR = 14;
const int pinG = 12;
const int pinB = 13;

ESP8266WebServer server(80);

/* 
  http://192.168.4.1    
  writing html in this IDE really sucks... so definitely do it outside and just paste it in
*/
void handleRoot() {
  server.send(200, "text/html", R"(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RGB Laser Controller</title>
</head>
<body>
    <h1>Solid Color</h1>
    <input type="color" id="colorPicker"/>
    <h1>Effects</h1>
    <div id="effectButtons">
        <button data-effect="1">Rainbow</button>
    </div>
    
    <script>
        <!-- no need for anything else -->
        function postRequest(url){
            fetch(url, {method: "POST"})
            .then(response => response.text())
            .then(data => console.log(data));
        }
        
        document.getElementById("colorPicker").addEventListener("input", function (){
            const color = this.value;
            const r = parseInt(color.substr(1,2), 16);
            const g = parseInt(color.substr(3,2), 16);
            const b = parseInt(color.substr(5,2), 16);
            const url = `/color?r=${r}&g=${g}&b=${b}`;
            postRequest(url);
        });
        
        document.getElementById("effectButtons").addEventListener("click", function(event){
            if (event.target.tagName === "BUTTON"){
                const effect = event.target.getAttribute("data-effect");
                const url = `/effect?mode=${effect}`
                postRequest(url);
            }
        });
    </script>
</body>
</html>
  )");
}

enum RgbMode {
  NONE,
  SOLID_COLOR,
  EFFECT
};
RgbMode rgbMode = NONE;

void handleColor() {
  // non numbers or out of 8b range are UB, so your fault
  uint8_t r = ("0" + server.arg("r")).toInt();
  uint8_t g = ("0" + server.arg("g")).toInt();
  uint8_t b = ("0" + server.arg("b")).toInt();
  Serial.printf("red: %d, green: %d, blue: %d\n", r, g, b);
  rgbMode = SOLID_COLOR;
  emitRGB(r, g, b);
  // why the hell is this necessary, c++?
  char buf[100];
  sprintf(buf, "set red: %d, green: %d, blue: %d", r, g, b);
  server.send(200, "text/html", buf);
}

void handleRainbowDemo() {
  server.send(200, "text/html", "effect rainbow");
  rgbMode = EFFECT;
}

// h in [1,360), s and v in [0,1]. Sorry for float usage...
void emitHSV(int16_t h, float s, float v) {
  float max = v;
  float c = s * v;
  float min = max - c;

  float hPrime = (h >= 300) ? (h - 360) / 60.0 : h / 60.0;
  if (hPrime < 1) {
    if (hPrime < 0) {
      emitRGB(max * 255, min * 255, (min - hPrime * c) * 255);
    } else {
      emitRGB(max * 255, (min + hPrime * c) * 255, min * 255);
    }
    return;
  }
  if (hPrime < 3) {
    if (hPrime < 2) {
      emitRGB((min - (hPrime - 2) * c) * 255, max * 255, min * 255);
    } else {
      emitRGB(min * 255, max * 255, (min + (hPrime - 2) * c) * 255);
    }
    return;
  }
  if (hPrime <= 5) {  // definitely
    if (hPrime < 4) {
      emitRGB(min * 255, (min - (hPrime - 4) * c) * 255, max * 255);
    } else {
      emitRGB((min + (hPrime - 4) * c) * 255, min * 255, max * 255);
    }
    return;
  }
}

//around 16 is the lower limit of the module's range, any lower looks like 0 to it
void emitRGB(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(pinR, r);
  analogWrite(pinG, g);
  analogWrite(pinB, b);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  emitRGB(0, 0, 0);
  // safe
  pinMode(4, OUTPUT);
  digitalWrite(4, HIGH);

  analogWriteFreq(8000);

  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid, password);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);
  server.on("/color", handleColor);
  server.on("/effect", handleRainbowDemo);
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  handleOutput();
  yield();
}

void handleOutput() {
  switch (rgbMode) {
    case NONE:
      emitRGB(0, 0, 0);
      break;
    case SOLID_COLOR:
      // already done
      break;
    case EFFECT:
      // for now this is only rainbow
      static int h;
      if (h++ == 360) h = 0;
      emitHSV(h, 1, 1);
  }
  delay(1);  // yield to wifi and shit
}

