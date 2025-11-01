/*
 * ESP32 ChaCha20 Encryption Test with Tang Nano 9K FPGA
 * 
 * This program:
 * 1. Sends 64-byte blocks of plaintext to the FPGA
 * 2. Receives 64-byte blocks of encrypted ciphertext back
 * 3. Implements ChaCha20 in software to verify FPGA encryption
 * 4. Compares FPGA and software results
 * 
 * Hardware Connections:
 * ESP32 TX (GPIO17) -> FPGA RX (Pin 18)
 * ESP32 RX (GPIO16) -> FPGA TX (Pin 17)
 * ESP32 GND         -> FPGA GND
 */

#include <Arduino.h>

// Use UART2 for FPGA communication
HardwareSerial FPGASerial(2);

// UART Configuration
#define FPGA_RX_PIN 16
#define FPGA_TX_PIN 17
#define BAUD_RATE 115200

// ChaCha20 Configuration
#define BLOCK_SIZE 64  // ChaCha20 produces 64-byte blocks

// Test data
uint8_t plaintext[BLOCK_SIZE];
uint8_t ciphertext_fpga[BLOCK_SIZE];
uint8_t ciphertext_sw[BLOCK_SIZE];

// Timing
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 3000;  // Send every 3 seconds
int testCounter = 0;
bool waitingForResponse = false;
int rxIndex = 0;

// ------------------------------------------------------------------
// ChaCha20 Software Implementation (for verification)
// ------------------------------------------------------------------

#define ROTL32(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

#define CHACHA_QUARTERROUND(a, b, c, d) \
  a += b; d ^= a; d = ROTL32(d, 16); \
  c += d; b ^= c; b = ROTL32(b, 12); \
  a += b; d ^= a; d = ROTL32(d, 8); \
  c += d; b ^= c; b = ROTL32(b, 7);

void chacha20_block(uint32_t out[16], const uint32_t in[16]) {
  uint32_t x[16];
  
  // Initialize working state
  for (int i = 0; i < 16; i++) {
    x[i] = in[i];
  }
  
  // 20 rounds (10 double-rounds)
  for (int i = 0; i < 10; i++) {
    // Column rounds
    CHACHA_QUARTERROUND(x[0], x[4], x[8],  x[12]);
    CHACHA_QUARTERROUND(x[1], x[5], x[9],  x[13]);
    CHACHA_QUARTERROUND(x[2], x[6], x[10], x[14]);
    CHACHA_QUARTERROUND(x[3], x[7], x[11], x[15]);
    
    // Diagonal rounds
    CHACHA_QUARTERROUND(x[0], x[5], x[10], x[15]);
    CHACHA_QUARTERROUND(x[1], x[6], x[11], x[12]);
    CHACHA_QUARTERROUND(x[2], x[7], x[8],  x[13]);
    CHACHA_QUARTERROUND(x[3], x[4], x[9],  x[14]);
  }
  
  // Add working state to original state
  for (int i = 0; i < 16; i++) {
    out[i] = x[i] + in[i];
  }
}

void chacha20_encrypt(uint8_t* output, const uint8_t* input, size_t len,
                      const uint8_t key[32], const uint8_t nonce[8], uint32_t counter) {
  uint32_t state[16];
  uint32_t keystream[16];
  
  // ChaCha20 constants "expand 32-byte k"
  state[0] = 0x61707865;
  state[1] = 0x3320646e;
  state[2] = 0x79622d32;
  state[3] = 0x6b206574;
  
  // Key (256-bit / 32 bytes)
  for (int i = 0; i < 8; i++) {
    state[4 + i] = ((uint32_t)key[i*4 + 0] << 0) |
                   ((uint32_t)key[i*4 + 1] << 8) |
                   ((uint32_t)key[i*4 + 2] << 16) |
                   ((uint32_t)key[i*4 + 3] << 24);
  }
  
  // Block counter (64-bit)
  state[12] = counter;
  state[13] = 0;
  
  // Nonce (64-bit)
  state[14] = ((uint32_t)nonce[0] << 0) |
              ((uint32_t)nonce[1] << 8) |
              ((uint32_t)nonce[2] << 16) |
              ((uint32_t)nonce[3] << 24);
  state[15] = ((uint32_t)nonce[4] << 0) |
              ((uint32_t)nonce[5] << 8) |
              ((uint32_t)nonce[6] << 16) |
              ((uint32_t)nonce[7] << 24);
  
  // Generate keystream
  chacha20_block(keystream, state);
  
  // XOR plaintext with keystream
  for (size_t i = 0; i < len && i < 64; i++) {
    output[i] = input[i] ^ ((uint8_t*)keystream)[i];
  }
}

// ------------------------------------------------------------------
// Main Functions
// ------------------------------------------------------------------

void setup() {
  // Initialize USB Serial for debugging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("ESP32 ChaCha20 Encryption Test");
  Serial.println("FPGA Hardware Accelerator Verification");
  Serial.println("========================================");
  
  // Initialize UART2 for FPGA communication
  FPGASerial.begin(BAUD_RATE, SERIAL_8N1, FPGA_RX_PIN, FPGA_TX_PIN);
  Serial.println("FPGA UART initialized (115200 baud, 8N1)");
  Serial.println("========================================\n");
  
  delay(500);
}

void loop() {
  // Send data to FPGA periodically
  if (!waitingForResponse && (millis() - lastSendTime >= sendInterval)) {
    sendTestBlock();
  }
  
  // Check for data received from FPGA
  if (waitingForResponse && FPGASerial.available()) {
    receiveFromFPGA();
  }
  
  // Check for manual commands from USB Serial
  if (Serial.available()) {
    handleSerialCommand();
  }
}

void sendTestBlock() {
  testCounter++;
  lastSendTime = millis();
  waitingForResponse = true;
  rxIndex = 0;
  
  Serial.println("╔════════════════════════════════════════╗");
  Serial.printf("║  Test Block #%-3d                      ║\n", testCounter);
  Serial.println("╚════════════════════════════════════════╝");
  
  // Generate test plaintext
  // For test purposes, use a simple pattern
  for (int i = 0; i < BLOCK_SIZE; i++) {
    plaintext[i] = (testCounter * 64 + i) & 0xFF;
  }
  
  Serial.println("\n[1] Plaintext (64 bytes):");
  printHexBlock(plaintext, BLOCK_SIZE);
  
  // Compute expected ciphertext using software ChaCha20
  uint8_t key[32] = {0};  // All-zero key (matches FPGA)
  uint8_t nonce[8] = {0}; // All-zero nonce (matches FPGA)
  uint32_t counter = testCounter - 1;  // Block counter
  
  chacha20_encrypt(ciphertext_sw, plaintext, BLOCK_SIZE, key, nonce, counter);
  
  Serial.println("\n[2] Expected Ciphertext (Software ChaCha20):");
  printHexBlock(ciphertext_sw, BLOCK_SIZE);
  
  // Send plaintext to FPGA
  Serial.println("\n[3] Sending to FPGA...");
  FPGASerial.write(plaintext, BLOCK_SIZE);
  Serial.println("✓ 64 bytes sent");
  Serial.println("\n[4] Waiting for FPGA response...\n");
}

void receiveFromFPGA() {
  // Read bytes as they arrive
  while (FPGASerial.available() && rxIndex < BLOCK_SIZE) {
    ciphertext_fpga[rxIndex] = FPGASerial.read();
    rxIndex++;
  }
  
  // Check if we received the full block
  if (rxIndex >= BLOCK_SIZE) {
    waitingForResponse = false;
    
    Serial.println("[5] Received from FPGA:");
    printHexBlock(ciphertext_fpga, BLOCK_SIZE);
    
    // Verify against software implementation
    Serial.println("\n[6] Verification:");
    bool match = true;
    int mismatchCount = 0;
    
    for (int i = 0; i < BLOCK_SIZE; i++) {
      if (ciphertext_fpga[i] != ciphertext_sw[i]) {
        if (mismatchCount == 0) {
          Serial.println("✗ MISMATCH DETECTED!");
          Serial.println("\nByte | FPGA | Expected");
          Serial.println("-----|------|----------");
        }
        Serial.printf("%4d | 0x%02X | 0x%02X\n", i, ciphertext_fpga[i], ciphertext_sw[i]);
        match = false;
        mismatchCount++;
        
        if (mismatchCount >= 10) {
          Serial.printf("... and %d more mismatches\n", BLOCK_SIZE - i - 1);
          break;
        }
      }
    }
    
    if (match) {
      Serial.println("✓ PERFECT MATCH!");
      Serial.println("  FPGA encryption matches software implementation.");
    } else {
      Serial.printf("✗ VERIFICATION FAILED (%d/%d bytes incorrect)\n", mismatchCount, BLOCK_SIZE);
    }
    
    Serial.println("\n========================================\n");
  }
}

void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  command.toLowerCase();
  
  if (command == "test" || command == "t") {
    Serial.println("Manual test triggered...");
    lastSendTime = 0;  // Force immediate send
    waitingForResponse = false;
  }
  else if (command == "reset" || command == "r") {
    Serial.println("Resetting counter...");
    testCounter = 0;
  }
  else if (command.startsWith("send ")) {
    // Custom plaintext (hex string)
    Serial.println("Custom send not implemented yet");
  }
  else if (command == "help" || command == "h") {
    printHelp();
  }
  else if (command.length() > 0) {
    Serial.println("Unknown command. Type 'help' for commands.");
  }
}

void printHexBlock(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (i % 16 == 0) {
      if (i > 0) Serial.println();
      Serial.printf("  %04X: ", i);
    }
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
}

void printHelp() {
  Serial.println("\n========================================");
  Serial.println("Available Commands:");
  Serial.println("========================================");
  Serial.println("  test, t     - Send test block immediately");
  Serial.println("  reset, r    - Reset block counter");
  Serial.println("  help, h     - Show this help");
  Serial.println("========================================\n");
}

