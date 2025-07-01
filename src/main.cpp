/* Modbus TCP and WebSocket on ESP32
 * Chương trình mẫu Modbus IP trên ESP8266/ESP32
 * Chương trình này sử dụng thư viện ModbusIP_ESP8266 để giao tiếp với thiết bị Modbus Slave.
 * Nó có thể hoạt động ở chế độ Master hoặc Slave, tùy thuộc vào giá trị của biến 'role'.
 * Chương trình này có thể được sử dụng để giám sát và điều khiển các thiết bị Modbus từ xa
 */

//slave là server, master là client
//1 thanh ghi là 2 byte = 1 word
//2 thanh ghi là 4 byte = 1 dword
// //2 thanh ghi là 4 byte = 1 float
// #define RS485_DEFAULT_DE_PIN 21
// #define RS485_DEFAULT_RE_PIN 22
#include <ArduinoRS485.h>
#ifdef ESP8266
 #include <ESP8266WiFi.h>
#else
 #include <WiFi.h>
#endif
#include <ModbusIP_ESP8266.h>
#include <Arduino_JSON.h>
#include <ESPAsyncWebServer.h>

#define MAX_REG_ADDR 20
int AddrReadList[MAX_REG_ADDR];
uint16_t ValueReadList[MAX_REG_ADDR];
int regReadCount = 0;

IPAddress remote(192,168,3,161);  // Address of Modbus Slave device
const int LOOP_COUNT = 10;

ModbusIP mb;  //ModbusIP object

bool role = false; // Register value to read
// true = master, false = slave

uint16_t gc(TRegister* r, uint16_t v) { // Callback function
  if (r->value != v) {  // Check if Coil state is going to be changed
    Serial.print("Regis address: ");
    Serial.println(r->address.address);
    Serial.print("Set reg: ");
    Serial.println(v);

    // Cập nhật ValueReadList tại vị trí tương ứng
    for (int i = 0; i < regReadCount; i++) {
      if (AddrReadList[i] == r->address.address) {
        ValueReadList[i] = v;
        break;
      }
    }
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

void handleAddrInput(JSONVar addressArray, int* addrList, int* addrCount) {
  if (JSON.typeof(addressArray) == "array") {
    int count = 0;
    for (int i = 0; i < addressArray.length(); i++) {
      if (JSON.typeof(addressArray[i]) == "number") {
        int addr = (int)addressArray[i];
        if (addrList && count < MAX_REG_ADDR) addrList[count++] = addr;
        Serial.print("Register single address: ");
        Serial.println(addr);
        if (!role) {
          mb.addHreg(addr, 0);
          mb.onSetHreg(addr, gc);
        }
      } else if (JSON.typeof(addressArray[i]) == "array" && addressArray[i].length() == 2) {
        int start = (int)addressArray[i][0];
        int end = (int)addressArray[i][1];
        Serial.print("Register address range: ");
        Serial.print(start);
        Serial.print(" to ");
        Serial.println(end);
        for (int addr = start; addr <= end && count < MAX_REG_ADDR; addr++) {
          if (addrList) addrList[count++] = addr;
          if (!role) {
            mb.addHreg(addr, 0);
            mb.onSetHreg(addr, gc);
          }
        }
      }
    }
    if (addrCount) *addrCount = count;
  }
}


void setRole(bool isMaster) {
  if(isMaster) {//master
    mb.client();
    if (mb.isConnected(remote)) {   // Check if connection to Modbus Slave is established
      Serial.println("Connect successfully. Reading Holding Registers from Slave...");
    }
  } else {//slave
    mb.server(502); // Start Modbus Server on port 502
    Serial.println("Modbus Server started");
  }
}

void SerialProccess() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); // Read input until newline
    JSONVar json = JSON.parse(input); // Parse JSON input
    Serial.print("Received JSON: ");
    Serial.println(input); // Print received JSON input

    if(JSON.typeof(json) == "undefined") {
      Serial.println("Parsing JSON failed!");

    //SET VALUE
    } else if (json.hasOwnProperty("setValue")) {
      Serial.println("Processing setValue command");
      JSONVar setValue = json["setValue"];
      int Addr;
      if (setValue.hasOwnProperty("address")) {
        Addr = (int)setValue["address"]; // Set REG to the address specified in JSON
        Serial.print("Set address: ");
        Serial.println(Addr);
      }
      if (setValue.hasOwnProperty("value")) {
        int value = (int)setValue["value"]; // Set value to the value specified in JSON
        Serial.print("Set value: ");
        Serial.println(value);
        if (role) { // If the role is master
          mb.writeHreg(remote, Addr, value); // Write value to Holding Register at REG address
          Serial.printf("Write to remote %s: [%d] = %d\n", remote.toString().c_str(), Addr, value);
        } else { // If the role is slave
          mb.Hreg(Addr, value); // Write value to Holding Register at REG address
        }
      }
    
    //ADDRESS
    } else if (json.hasOwnProperty("address")) {
      JSONVar addressArray = json["address"];
        // Xử lý địa chỉ đọc
          handleAddrInput(addressArray, AddrReadList, &regReadCount);
          Serial.print("Set read addresses: ");
          for (int i = 0; i < regReadCount; i++) {
            Serial.print(AddrReadList[i]);
            Serial.print(" ");
          }
          Serial.println();
      

    //REMOTE
    } else if (json.hasOwnProperty("remote")) {
      remote = IPAddress(
        (int)json["remote"][0],
        (int)json["remote"][1],
        (int)json["remote"][2],
        (int)json["remote"][3]
    );
      mb.connect(remote);

    //ROLE
    } else if (json.hasOwnProperty("role")) {
      role = (int)json["role"];
      Serial.print("Set role: ");
      Serial.println(role ? "Master" : "Slave");
      setRole(role); // Call setRole function to update Modbus role

    //WIFICONFIG
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
  }
}

void readRegisters() {
  if (role) { // If the role is master
    for (int i = 0; i < regReadCount; i++) {
        int addr = AddrReadList[i];
        mb.readHreg(remote, addr, &ValueReadList[i], 1); // Đọc giá trị từ slave
        Serial.printf("[%d]: %d", addr, ValueReadList[i]);
    }
    Serial.println();
  } else {
    for (int i = 0; i < regReadCount; i++) {
      int addr = AddrReadList[i];
      Serial.printf("[%d]: ", addr);
      Serial.print(mb.Hreg(addr));
    }
    Serial.println();
  }
}


void setup() {
  Serial.begin(115200);
  delay(5000); // Wait for Serial to initialize
  // WifiConect("I-Soft", "i-soft@2023"); // Replace with your WiFi credentials
  Serial.println("Connecting to WiFi...");
  String ssid = "I-Soft"; // Replace with your WiFi SSID
  String password = "i-soft@2023"; // Replace with your WiFi password
  Serial.println("IP local: " + WifiConect(ssid, password)); // Connect to WiFi and print local IP address

  setRole(role); // Set the role of the device (Master or Slave)
  Serial.println("Modbus IP Remote Address: " + remote.toString()); // Print remote address
  Serial.println("Modbus IP Role: " + String(role ? "Master" : "Slave")); // Print Modbus role
}//setup

void loop() {
  SerialProccess(); // Process Serial input for JSON commands

  if (WiFi.status() != WL_CONNECTED) { // Check WiFi connection
    Serial.println("WiFi disconnected, trying to reconnect...");
    WiFi.reconnect();
    delay(1000);
    return;
  }
  if (role) { // MASTER
    while (!mb.isConnected(remote)) {
      mb.connect(remote);
      Serial.println("Trying to connect...");
      delay(1000);
    }
  }

  mb.task(); // Run Modbus task (client or server)
  delay(100); // Polling interval
  
  static uint32_t lastTime = 0;
  if (millis() - lastTime > 1000) {
    lastTime = millis();
    readRegisters();
  }
}
