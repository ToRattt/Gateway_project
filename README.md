# 📡 4G Gateway Project (ESP32 + A7680C + ThingsBoard)

## 🧠 Tổng quan

Project này xây dựng một **Gateway IoT sử dụng ESP32 + module 4G A7680C** để:

* Thu thập dữ liệu (GPS / sensor / BLE)
* Gửi dữ liệu lên cloud (ThingsBoard) qua **MQTT**
* Nhận lệnh điều khiển từ cloud (RPC)
* Điều khiển thiết bị ngoại vi (BLE / GPIO)

---

## 🏗️ Cấu trúc thư mục

```
Gateway_project/
├── components/
├── main/
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

---

## 📁 Chi tiết các folder

### 🔹 `main/`

**Vai trò:** Entry point của chương trình

* Chứa `main.c`
* Không xử lý logic phức tạp
* Chỉ:

  * Khởi tạo hệ thống
  * Gọi `network_mgr_run()` trong vòng lặp

👉 Nguyên tắc:

> main.c phải đơn giản, chỉ đóng vai trò điều phối

---

### 🔹 `components/`

Chứa toàn bộ logic chính, chia theo từng module độc lập

---

## 📦 Các module trong `components/`

---

### 📡 `sim_a7680c/`

**Chức năng:** Điều khiển module 4G

* Gửi AT command
* Kiểm tra SIM
* Kết nối mạng 4G (APN)
* Kích hoạt PDP context

---

### ☁️ `mqtt_tb/`

**Chức năng:** Giao tiếp với ThingsBoard qua MQTT

* Kết nối MQTT broker
* Publish telemetry
* Subscribe RPC

---

### 🌐 `network_mgr/`

**Chức năng:** Điều phối toàn hệ thống (QUAN TRỌNG NHẤT)

* Quản lý state machine:

  * INIT → SIM → NETWORK → MQTT → RUNNING
* Retry khi lỗi:

  * Mất mạng
  * Mất MQTT
* Điều phối các module khác

---

### 📍 `gps/`

**Chức năng:** Đọc dữ liệu GPS

* Lấy:

  * latitude
  * longitude
  * timestamp
* Kiểm tra GPS hợp lệ

---

### 🧾 `json_builder/`

**Chức năng:** Tạo dữ liệu JSON gửi lên cloud

Ví dụ:

```json
{"lat":10.123,"lon":106.123,"ts":123456789}
```

---

### 🔧 `rpc_handler/`

**Chức năng:** Xử lý lệnh từ cloud (RPC)

* Nhận JSON từ MQTT
* Parse command
* Điều khiển:

  * GPIO
  * BLE
  * hoặc logic khác

---

### 📶 `ble_gateway/` *(optional)*

**Chức năng:** Giao tiếp BLE

* Nhận dữ liệu từ sensor BLE
* Gửi lệnh xuống thiết bị BLE

---

### 🖥️ `uart_debug/`

**Chức năng:** Debug hệ thống

* In log ra Serial
* Hiển thị:

  * AT command
  * phản hồi từ SIM
  * trạng thái hệ thống

---

### ⚙️ `app_config/`

**Chức năng:** Cấu hình hệ thống

Chứa các thông tin:

* APN
* MQTT server
* Device token
* UART pin
* Chu kỳ gửi dữ liệu

---

## 🔄 Luồng hoạt động hệ thống

### 📤 Device → Cloud

1. ESP32 khởi tạo SIM
2. Kết nối mạng 4G
3. Kết nối MQTT
4. Đọc dữ liệu (GPS / sensor)
5. Tạo JSON
6. Gửi lên ThingsBoard

---

### 📥 Cloud → Device (RPC)

1. User thao tác trên dashboard
2. ThingsBoard gửi RPC qua MQTT
3. ESP32 nhận lệnh
4. `rpc_handler` xử lý
5. Điều khiển thiết bị
6. Gửi trạng thái lại cloud

---

## ⚠️ Nguyên tắc thiết kế

* ❌ Không viết logic phức tạp trong `main.c`
* ✔️ Mỗi module 1 chức năng riêng
* ✔️ Dễ mở rộng, dễ debug
* ✔️ Tách biệt rõ:

  * Network
  * Device control
  * Data processing

---

## 🚀 Ghi chú

* Giao thức chính: **MQTT**
* Cloud: **ThingsBoard**
* Hardware:

  * ESP32
  * A7680C (4G)
  * (Optional) BLE devices

---

## 📌 Mục tiêu

* Xây dựng gateway IoT ổn định
* Có khả năng:

  * telemetry realtime
  * remote control (RPC)
  * mở rộng dễ dàng

---

## 🧠 Ghi nhớ nhanh

| Module       | Vai trò        |
| ------------ | -------------- |
| main         | điều phối      |
| network_mgr  | bộ não         |
| sim_a7680c   | kết nối 4G     |
| mqtt_tb      | gửi/nhận cloud |
| rpc_handler  | xử lý lệnh     |
| gps          | dữ liệu        |
| json_builder | đóng gói       |
| uart_debug   | debug          |

---

## 📬 Liên hệ / Ghi chú thêm

(ghi thêm khi cần)
