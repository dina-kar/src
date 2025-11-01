# ESP32 to FPGA ChaCha20 Hardware Encryption

This project implements **ChaCha20 stream cipher encryption** on the Tang Nano 9K FPGA as a hardware accelerator, with ESP32 handling communication and verification.

## 🔐 Overview

The system demonstrates hardware-accelerated encryption:
- **ESP32**: Sends 64-byte plaintext blocks via UART
- **FPGA**: Encrypts using ChaCha20 hardware core
- **ESP32**: Receives ciphertext and verifies against software implementation

### Why ChaCha20?

ChaCha20 is a modern, fast, and secure stream cipher:
- Designed by Daniel J. Bernstein (djb)
- Used in TLS 1.3, WireGuard VPN, and Signal
- More secure than RC4, faster than AES on software
- Excellent for hardware implementation

## 📁 Project Structure

```
uart/src/
├── M/
│   ├── main.v                          # Top module with ChaCha20 integration
│   └── chacha-master/src/rtl/
│       ├── chacha_core.v               # ChaCha20 core (Secworks implementation)
│       └── chacha_qr.v                 # Quarter-round function
├── uart_rx.v                           # UART receiver
├── uart_tx.v                           # UART transmitter
├── top.cst                             # Pin constraints
├── esp32_chacha20_test.ino             # ESP32 test program with software ChaCha20
└── README_CHACHA20.md                  # This file
```

## 🔌 Hardware Setup

### Required Components
- **Tang Nano 9K FPGA** (GW1NR-9C)
- **ESP32 Development Board** (any variant)
- **3 Jumper Wires**

### Wiring Connections

| ESP32 Pin | Direction | FPGA Pin | Physical Pin |
|-----------|-----------|----------|--------------|
| GPIO 17   | TX →      | RX       | Pin 18       |
| GPIO 16   | ← RX      | TX       | Pin 17       |
| GND       | ⇄         | GND      | Any GND      |

```
ESP32                    Tang Nano 9K
-----                    ------------
GPIO17 (TX) ----→----→  Pin 18 (RX)
GPIO16 (RX) ←----←----  Pin 17 (TX)
GND         ←----------→ GND
```

**Note**: Both devices use 3.3V logic levels ✓

## ⚙️ FPGA Implementation Details

### Architecture

The FPGA design (`M/main.v`) implements a complete ChaCha20 encryption pipeline:

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  UART RX    │────→│  64-byte     │────→│  ChaCha20   │
│  (Serial)   │     │  Buffer      │     │  Core       │
└─────────────┘     └──────────────┘     └─────────────┘
                                                 │
                                                 ↓ (Keystream)
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│  UART TX    │←────│  XOR with    │←────│  64-byte    │
│  (Serial)   │     │  Plaintext   │     │  Keystream  │
└─────────────┘     └──────────────┘     └─────────────┘
```

### State Machine

```
S_RX_WAIT    → Wait for 64 bytes from UART
     ↓
S_CORE_INIT  → Initialize ChaCha20 core with counter
     ↓
S_CORE_WAIT  → Wait for keystream generation
     ↓
S_LOAD_KS    → Load 512-bit keystream into buffer
     ↓
S_PRE_TX     → Prepare first encrypted byte
     ↓
S_TX_SEND    → Transmit all 64 encrypted bytes
     ↓
(back to S_RX_WAIT with incremented counter)
```

### ChaCha20 Configuration

```verilog
parameter CLK_FRE   = 27;          // 27 MHz clock
parameter UART_BAUD = 115200;      // Baud rate
parameter CHACHA_ROUNDS = 20;      // ChaCha20 (20 rounds)
parameter CHACHA_KEYLEN = 1;       // 256-bit key

// For testing: all-zero key and IV
localparam [255:0] CHACHA_KEY = 256'h0;
localparam [63:0]  CHACHA_IV  = 64'h0;
```

**⚠️ Security Note**: The all-zero key is for **TESTING ONLY**. In production, use a proper cryptographic key!

### Block Operation

1. **Receive**: Accumulate 64 bytes of plaintext
2. **Encrypt**: Generate keystream using ChaCha20(key, IV, counter)
3. **XOR**: Ciphertext = Plaintext ⊕ Keystream
4. **Transmit**: Send 64 bytes of ciphertext
5. **Increment**: Counter += 1 for next block

## 💻 ESP32 Software

### Features

The ESP32 program (`esp32_chacha20_test.ino`):

✅ **Automatic Testing**
- Sends 64-byte test blocks every 3 seconds
- Incremental test patterns for easy debugging

✅ **Software ChaCha20 Implementation**
- Full ChaCha20 in software for verification
- Compares FPGA output with expected results

✅ **Verification & Reporting**
- Byte-by-byte comparison
- Detailed mismatch reporting
- Success/failure statistics

✅ **Manual Control**
- Interactive commands via Serial Monitor
- Custom test triggering
- Counter reset

### Setup Instructions

1. **Install Arduino IDE** with ESP32 support
   ```
   Boards Manager → "ESP32 by Espressif Systems"
   ```

2. **Open** `esp32_chacha20_test.ino`

3. **Select Board**: "ESP32 Dev Module" (or your board)

4. **Select Port**: Your ESP32's COM/USB port

5. **Upload** the sketch

6. **Open Serial Monitor** at 115200 baud

### Expected Output

```
========================================
ESP32 ChaCha20 Encryption Test
FPGA Hardware Accelerator Verification
========================================
FPGA UART initialized (115200 baud, 8N1)
========================================

╔════════════════════════════════════════╗
║  Test Block #1                         ║
╚════════════════════════════════════════╝

[1] Plaintext (64 bytes):
  0000: 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 
  0010: 10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1F 
  ...

[2] Expected Ciphertext (Software ChaCha20):
  0000: 76 B8 E0 AD A0 F1 3D 90 40 5D 6A E5 53 86 BD 28 
  ...

[3] Sending to FPGA...
✓ 64 bytes sent

[4] Waiting for FPGA response...

[5] Received from FPGA:
  0000: 76 B8 E0 AD A0 F1 3D 90 40 5D 6A E5 53 86 BD 28 
  ...

[6] Verification:
✓ PERFECT MATCH!
  FPGA encryption matches software implementation.

========================================
```

### Serial Commands

| Command | Description |
|---------|-------------|
| `test` or `t` | Send test block immediately |
| `reset` or `r` | Reset block counter to 0 |
| `help` or `h` | Show command help |

## 🚀 Getting Started

### Step 1: Program the FPGA

1. Open **Gowin FPGA Designer**
2. Create/open project with `M/main.v` as top module
3. Add all required files:
   - `M/main.v`
   - `M/chacha-master/src/rtl/chacha_core.v`
   - `M/chacha-master/src/rtl/chacha_qr.v`
   - `uart_rx.v`
   - `uart_tx.v`
4. Set constraints file: `top.cst`
5. **Synthesize** → **Place & Route** → **Program Device**

### Step 2: Upload ESP32 Code

1. Open `esp32_chacha20_test.ino` in Arduino IDE
2. Select ESP32 board and port
3. Upload

### Step 3: Connect Hardware

Wire the three connections (TX, RX, GND) as shown above

### Step 4: Test

1. Open Serial Monitor (115200 baud)
2. Watch automatic tests run every 3 seconds
3. Verify "✓ PERFECT MATCH!" messages

## 🔧 Troubleshooting

### No Data Received from FPGA

**Symptoms**: ESP32 sends data but never receives response

**Solutions**:
1. ✓ Check wiring (TX→RX crossover, not TX→TX)
2. ✓ Verify FPGA is programmed and running
3. ✓ Check GND connection
4. ✓ Ensure both use 115200 baud
5. ✓ Verify FPGA reset (rst_n) is not held low

### Mismatched Encryption Output

**Symptoms**: Data received but doesn't match expected ciphertext

**Possible Causes**:
1. **Byte order mismatch**: ChaCha20 uses little-endian
2. **Counter mismatch**: Check initial counter value
3. **Timing issue**: UART data corruption
4. **Core not ready**: FPGA may not be waiting for core_ready

**Debug Steps**:
```
1. Verify first block (counter=0) matches test vectors
2. Check keystream extraction in S_LOAD_KS state
3. Verify XOR operation in S_TX_SEND state
4. Use logic analyzer on UART pins
```

### FPGA Synthesis Errors

**Common Issues**:
- Missing ChaCha core files → Add all .v files to project
- Path errors → Use relative paths from project root
- Timing violations → Check clock constraints (27 MHz)

### ESP32 Compilation Errors

- **"HardwareSerial"** → Ensure ESP32 board selected, not Arduino
- **Undefined ChaCha functions** → Check all functions are in .ino file

## 🔒 Security Considerations

### ⚠️ THIS IS A DEMONSTRATION PROJECT

**Current Configuration (NOT SECURE for production)**:
- ✗ All-zero encryption key
- ✗ All-zero nonce/IV
- ✗ No authentication (MAC/AEAD)
- ✗ No key exchange protocol
- ✗ Predictable counter initialization

### For Production Use:

1. **Use Real Keys**
   ```verilog
   // Generate with cryptographically secure RNG
   localparam [255:0] CHACHA_KEY = 256'h<your_256_bit_key>;
   ```

2. **Use Random Nonce**
   ```verilog
   localparam [63:0] CHACHA_IV = 64'h<your_random_nonce>;
   ```

3. **Add Authentication**
   - Use ChaCha20-Poly1305 (AEAD)
   - Prevents tampering and forgery

4. **Secure Key Storage**
   - Store keys in FPGA eFUSE or secure storage
   - Never hardcode in source code

5. **Key Exchange**
   - Implement key agreement (e.g., ECDH)
   - Don't transmit keys in plaintext

## 📊 Performance

### Throughput

- **UART Bottleneck**: 115200 baud ≈ 11.5 KB/s
- **Block Size**: 64 bytes
- **Latency**: ~55ms per block (UART + ChaCha computation)

### Improvements

To increase throughput:
1. **Higher Baud Rate**: 921600 (8× faster) or 2000000
2. **SPI Interface**: 10+ Mbps possible
3. **Parallel Processing**: Pipeline multiple blocks
4. **Optimized Core**: Reduce ChaCha rounds (ChaCha12, ChaCha8)

## 📚 ChaCha20 Test Vectors

You can verify correctness using RFC 8439 test vectors:

**Test Vector 1** (from RFC 8439 Section 2.3.2):
```
Key:      000102...1e1f (32 bytes)
Nonce:    000000000000004a00000000
Counter:  1
Block 1:  224f51f3401bd9e12fde276fb8631ded...
```

## 🛠️ Customization

### Change Encryption Key

Edit `M/main.v`:
```verilog
// Replace with your 256-bit key
localparam [255:0] CHACHA_KEY = 256'h0123456789ABCDEF...;
```

### Change Nonce/IV

```verilog
// Replace with your 64-bit nonce
localparam [63:0] CHACHA_IV = 64'h123456789ABCDEF0;
```

### Use ChaCha12 or ChaCha8

```verilog
// Faster but slightly less secure
localparam [4:0] CHACHA_ROUNDS = 5'd12;  // ChaCha12
// or
localparam [4:0] CHACHA_ROUNDS = 5'd8;   // ChaCha8
```

### Increase Block Size

ChaCha20 core produces 64-byte blocks. To process larger data:
- ESP32 splits data into 64-byte chunks
- FPGA processes each chunk with incrementing counter
- Works automatically with current design!

## 📖 References

1. **ChaCha20 Specification**
   - RFC 8439: https://tools.ietf.org/html/rfc8439
   - Original paper: https://cr.yp.to/chacha.html

2. **ChaCha Core Implementation**
   - Secworks ChaCha: https://github.com/secworks/chacha
   - Licensed under BSD 2-Clause

3. **FPGA Resources**
   - Tang Nano 9K Wiki: https://wiki.sipeed.com/hardware/en/tang/Tang-Nano-9K/Nano-9K.html
   - Gowin FPGA Tools: https://www.gowinsemi.com/

4. **ESP32 Resources**
   - Arduino-ESP32: https://github.com/espressif/arduino-esp32

## 🤝 Credits

- **ChaCha Algorithm**: Daniel J. Bernstein
- **ChaCha Verilog Core**: Secworks (Joachim Strömbergson)
- **UART Modules**: Tang Nano examples
- **Integration**: This project

## 📄 License

- ChaCha core: BSD 2-Clause (Secworks)
- Integration code: Use freely for educational/commercial purposes

## 🎯 Next Steps

### Beginner Projects
1. ✅ Get basic echo working (see README_ESP32_FPGA.md)
2. ✅ Implement ChaCha20 encryption (this project)
3. → Add Poly1305 authentication
4. → Implement ChaCha20-Poly1305 AEAD

### Advanced Projects
- [ ] AES hardware accelerator
- [ ] RSA/ECC cryptographic processor  
- [ ] Secure bootloader with FPGA
- [ ] Hardware random number generator (TRNG)
- [ ] Side-channel resistant implementation

### Performance Optimization
- [ ] Increase baud rate to 921600
- [ ] Implement DMA for ESP32 UART
- [ ] Pipeline multiple ChaCha blocks
- [ ] Add flow control (RTS/CTS)

## 💡 Applications

This hardware encryption can be used for:

1. **IoT Security**: Encrypt sensor data before WiFi transmission
2. **Secure Logging**: Hardware-encrypted SD card storage
3. **VPN Offload**: Accelerate ChaCha20-Poly1305 for WireGuard
4. **Secure Communication**: ESP32 WiFi/BLE with FPGA encryption
5. **Learning**: Understand hardware crypto implementation

---

**Questions?** Check troubleshooting section or ESP32 Serial Monitor output for detailed diagnostics.

**Found a bug?** The design is provided as-is for educational purposes.

**Happy Encrypting! 🔐**

