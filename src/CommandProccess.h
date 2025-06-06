
#include <Arduino.h>
#include <Arduino_JSON.h>

void SerialProccess() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); // Read input until newline
    JSONVar json = JSON.parse(input); // Parse JSON input
    if (JSON.typeof(json) == "object") { // Check if parsed data is an object
      for (int i = 0; i < 10; i++) {
        if (json.hasOwnProperty(String(REG + i))) {
          writeValues[i] = json[String(REG + i)];
          Serial.print("Set value for register ");
          Serial.print(REG + i);
          Serial.print(": ");
          Serial.println(writeValues[i]);
        }
      }
      if(JSON.typeof(json) == "undefined") {
      Serial.println("Parsing JSON failed!");
      } else if (json.hasOwnProperty("address")) {
      JSONVar addressArray = json["address"];
      if (addressArray.length() >= 2) {
        if (addressArray[0].hasOwnProperty("addressRead")) {
          int addrRead = (int)addressArray[0]["addressRead"];
          Serial.print("Set addressRead: ");
          Serial.println(addrRead);
          REG = addrRead; // Assign addrRead to REG variable
        }
        if (addressArray[1].hasOwnProperty("addressWrite")) {
          int addrWrite = (int)addressArray[1]["addressWrite"];
          Serial.print("Set addressWrite: ");
          Serial.println(addrWrite);
          REG2 = addrWrite; // Assign addrWrite to REG2 variable
        }
      }
      } else if (json.hasOwnProperty("remote")) {
        remote = IPAddress(
          (int)json["remote"][0],
          (int)json["remote"][1],
          (int)json["remote"][2],
          (int)json["remote"][3]
        );
        mb.connect(remote); 
      } else if (json.hasOwnProperty("role")) {
        role = json["role"];
        Serial.print("Set role: ");
        Serial.println(role ? "Master" : "Slave");
        role ? mb.client() : mb.server(502); // Set Modbus role
      } else if(json.hasOwnProperty("wificonfig")) {
        JSONVar wifiConfig = json["wificonfig"];
        if (wifiConfig.hasOwnProperty("ssid") && wifiConfig.hasOwnProperty("password")) {
          String ssid = wifiConfig["ssid"];
          String password = wifiConfig["password"];
          Serial.print("Connecting to WiFi SSID: ");
          Serial.println(ssid);
          Serial.print("Password: ");
          Serial.println(password);
          String localIP = WifiConect(ssid, password); // Connect to WiFi
          Serial.print("Local IP Address: ");
          Serial.println(localIP);
        } else {
          Serial.println("Invalid WiFi configuration in JSON input");
        }
      }
      else {
        Serial.println("Invalid JSON input");
      }
    }
  }
}