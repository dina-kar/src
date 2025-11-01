# ESP32 to Tang Nano 9K FPGA UART Communication

This project implements simple UART communication between an ESP32 and Tang Nano 9K FPGA. The FPGA echoes back any data it receives from the ESP32.

## Hardware Requirements

- **Tang Nano 9K FPGA Board**
- **ESP32 Development Board** (any variant)
- **Jumper Wires** (3 wires minimum)

## Wiring Connections

### Pin Connections

| ESP32 Pin | Function | FPGA Pin | FPGA Physical Pin |
|-----------|----------|----------|-------------------|
| GPIO 17   | TX       | RX       | Pin 18           |
| GPIO 16   | RX       | TX       | Pin 17           |
| GND       | Ground   | GND      | Any GND pin      |

### Connection Diagram
```
ESP32                    Tang Nano 9K
-----                    ------------
GPIO17 (TX) -----------> Pin 18 (RX)
GPIO16 (RX) <----------- Pin 17 (TX)
GND         <----------> GND
```

**Important Notes:**
- Both ESP32 and Tang Nano 9K operate at 3.3V logic levels, so they are directly compatible
- Do NOT connect 5V signals to the FPGA
- Make sure GND is connected between both devices

## FPGA Configuration

### UART Parameters
- **Clock Frequency:** 27 MHz (Tang Nano 9K built-in oscillator)
- **Baud Rate:** 115200
- **Data Format:** 8 bits, No parity, 1 stop bit (8N1)

### Files Description

1. **uart_top.v** - Top module with simple echo logic
2. **uart_rx.v** - UART receiver module
3. **uart_tx.v** - UART transmitter module
4. **top.cst** - Pin constraints file

### Behavior
The FPGA firmware implements a simple echo/loopback:
- Receives data from ESP32 via UART RX
- Immediately echoes back the received data via UART TX
- No processing or modification of data

## ESP32 Software

### Setup Requirements

1. **Arduino IDE** with ESP32 board support installed
   - Install via Board Manager: "ESP32 by Espressif Systems"

2. **Upload the Program**
   - Open `esp32_uart_test.ino` in Arduino IDE
   - Select your ESP32 board (e.g., "ESP32 Dev Module")
   - Select the correct COM port
   - Upload the sketch

### ESP32 Program Features

The ESP32 program:
- Sends test messages to FPGA every 2 seconds
- Displays received echo data on Serial Monitor
- Accepts custom messages via Serial Monitor
- Shows data in both ASCII and HEX format for debugging

### Using the Serial Monitor

1. Open Serial Monitor (Tools -> Serial Monitor)
2. Set baud rate to **115200**
3. You should see:
   - Automatic test messages being sent
   - Echo responses from FPGA
   - Hex values for debugging

4. **Send Custom Messages:**
   - Type any text in Serial Monitor input box
   - Press Enter or click Send
   - The ESP32 will forward it to FPGA
   - You'll see the echoed response

## Testing Procedure

### 1. Program the FPGA
```bash
# Using Gowin IDE:
# 1. Open the project with uart_top.v as top module
# 2. Synthesize and Place & Route
# 3. Program the FPGA
```

### 2. Upload ESP32 Code
```bash
# Using Arduino IDE or PlatformIO
# Upload esp32_uart_test.ino to your ESP32
```

### 3. Make Connections
- Connect the three wires as shown in the wiring diagram
- Power both devices (ESP32 via USB, FPGA via USB)

### 4. Open Serial Monitor
- Set baud rate to 115200
- You should see messages like:
```
=================================
ESP32 <-> FPGA UART Test
=================================
FPGA UART initialized
Baud Rate: 115200
Format: 8N1
=================================

Sending to FPGA: MSG#1
Waiting for echo...
Received from FPGA: MSG#1
 [HEX: 4D 53 47 23 31 0D 0A ]
```

## Troubleshooting

### No Echo Received

1. **Check Wiring:**
   - ESP32 TX must connect to FPGA RX
   - ESP32 RX must connect to FPGA TX
   - GND must be connected

2. **Check FPGA Programming:**
   - Verify FPGA is programmed correctly
   - Check that rst_n is not held low

3. **Check Baud Rate:**
   - Both devices must use 115200 baud
   - FPGA clock must be 27 MHz

### Garbage Data Received

- Usually indicates baud rate mismatch
- Verify both sides are using 115200 baud
- Check FPGA clock frequency parameter

### No Response at All

- Check power to both devices
- Verify GND connection
- Use a logic analyzer or oscilloscope to check TX/RX signals
- Verify ESP32 GPIO pins are correct (16, 17)

## Modifying the Design

### Change Baud Rate

**FPGA (uart_top.v):**
```verilog
parameter UART_FRE = 9600;  // Change to desired baud rate
```

**ESP32 (esp32_uart_test.ino):**
```cpp
#define BAUD_RATE 9600  // Must match FPGA
```

### Change ESP32 UART Pins

```cpp
#define FPGA_RX_PIN 16  // Change to your desired RX pin
#define FPGA_TX_PIN 17  // Change to your desired TX pin
```

### Add Data Processing

Instead of simple echo, you can modify `uart_top.v` to:
- Process received data
- Perform calculations
- Control other peripherals
- Implement a command protocol

## Example Applications

1. **Sensor Data Collection:**
   - ESP32 reads sensors (WiFi/Bluetooth/I2C/SPI)
   - Sends data to FPGA for processing
   - FPGA performs high-speed processing

2. **Remote Control:**
   - ESP32 connects to WiFi/Bluetooth
   - Receives commands from network
   - Forwards to FPGA for hardware control

3. **Data Logger:**
   - FPGA captures high-speed data
   - Sends to ESP32 via UART
   - ESP32 logs to SD card or cloud

## Advanced Features (Future Enhancements)

- [ ] Implement packet protocol with checksums
- [ ] Add flow control (RTS/CTS)
- [ ] Increase baud rate for faster communication
- [ ] Add buffering for large data transfers
- [ ] Implement command/response protocol
- [ ] Add error detection and retry logic

## License

This project is provided as-is for educational and development purposes.

## Support

For issues or questions:
- Check the Tang Nano 9K documentation
- Review ESP32 UART examples
- Test with a USB-UART adapter first to isolate issues

