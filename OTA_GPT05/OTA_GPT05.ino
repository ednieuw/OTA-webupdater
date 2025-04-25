/*
Upload the sketch
If connected for the first time
- Connect in the WIFI settings of your device to Edsoft-ESP32-OTA
- IF the WIFI manager page does not open after a few seconds
  open the URL: 192.168.4.1

 Option 1: Choose UPLOAD and upload the bin file. 
           Your new software will start. 
 Option 2: Select the blue button "Enter credentials". 
           Choose the router from the list and enter the password.
           (The IP adres is printed in the serial monitor)
           Restart the ESP32 and it will connect to WIFI.
           Open the web page by entering in the URL: edsoft.local
           Select and upload the bin file.
           Wait 10 sec and the root page of the application will start if available. 

If credentials are known the app opens a upload page and 
will start the root menu page after 10 seconds if available.

*/

#include <WiFi.h>
#include <WiFiManager.h> // install via Library Manager by tapazu
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

AsyncWebServer server(80);
bool shouldReboot = false;

// HTML page stored in flash
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>OTA Wordclock Firmware Upload</title>
  <style>
    body { font-family: Arial, sans-serif; padding: 20px; }
    #progressBar {
      width: 100%;
      background-color: #ccc;
      border-radius: 8px;
      margin-top: 10px;
      height: 25px;
      overflow: hidden;
    }
    #progressBarFill {
      height: 100%;
      width: 0%;
      background-color: #4caf50;
      text-align: center;
      line-height: 25px;
      color: white;
    }
  </style>
</head>
<body>
  <h2 style="color: #FFBB00;"> Upload Wordclock Firmware (.bin)</h2>
  <form id="uploadForm">
    <input type="file" id="file" style="width: 100%; accept=".bin" required>
    <br><br>
    <button type="submit">Upload</button>
  </form>

  <div id="progressBar">
    <div id="progressBarFill">0%</div>
  </div>
  <br>
    <h3 style="color: #FFAA00;"> 
    Reload the browser page 10 seconds after the upload</h3>
  <script>
    const form = document.getElementById('uploadForm');
    const progressFill = document.getElementById('progressBarFill');

    form.addEventListener('submit', function (e) {
      e.preventDefault();
      const file = document.getElementById('file').files[0];
      if (!file) return;

      const xhr = new XMLHttpRequest();
      xhr.open('POST', '/update');

      xhr.upload.addEventListener('progress', function (e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          progressFill.style.width = percent + '%';
          progressFill.textContent = percent + '%';
        }
      });

xhr.onload = function () {
  if (xhr.status === 200) {
    progressFill.textContent = 'Upload complete! Rebooting...';
    // Give ESP32 time to reboot and then reload page
    setTimeout(() => {
      location.reload();
    }, 10000); // ⏳ reload after 10 seconds
  } else {
    progressFill.style.backgroundColor = 'red';
    progressFill.textContent = 'Upload failed.';
  }
};

      const formData = new FormData();
      formData.append('update', file);
      xhr.send(formData);
    });
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);

  // WiFiManager auto-connect with fallback AP
  WiFiManager wifiManager;

  // Optional: reset saved credentials for testing
   wifiManager.resetSettings();

  // This will block until connected or user configures new credentials
  wifiManager.autoConnect("Edsoft-ESP32-OTA");

  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 // Start mDNS responder
  if (MDNS.begin("edsoft")) {
    Serial.println("mDNS responder edsoft.local started");
  } else {
    Serial.println("Error starting mDNS");
  }

  // Serve the HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

// Handle the actual OTA upload
server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {
  shouldReboot = true;
    request->send(200, "text/html", 
      "<!DOCTYPE html><html><body>"
      "<h2>Update successful. Rebooting...</h2>"
      "<p>You will be redirected shortly.</p>"
      "<script>setTimeout(()=>{location.href='/'}, 10000);</script>"
      "</body></html>");
  }, 
  [](AsyncWebServerRequest *request, String filename, size_t index,
        uint8_t *data, size_t len, bool final){

    if (!index) {
      Serial.printf("OTA Start: %s\n", filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    }

    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }

    if (final) {
      if (Update.end(true)) {
        Serial.printf("OTA Success: %u bytes\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
}

void loop() {
   if (shouldReboot) { delay(1000);   ESP.restart(); }                                         // After finish OTA update restart
}
