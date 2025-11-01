module main(
              input  wire clk,
              input  wire rst_n,
              input  wire uart_rx,
              output wire uart_tx
             );

  // Parameters for UART.
  parameter CLK_FRE   = 27;       // MHz
  parameter UART_BAUD = 115200;

  // ------------------------------------------------------------------
  // UART wiring.
  // ------------------------------------------------------------------
  wire [7:0] rx_data;
  wire       rx_data_valid;
  wire       rx_data_ready;

  wire [7:0] tx_data;
  wire       tx_data_valid;
  wire       tx_data_ready;

  assign rx_data_ready = 1'b1;  // Always ready to accept bytes

  uart_rx #(
    .CLK_FRE(CLK_FRE),
    .BAUD_RATE(UART_BAUD)
  ) u_rx (
    .clk(clk),
    .rst_n(rst_n),
    .rx_data(rx_data),
    .rx_data_valid(rx_data_valid),
    .rx_data_ready(rx_data_ready),
    .rx_pin(uart_rx)
  );

  uart_tx #(
    .CLK_FRE(CLK_FRE),
    .BAUD_RATE(UART_BAUD)
  ) u_tx (
    .clk(clk),
    .rst_n(rst_n),
    .tx_data(tx_data),
    .tx_data_valid(tx_data_valid),
    .tx_data_ready(tx_data_ready),
    .tx_pin(uart_tx)
  );

  // ------------------------------------------------------------------
  // ChaCha core wiring and configuration.
  // We use ChaCha20 (20 rounds), 256-bit key, zero IV by default.
  // The core produces a 64-byte keystream per block; we XOR in HW.
  // ------------------------------------------------------------------
  localparam [4:0]  CHACHA_ROUNDS = 5'd20;  // ChaCha20
  localparam        CHACHA_KEYLEN = 1'b1;   // 1 = 256-bit key

  // NOTE: Replace KEY/IV with your real secrets in production.
  // For verification we use all-zero key and IV.
  localparam [255:0] CHACHA_KEY = 256'h0;
  localparam [63:0]  CHACHA_IV  = 64'h0;

  reg         core_init;
  reg         core_next;  // unused, single-block at a time
  wire        core_ready;
  reg  [63:0] core_ctr;

  wire [511:0] core_data_out;
  wire         core_data_out_valid;

  // We feed zero plaintext into the core and do XOR with received data externally.
  wire [511:0] core_data_in = 512'h0;

  chacha_core core (
    .clk(clk),
    .reset_n(rst_n),
    .init(core_init),
    .next(core_next),
    .key(CHACHA_KEY),
    .keylen(CHACHA_KEYLEN),
    .iv(CHACHA_IV),
    .ctr(core_ctr),
    .rounds(CHACHA_ROUNDS),
    .data_in(core_data_in),
    .ready(core_ready),
    .data_out(core_data_out),
    .data_out_valid(core_data_out_valid)
  );

  // ------------------------------------------------------------------
  // Buffers and control FSM.
  // - Accumulate 64 bytes from UART RX into plain_buf
  // - Trigger ChaCha to generate keystream for current counter
  // - XOR keystream with buffered plaintext and transmit 64 bytes
  // - Increment counter and return to RX
  // ------------------------------------------------------------------
  reg [7:0] plain_buf [0:63];
  reg [7:0] ks_buf    [0:63];

  reg [6:0] rx_count;   // 0..64
  reg [6:0] tx_count;   // 0..64

  reg [2:0] state;
  localparam S_RX_WAIT   = 3'd0;
  localparam S_CORE_INIT = 3'd1;
  localparam S_CORE_WAIT = 3'd2;
  localparam S_LOAD_KS   = 3'd3;
  localparam S_PRE_TX    = 3'd5;
  localparam S_TX_SEND   = 3'd4;

  // Add pipeline register for TX data
  reg [7:0] tx_data_pipe;
  reg       tx_data_valid_pipe;

  // Temporary register for keystream unpacking
  reg [31:0] word_tmp;
  
  integer i;

  // Capture incoming bytes until we have a full 64-byte block.
  // Accept data in any state to avoid dropping bytes when ESP32 sends immediately
  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      rx_count <= 7'd0;
    end else begin
      if (rx_data_valid && rx_count < 7'd64) begin
        plain_buf[rx_count] <= rx_data;
        rx_count <= rx_count + 7'd1;
      end else if (state == S_TX_SEND && tx_count == 7'd63 && tx_data_valid_pipe && tx_data_ready) begin
        // Reset rx_count when last byte is transmitted to prepare for next block
        rx_count <= 7'd0;
      end
    end
  end

  // FSM for ChaCha and UART TX.
  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state          <= S_RX_WAIT;
      core_init      <= 1'b0;
      core_next      <= 1'b0;
      core_ctr       <= 64'h0;
      tx_count       <= 7'd0;
      tx_data_pipe   <= 8'h00;
      tx_data_valid_pipe <= 1'b0;
    end else begin
      // Defaults
      core_init     <= 1'b0;
      core_next     <= 1'b0;

      case (state)
        S_RX_WAIT: begin
          if (rx_count == 7'd64 && core_ready) begin
            // Start ChaCha for current block counter
            core_init <= 1'b1;  // one-cycle pulse
            state     <= S_CORE_INIT;
          end
        end

        S_CORE_INIT: begin
          // Deassert init, wait for data_out_valid
          core_init <= 1'b0;
          state     <= S_CORE_WAIT;
        end

        S_CORE_WAIT: begin
          if (core_data_out_valid) begin
            state <= S_LOAD_KS;
          end
        end

        S_LOAD_KS: begin
          // Unpack 512-bit keystream into byte array in canonical order.
          // ChaCha core outputs 512 bits (16 words, 64 bytes)
          // Extract each 32-bit word and split into 4 bytes
          ks_buf[0]  <= core_data_out[511:504];
          ks_buf[1]  <= core_data_out[503:496];
          ks_buf[2]  <= core_data_out[495:488];
          ks_buf[3]  <= core_data_out[487:480];
          
          ks_buf[4]  <= core_data_out[479:472];
          ks_buf[5]  <= core_data_out[471:464];
          ks_buf[6]  <= core_data_out[463:456];
          ks_buf[7]  <= core_data_out[455:448];
          
          ks_buf[8]  <= core_data_out[447:440];
          ks_buf[9]  <= core_data_out[439:432];
          ks_buf[10] <= core_data_out[431:424];
          ks_buf[11] <= core_data_out[423:416];
          
          ks_buf[12] <= core_data_out[415:408];
          ks_buf[13] <= core_data_out[407:400];
          ks_buf[14] <= core_data_out[399:392];
          ks_buf[15] <= core_data_out[391:384];
          
          ks_buf[16] <= core_data_out[383:376];
          ks_buf[17] <= core_data_out[375:368];
          ks_buf[18] <= core_data_out[367:360];
          ks_buf[19] <= core_data_out[359:352];
          
          ks_buf[20] <= core_data_out[351:344];
          ks_buf[21] <= core_data_out[343:336];
          ks_buf[22] <= core_data_out[335:328];
          ks_buf[23] <= core_data_out[327:320];
          
          ks_buf[24] <= core_data_out[319:312];
          ks_buf[25] <= core_data_out[311:304];
          ks_buf[26] <= core_data_out[303:296];
          ks_buf[27] <= core_data_out[295:288];
          
          ks_buf[28] <= core_data_out[287:280];
          ks_buf[29] <= core_data_out[279:272];
          ks_buf[30] <= core_data_out[271:264];
          ks_buf[31] <= core_data_out[263:256];
          
          ks_buf[32] <= core_data_out[255:248];
          ks_buf[33] <= core_data_out[247:240];
          ks_buf[34] <= core_data_out[239:232];
          ks_buf[35] <= core_data_out[231:224];
          
          ks_buf[36] <= core_data_out[223:216];
          ks_buf[37] <= core_data_out[215:208];
          ks_buf[38] <= core_data_out[207:200];
          ks_buf[39] <= core_data_out[199:192];
          
          ks_buf[40] <= core_data_out[191:184];
          ks_buf[41] <= core_data_out[183:176];
          ks_buf[42] <= core_data_out[175:168];
          ks_buf[43] <= core_data_out[167:160];
          
          ks_buf[44] <= core_data_out[159:152];
          ks_buf[45] <= core_data_out[151:144];
          ks_buf[46] <= core_data_out[143:136];
          ks_buf[47] <= core_data_out[135:128];
          
          ks_buf[48] <= core_data_out[127:120];
          ks_buf[49] <= core_data_out[119:112];
          ks_buf[50] <= core_data_out[111:104];
          ks_buf[51] <= core_data_out[103:96];
          
          ks_buf[52] <= core_data_out[95:88];
          ks_buf[53] <= core_data_out[87:80];
          ks_buf[54] <= core_data_out[79:72];
          ks_buf[55] <= core_data_out[71:64];
          
          ks_buf[56] <= core_data_out[63:56];
          ks_buf[57] <= core_data_out[55:48];
          ks_buf[58] <= core_data_out[47:40];
          ks_buf[59] <= core_data_out[39:32];
          
          ks_buf[60] <= core_data_out[31:24];
          ks_buf[61] <= core_data_out[23:16];
          ks_buf[62] <= core_data_out[15:8];
          ks_buf[63] <= core_data_out[7:0];
          
          state <= S_PRE_TX;
        end

        S_PRE_TX: begin
          tx_count      <= 7'd0;
          // Prepare first byte for transmission
          tx_data_pipe <= plain_buf[0] ^ ks_buf[0];
          tx_data_valid_pipe <= 1'b1;
          state <= S_TX_SEND;
        end

        S_TX_SEND: begin
          if (tx_data_valid_pipe && tx_data_ready) begin
            if (tx_count == 7'd63) begin
              // Last byte of block sent
              tx_count      <= 7'd64;
              tx_data_valid_pipe <= 1'b0;
              // Increment counter for next block and go back to RX
              core_ctr      <= core_ctr + 64'd1;
              state         <= S_RX_WAIT;
            end else begin
              tx_count <= tx_count + 7'd1;
              // Prepare next byte
              tx_data_pipe <= plain_buf[tx_count + 7'd1] ^ ks_buf[tx_count + 7'd1];
            end
          end
        end

        default: begin
          state <= S_RX_WAIT;
        end
      endcase
    end
  end

  // Connect pipeline to UART TX
  assign tx_data = tx_data_pipe;
  assign tx_data_valid = tx_data_valid_pipe;

endmodule
