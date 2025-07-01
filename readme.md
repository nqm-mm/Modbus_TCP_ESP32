# Modbus TCP on ESP32 (PlatformIO)

## 1. Cấu hình file platformio.ini

```ini
[env:motorgo_mini_1]
platform = espressif32
board = motorgo_mini_1
board_build.mcu = esp32s3
framework = arduino
build_flags = -DSERIAL_PORT_HARDWARE=Serial1 -DCUSTOM_RS485_DEFAULT_DE_PIN=21 -DCUSTOM_RS485_DEFAULT_RE_PIN=22
lib_deps = 
    emelianov/modbus-esp8266@^4.1.0
    arduino-libraries/ArduinoModbus@^1.0.9
    esphome/ESPAsyncWebServer-esphome@^3.3.0
monitor_speed = 115200
```

- **Lưu ý:**  
  - `build_flags` đã được cấu hình để sử dụng UART1 cho RS485.
  - Các thư viện cần thiết đã được khai báo trong `lib_deps`.

## 2. Các mảng và biến cần khởi tạo

Khai báo các mảng và biến sau để quản lý địa chỉ và giá trị thanh ghi:

```cpp
#define MAX_REG_ADDR 20
int AddrReadList[MAX_REG_ADDR];         // Mảng lưu địa chỉ các thanh ghi cần đọc/ghi
uint16_t ValueReadList[MAX_REG_ADDR];   // Mảng lưu giá trị tương ứng với AddrReadList
int regReadCount = 0;                   // Số lượng thanh ghi đã đăng ký
bool role = false;                      // Vai trò: true = master, false = slave
IPAddress remote(192,168,3,161);        // Địa chỉ IP của thiết bị Modbus Slave
```

## 3. Chức năng và cách hoạt động của từng hàm

### **gc**

```cpp
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
```

- Callback khi có lệnh ghi vào thanh ghi (chỉ dùng cho slave).
- Cập nhật giá trị mới vào `ValueReadList` tại vị trí tương ứng với địa chỉ trong `AddrReadList`.

### **WifiConect**

```cpp
String WifiConect(String ssid, String password) {
  WiFi.begin(ssid.c_str(), password.c_str()); // Connect to WiFi network
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { // Wait for connection
    delay(500);
    Serial.print(".");
  }
  return WiFi.localIP().toString(); // Return local IP address
}
```

- Kết nối WiFi với SSID và password truyền vào.
- Trả về địa chỉ IP của ESP32.

### **handleAddrInput**

```cpp
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
```

- Nhận một mảng JSON địa chỉ, lưu vào `AddrReadList` và cập nhật `regReadCount`.
- Nếu là slave, tự động đăng ký các thanh ghi này với Modbus (`mb.addHreg` và callback).

### **setRole**

```cpp
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
```

- Thiết lập chế độ hoạt động: master (client) hoặc slave (server).
- Nếu là slave, khởi tạo server Modbus trên port 502.
- Nếu là master, chuyển sang chế độ client.

### **SerialProccess**

```cpp
void SerialProccess() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); // Read input until newline
    JSONVar json = JSON.parse(input); // Parse JSON input
    Serial.print("Received JSON: ");
    Serial.println(input); // Print received JSON input

    if(JSON.typeof(json) == "undefined") {
      Serial.println("Parsing JSON failed!");
    } else if (json.hasOwnProperty("setValue")) {
    //SET VALUE
    
    } else if (json.hasOwnProperty("address")) {
    //ADDRESS

    } else if (json.hasOwnProperty("remote")) {
    //REMOTE

    } else if (json.hasOwnProperty("role")) {
    //ROLE

    } else if(json.hasOwnProperty("wificonfig")) {
    //WIFICONFIG

  }
}
```

- Đọc lệnh JSON từ Serial, phân tích và thực thi:
  - `setValue`: Ghi giá trị vào thanh ghi chỉ định.
  - `address`: Cập nhật danh sách địa chỉ thanh ghi cần đọc/ghi.
  - `remote`: Đổi địa chỉ IP của thiết bị slave.
  - `role`: Đổi vai trò master/slave.
  - `wificonfig`: Đổi cấu hình WiFi.

### **readRegisters**

```cpp
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
```

- Nếu là master: Duyệt qua `AddrReadList`, đọc từng thanh ghi từ slave và lưu vào `ValueReadList`.
- Nếu là slave: Duyệt qua `AddrReadList`, in giá trị hiện tại của từng thanh ghi local ra Serial.

### **loop**

```cpp
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
```

- Gọi `SerialProccess()` để xử lý lệnh từ Serial.
- Kiểm tra kết nối WiFi, tự động reconnect nếu mất kết nối.
- Nếu là master, đảm bảo luôn kết nối tới slave.
- Gọi `mb.task()` để xử lý giao tiếp Modbus.
- Định kỳ (mỗi 1 giây) gọi `readRegisters()` để đọc/in giá trị các thanh ghi.

## 4. Giao tiếp qua Serial Monitor bằng lệnh JSON

Có thể cấu hình và điều khiển thiết bị qua Serial Monitor bằng các lệnh JSON như sau (tham khảo file NOTE.json):

- **Đổi vai trò master/slave:**

    ```json
    {"role":1}   // 1: master, 0: slave
    ```

- **Đăng ký địa chỉ thanh ghi:**

    ```json
    { "address": [100, 101, [105,109]] }
    ```

- Đăng ký các thanh ghi 100, 101, 105, 106, 107, 108, 109.

- **Ghi giá trị vào thanh ghi:**

    ```json
    {"setValue":{"address":110,"value":123}}
    ```

- Ghi giá trị 123 vào thanh ghi 110.

- **Đổi địa chỉ IP của slave:**

    ```json
    {"remote":[192,168,3,161]}
    ```

- **Cấu hình WiFi:**

    ```json
    {"wificonfig":{"ssid":"yourSSID", "password":"yourPassword"}}
    ```

**Lưu ý:**  

- Gửi từng lệnh một, kết thúc bằng Enter (newline), theo thứ tự.

---
