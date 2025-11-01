module uart_test(
	input                        clk,
	input                        rst_n,
	input                        uart_rx,
	output                       uart_tx
);

// Clock and UART parameters
parameter                        CLK_FRE  = 27;      // MHz (Tang Nano 9K default clock)
parameter                        UART_FRE = 115200;  // Baud rate

// Internal signals
reg[7:0]                         tx_data;
reg                              tx_data_valid;
wire                             tx_data_ready;
wire[7:0]                        rx_data;
wire                             rx_data_valid;
wire                             rx_data_ready;

// Ready to receive data anytime
assign rx_data_ready = 1'b1;

// Simple echo logic: when data is received, echo it back
always@(posedge clk or negedge rst_n)
begin
	if(rst_n == 1'b0)
	begin
		tx_data <= 8'd0;
		tx_data_valid <= 1'b0;
	end
	else
	begin
		// When new data is received and transmitter is ready
		if(rx_data_valid == 1'b1 && tx_data_ready == 1'b1)
		begin
			tx_data <= rx_data;        // Echo back received data
			tx_data_valid <= 1'b1;     // Signal valid data to transmit
		end
		// Clear valid flag after data is accepted by transmitter
		else if(tx_data_valid == 1'b1 && tx_data_ready == 1'b1)
		begin
			tx_data_valid <= 1'b0;
		end
	end
end

uart_rx#
(
	.CLK_FRE(CLK_FRE),
	.BAUD_RATE(UART_FRE)
) uart_rx_inst
(
	.clk                        (clk                      ),
	.rst_n                      (rst_n                    ),
	.rx_data                    (rx_data                  ),
	.rx_data_valid              (rx_data_valid            ),
	.rx_data_ready              (rx_data_ready            ),
	.rx_pin                     (uart_rx                  )
);

uart_tx#
(
	.CLK_FRE(CLK_FRE),
	.BAUD_RATE(UART_FRE)
) uart_tx_inst
(
	.clk                        (clk                      ),
	.rst_n                      (rst_n                    ),
	.tx_data                    (tx_data                  ),
	.tx_data_valid              (tx_data_valid            ),
	.tx_data_ready              (tx_data_ready            ),
	.tx_pin                     (uart_tx                  )
);
endmodule