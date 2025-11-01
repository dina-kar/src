/*
 * ESP32 UART Communication with Tang Nano 9K FPGA
 * 
 * This program communicates with the FPGA via UART.
 * It sends data to the FPGA and receives the echoed data back.
 * 
 * Hardware Connections:
 * ESP32 TX (GPIO17) -> FPGA RX (Pin 18)
 * ESP32 RX (GPIO16) -> FPGA TX (Pin 17)
 * ESP32 GND         -> FPGA GND
 * 
 * IMPORTANT: Tang Nano 9K uses 3.3V logic, which is compatible with ESP32
 */

// Use UART2 for FPGA communication (UART0 is used for USB programming/debugging)
HardwareSerial FPGASerial(2);

// UART Configuration
#define FPGA_RX_PIN 16  // ESP32 RX connected to FPGA TX
#define FPGA_TX_PIN 17  // ESP32 TX connected to FPGA RX
#define BAUD_RATE 115200

// Timing
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 2000; // Send data every 2 seconds
int messageCounter = 0;

void setup() {
  // Initialize USB Serial for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("ESP32 <-> FPGA UART Test");
  Serial.println("=================================");
  
  // Initialize UART2 for FPGA communication
  FPGASerial.begin(BAUD_RATE, SERIAL_8N1, FPGA_RX_PIN, FPGA_TX_PIN);
  Serial.println("FPGA UART initialized");
  Serial.println("Baud Rate: 115200");
  Serial.println("Format: 8N1");
  Serial.println("=================================\n");
  
  delay(500);
}

void loop() {
  // Send data to FPGA periodically
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    sendTestData();
  }
  
  // Check for data received from FPGA
  if (FPGASerial.available()) {
    receiveFromFPGA();
  }
  
  // Check for commands from USB Serial
  if (Serial.available()) {
    handleSerialCommand();
  }
}

void sendTestData() {
  messageCounter++;
  
  // Create test message
  String message = "MSG#" + String(messageCounter) + "\r\n";
  
  Serial.print("Sending to FPGA: ");
  Serial.print(message);
  
  // Send to FPGA
  FPGASerial.print(message);
  
  Serial.println("Waiting for echo...");
}

void receiveFromFPGA() {
  String received = "";
  
  // Read all available bytes
  while (FPGASerial.available()) {
    char c = FPGASerial.read();
    received += c;
    delay(2); // Small delay to allow more bytes to arrive
  }
  
  if (received.length() > 0) {
    Serial.print("Received from FPGA: ");
    
    // Print as string
    Serial.print(received);
    
    // Also print hex values for debugging
    Serial.print(" [HEX: ");
    for (int i = 0; i < received.length(); i++) {
      if (received[i] < 0x10) Serial.print("0");
      Serial.print(received[i], HEX);
      Serial.print(" ");
    }
    Serial.println("]");
  }
}

void handleSerialCommand() {
  String command = Serial.readStringUntil('\n');
  command.trim();
  
  if (command.length() > 0) {
    Serial.print("Sending custom message: ");
    Serial.println(command);
    
    // Send custom message to FPGA
    FPGASerial.println(command);
    
    Serial.println("Waiting for echo...");
  }
}

// Alternative simple test functions

void sendSingleByte(byte data) {
  Serial.print("Sending byte: 0x");
  Serial.println(data, HEX);
  FPGASerial.write(data);
}

void sendByteArray(byte* data, int length) {
  Serial.print("Sending ");
  Serial.print(length);
  Serial.println(" bytes to FPGA");
  
  for (int i = 0; i < length; i++) {
    FPGASerial.write(data[i]);
    Serial.print("0x");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

