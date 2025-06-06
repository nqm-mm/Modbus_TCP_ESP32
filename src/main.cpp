/*
  Modbus-Arduino Example - Master Modbus IP Client (ESP8266/ESP32)
  Read Holding Register from Server device

  (c)2018 Alexander Emelianov (a.m.emelianov@gmail.com)
  https://github.com/emelianov/modbus-esp8266
*/
//slave là server, master là client
//1 thanh ghi là 2 byte = 1 word
//2 thanh ghi là 4 byte = 1 dword
//2 thanh ghi là 4 byte = 1 float
#ifdef ESP8266
 #include <ESP8266WiFi.h>
#else
 #include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>
#include <Arduino_JSON.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

 int REG = 100;               // Modbus Hreg Offset
 int REG2 = 110;             // Modbus Hreg Offset reading from máter 
IPAddress remote(192,168,1,4);  // Address of Modbus Slave device
const int LOOP_COUNT = 10;
uint16_t writeValues[10];

ModbusIP mb;  //ModbusIP object

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Create a WebSocket object
AsyncWebSocket ws("/ws");

bool role = false; // Register value to read
// true = master, false = slave
String createJsonResponse(const String& dirPath = "/") ;
uint16_t gc(TRegister* r, uint16_t v) { // Callback function
  if (r->value != v) {  // Check if Coil state is going to be changed
    Serial.print("Regis address: ");
    Serial.println(r->address.address);
    Serial.print("Set reg: ");
    Serial.println(v);
  }
  return v;
}

String WifiConect(String ssid, String password) {
  WiFi.begin(ssid.c_str(), password.c_str()); // Connect to WiFi network
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Wait for connection
    delay(500);
    Serial.print(".");
  }
  return WiFi.localIP().toString(); // Return local IP address
}

void setRole(bool isMaster) {
if(isMaster) {//master
    mb.client();
    if (mb.isConnected(remote)) {   // Check if connection to Modbus Slave is established
    Serial.println("Connect successfully. Reading Holding Registers from Slave...");
    }
  } else {//slave
    mb.server(502); // Start Modbus Server on port 502
    for(int i = 0; i < 10; i++) {
      mb.addHreg(REG + i, 0); // Tạo 10 thanh ghi tại địa chỉ từ REG (100) đến REG+9 (109), giá trị khởi tạo là 0.
      mb.addHreg(REG2 + i, 0); // Add Holding Registers at offset REG with initial value 0
      mb.onSetHreg (REG + i, gc);     //Gán hàm callback gc cho mỗi thanh ghi, để xử lý khi có lệnh ghi vào thanh ghi này.
      mb.onSetHreg (REG2 + i, gc);     // Assign Callback on set the Coil
    }
    Serial.println("Modbus Server started");
  }
}

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

      //SET VALUE
      } else if (json.hasOwnProperty("setValue")) {
        JSONVar setValue = json["setValue"];
        if (setValue.hasOwnProperty("address")) {
          REG = (int)setValue["address"]; // Set REG to the address specified in JSON
          Serial.print("Set address: ");
          Serial.println(REG);
        }
        if (setValue.hasOwnProperty("value")) {
          int value = (int)setValue["value"]; // Set value to the value specified in JSON
          Serial.print("Set value: ");
          Serial.println(value);
          mb.Hreg(REG, value); // Write value to Holding Register at REG address
        }
      
      //ADDRESS
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

      //REMOTE
      } else if (json.hasOwnProperty("remote")) {
        remote = IPAddress(
          (int)json["remote"][0],
          (int)json["remote"][1],
          (int)json["remote"][2],
          (int)json["remote"][3]
      );
        mb.connect(remote); 
      } else if (json.hasOwnProperty("role")) {
        role = (int)json["role"];
        Serial.print("Set role: ");
        Serial.println(role ? "Master" : "Slave");
        role ? mb.client() : mb.server(502); // Set Modbus role
        setRole(role); // Call setRole function to update Modbus role
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
      } else {
        Serial.println("Invalid JSON input");
      }
    }
  }
}

// String createJsonResponse(const String& dirPath = "/") {
//   String jsonResponse = "{";

//   File root = LittleFS.open(dirPath);
//   if (!root) {
//     jsonResponse += "\"error\":\"Path not found\",";
//     jsonResponse += "\"file\":[],";
//   } else if (!root.isDirectory()) {
//     // Nếu là file: trả về thông tin duy nhất
//     jsonResponse += "\"file\":[{";
//     jsonResponse += "\"name\":\"" + String(root.name()) + "\",";
//     jsonResponse += "\"size\":" + String(root.size()) + ",";
//     jsonResponse += "\"dir\":\"" + String(dirPath) + "\",";
//     jsonResponse += "\"type\":\"file\"";
//     jsonResponse += "}],";
//   } else {
//     // Nếu là thư mục: liệt kê tất cả file/thư mục con
//     jsonResponse += "\"file\":[";
//     File entry = root.openNextFile();
//     bool first = true;
//     while (entry) {
//       if (!first) jsonResponse += ",";
//       first = false;

//       jsonResponse += "{";
//       jsonResponse += "\"name\":\"" + String(entry.name()) + "\",";
//       jsonResponse += "\"size\":" + String(entry.size()) + ",";
//       jsonResponse += "\"dir\":\"" + String(dirPath) + "\",";
//       jsonResponse += "\"type\":\"" + String(entry.isDirectory() ? "folder" : "file") + "\"";
//       jsonResponse += "}";

//       entry = root.openNextFile();
//     }
//     jsonResponse += "],";
//   }

//   // Thêm thông tin bộ nhớ
//   jsonResponse += "\"userStatus\":{";
//   jsonResponse += "\"free\":" + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + ",";
//   jsonResponse += "\"used\":" + String(LittleFS.usedBytes()) + ",";
//   jsonResponse += "\"total\":" + String(LittleFS.totalBytes());
//   jsonResponse += "}";

//   jsonResponse += "}";

//   root.close();
//   return jsonResponse;
// }

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    JSONVar req = JSON.parse(message);
    Serial.println("Received WebSocket message: " + message);

    if (JSON.typeof(req) == "undefined") {
      ws.textAll("{\"error\":\"Invalid JSON\"}");
      return;
    }

    //SHOW
    // Handle different commands Show list files
    //{"cmnd":"show","dir":"/"}
    if (req.hasOwnProperty("cmnd") &&
        String((const char*)req["cmnd"]) == "show" &&
        req.hasOwnProperty("dir")
        ) {
      Serial.println("Show files in directory: " + String((const char*)req["dir"]));
      String dirPath = (const char*)req["dir"];
      String json = createJsonResponse(dirPath);
      ws.textAll(json);
      Serial.println("Sending response: " + json); 
    }

    // Handle delete file or directory
    //{"cmnd":"delete","name":"hello1.txt","dir":"/"}
    //{"cmnd":"delete","name":"folder1"}
    else if (req.hasOwnProperty("cmnd") && 
        String((const char*)req["cmnd"]) == "delete"
        ) {

      if (req.hasOwnProperty("type") && String((const char*)req["type"]) == "folder") {
        if (LittleFS.rmdir(String((const char*)req["dir"]) + String((const char*)req["name"]))) {
          Serial.println("Directory deleted successfully");
        } else {
          Serial.println("Failed to delete directory");
        }

      } else if (req.hasOwnProperty("type") && String((const char*)req["type"]) == "file") {
        if (LittleFS.remove(String((const char*)req["dir"]) + String((const char*)req["name"]))) {
          Serial.println("File deleted successfully");
        } else {
          Serial.println("Failed to delete file");
        }
      }

      String json = createJsonResponse(req.hasOwnProperty("dir") ? (const char*)req["dir"] : "/");
      ws.textAll(json);
    }

    // Handle download file
    //{"cmnd":"download","name":"/hello.txt","dir":"/"}
    else if (req.hasOwnProperty("cmnd") && 
        String((const char*)req["cmnd"]) == "download" &&
        req.hasOwnProperty("dir")
        ){

      String dirPath = (const char*)req["dir"];
      String nameFile = (const char*)req["name"];
      Serial.println("Downloading file: " + nameFile);
      File file = LittleFS.open(String((const char*)req["dir"]) + nameFile, "r");
      if (!file) {
          ws.textAll("{\"error\":\"File not found\"}");
          return;

      } else {
        // Read file content
        String fileContent;

        while (file.available()) { // Read file content byte by byte
          fileContent += String((char)file.read());
          String json = createJsonResponse(dirPath);
        }

        ws.textAll("{\"status\":\"File downloaded successfully\",\"data\":\"" + fileContent + "\"}");
        Serial.println("File downloaded successfully");
      }
      file.close();
    } 
    // Handle upload file
    //{"cmnd":"upload","name":"/hello.txt","data":"Hello World!","dir":"/"}
    // {"cmnd":"upload","name":"/helloFolder3.txt","data":"Hello World!","dir":"/folder1/folder2/folder3"}
    //{"cmnd":"upload","name":"/Hello1folder3.txt","dir":"/folder1/folder2/folder3","data":"hello folder3"}
    //{"cmnd":"upload","name":"folder1","dir":"/"}
    else if (req.hasOwnProperty("cmnd") && 
            String((const char*)req["cmnd"]) == "upload" &&
            req.hasOwnProperty("dir") 
            ) {
              
      String dirPath = (const char*)req["dir"];
      String nameFile = (const char*)req["name"];
      
      if(req.hasOwnProperty("type") && String((const char*)req["type"]) == "file") {
        File file = LittleFS.open(String(String((const char*)req["dir"])) + "/" + nameFile, "w");
        Serial.println("File created successfully");
        if(req.hasOwnProperty("data")) {
          String fileData = (const char*)req["data"];
          Serial.println("Uploading file: " + nameFile);

          if (!file) {
              ws.textAll("{\"error\":\"Failed to open file for writing\"}");
              return;
          }
          
          if (file.print(fileData)) {
              Serial.println("File uploaded successfully");
              ws.textAll("{\"status\":\"File uploaded successfully\"}");
          } else {
              Serial.println("Failed to write to file");
              ws.textAll("{\"error\":\"Failed to write to file\"}");
          }
        }
        file.close();

      } else {
        if (LittleFS.mkdir(String(String((const char*)req["dir"])) + "/" + nameFile)) {
          Serial.println("Directory created successfully");
          ws.textAll("{\"status\":\"Directory created successfully\"}");
        } else {
          ws.textAll("{\"error\":\"Failed to create directory\"}");
          return;
        }
      }
      String json = createJsonResponse(req.hasOwnProperty("dir") ? (const char*)req["dir"] : "/");
      ws.textAll(json);
    } else {
        ws.textAll("{\"error\":\"Invalid command or missing dir/data\"}");
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  Serial.println("Initializing WebSocket ...");
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);

  // WifiConect("I-Soft", "i-soft@2023"); // Replace with your WiFi credentials
  Serial.println("Connecting to WiFi...");
  String ssid = "I-Soft"; // Replace with your WiFi SSID
  String password = "i-soft@2023"; // Replace with your WiFi password
  Serial.println("IP local: " + WifiConect(ssid, password)); // Connect to WiFi and print local IP address

  initWebSocket();
  Serial.println("Starting Webserver ...");
  server.begin();

  setRole(role); // Set the role of the device (Master or Slave)
  Serial.println("Modbus IP Remote Address: " + remote.toString()); // Print remote address
  Serial.println("Modbus IP Role: " + String(role ? "Master" : "Slave")); // Print Modbus role
  Serial.println("Modbus IP Holding Register Start Address: " + String(REG)); // Print Holding Register start address
  Serial.println("Modbus IP Holding Register Write Address: " + String(REG2)); // Print Holding Register write address
}//setup

uint16_t res[10];
uint8_t show = LOOP_COUNT;

void loop() {
  ws.cleanupClients();
  SerialProccess(); // Process Serial input for JSON commands

  if (WiFi.status() != WL_CONNECTED) { // Check WiFi connection
    Serial.println("WiFi disconnected, trying to reconnect...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  static uint32_t lastMillis = 0;
  if (millis() - lastMillis > 1000) { // Check every second
    lastMillis = millis();
    Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());
    ws.textAll("ToiDangHoatDong"); // Print local IP address
  }


  // // Gửi dữ liệu Serial lên WebSocket nếu có dữ liệu mới

  //   String serialData = Serial.readStringUntil('\n');
  //   String json = "{\"serial\":\"" + serialData + "\"}";
  //   ws.textAll(json);
  //   Serial.println("Serial.available: " + json); // Print sent data

  

  if (role) { // Check if the role is master 
    if (mb.isConnected(remote)) {   // Check if connection to Modbus Slave is established
      mb.readHreg(remote, REG, res, 10);  // Đọc 10 thanh ghi (Holding Register) bắt đầu từ địa chỉ REG trên thiết bị slave, lưu kết quả vào mảng res.
      // Serial.println("Connect successfully. Reading Holding Registers from Slave...");
    } else {
      mb.connect(remote);           // Try to connect if no connection
    }

  mb.task();                      // Common local Modbus task
  delay(100);                     // Pulling interval

  if (!show--) {                   // Display Slave register value one time per second (with default settings)
    for (uint8_t i = 0; i < 10; i++) {
      Serial.print("res[");
      Serial.print(i);
      Serial.print("] = ");
      Serial.println(res[i]);
    }
    for (int i = 0; i < 10; i++) {
    mb.writeHreg(remote, REG2 + i, writeValues[i]); // Gửi giá trị i*10 tới thanh ghi REG+i trên slave
    }
    show = LOOP_COUNT;
  }
  
  // for (int i = 0; i < 10; i++) {
  //   mb.Hreg(REG2 + i, i * 10); // Update Holding Registers with new values
  // }

} else { // If the role is slave
    mb.task(); // Run Modbus Server task
    delay(100); // Pulling interval

  }
}