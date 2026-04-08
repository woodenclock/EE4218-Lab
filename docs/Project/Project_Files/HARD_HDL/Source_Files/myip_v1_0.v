/*
--------------------------------------------------------------------------------
-- myip_v1_0.v  –  AXI-Stream coprocessor wrapper for MLP inference
--
-- Input stream  (531 x 8-bit words, padded in 32-bit TDATA[7:0]):
--   [  0 .. 511] : X[64][8]      row-major; X[r][0] = 0 (bias handled in HW)
--   [512 .. 527] : W_HID[2][8]   neuron-0 first (k=0..7), then neuron-1
--   [528 .. 530] : W_OUT[3]      {bias=80, hid0=50, hid1=200}
--
-- Output stream (64 x 8-bit words):
--   Predictions 0 or 1 for each of the 64 samples.
--------------------------------------------------------------------------------
*/
`timescale 1ns / 1ps
`include "sharedparams.vh"

module myip_v1_0 (
    // DO NOT EDIT BELOW THIS LINE ////////////////////
    ACLK,
    ARESETN,
    S_AXIS_TREADY,
    S_AXIS_TDATA,
    S_AXIS_TLAST,
    S_AXIS_TVALID,
    M_AXIS_TVALID,
    M_AXIS_TDATA,
    M_AXIS_TLAST,
    M_AXIS_TREADY
    // DO NOT EDIT ABOVE THIS LINE ////////////////////
);

input           ACLK;
input           ARESETN;
output reg      S_AXIS_TREADY;
input  [31:0]   S_AXIS_TDATA;
input           S_AXIS_TLAST;
input           S_AXIS_TVALID;
output reg      M_AXIS_TVALID;
output reg [31:0] M_AXIS_TDATA;
output reg      M_AXIS_TLAST;
input           M_AXIS_TREADY;

// // ---------------------------------------------------------------------------
// // RAM sizes
// // ---------------------------------------------------------------------------
// localparam X_DEPTH_BITS    = `X_DEPTH_BITS;    // 9  → 512 entries
// localparam WHID_DEPTH_BITS = `WHID_DEPTH_BITS;  // 4  → 16  entries
// localparam WOUT_DEPTH_BITS = `WOUT_DEPTH_BITS;  // 2  → 4   entries (3 used)
// localparam RES_DEPTH_BITS  = `RES_DEPTH_BITS;   // 6  → 64  entries

// FSM states
localparam  Idle          = 6'b000001;
localparam  Read_X        = 6'b000010;
localparam  Read_WHID     = 6'b000100;
localparam  Read_WOUT     = 6'b001000;
localparam  Compute       = 6'b010000;
localparam  Write_Outputs = 6'b100000;

reg [5:0]   state;
reg         Start;
wire        Done;

// ---------------------------------------------------------------------------
// Counters
// ---------------------------------------------------------------------------
reg [`X_DEPTH_BITS-1:0]         x_counter;     // 0..511
reg [`WHID_DEPTH_BITS-1:0]      whid_counter;  // 0..15
reg [`WOUT_DEPTH_BITS-1:0]      wout_counter;  // 0..2
reg [`RES_DEPTH_BITS-1:0]       write_counter; // 0..63

//---------------------------------------------------------------------------
//X_RAM signals  (written by myip, read by mlp_accel)
//---------------------------------------------------------------------------
reg                             X_write_en;
reg  [`X_DEPTH_BITS-1:0]        X_write_address;
reg  [`AXI_DATA_WIDTH-1:0]      X_write_data_in;
wire                            X_read_en;
wire [`X_DEPTH_BITS-1:0]        X_read_address;
wire [`AXI_DATA_WIDTH-1:0]      X_read_data_out;

//---------------------------------------------------------------------------
//W_HID_RAM signals
//---------------------------------------------------------------------------
reg                             WHID_write_en;
reg  [`WHID_DEPTH_BITS-1:0]     WHID_write_address;
reg  [`AXI_DATA_WIDTH-1:0]      WHID_write_data_in;
wire                            WHID_read_en;
wire [`WHID_DEPTH_BITS-1:0]     WHID_read_address;
wire [`AXI_DATA_WIDTH-1:0]      WHID_read_data_out;

//---------------------------------------------------------------------------
//W_OUT_RAM signals
//---------------------------------------------------------------------------
reg                             WOUT_write_en;
reg  [`WOUT_DEPTH_BITS-1:0]     WOUT_write_address;
reg  [`AXI_DATA_WIDTH-1:0]      WOUT_write_data_in;
wire                            WOUT_read_en;
wire [`WOUT_DEPTH_BITS-1:0]     WOUT_read_address;
wire [`AXI_DATA_WIDTH-1:0]      WOUT_read_data_out;

//---------------------------------------------------------------------------
//RES_RAM signals  (written by mlp_accel, read by myip)
//---------------------------------------------------------------------------
wire                            RES_write_en;
wire [`RES_DEPTH_BITS-1:0]      RES_write_address;
wire [`AXI_DATA_WIDTH-1:0]      RES_write_data_in;
reg                             RES_read_en;
reg  [`RES_DEPTH_BITS-1:0]      RES_read_address;
wire [`AXI_DATA_WIDTH-1:0]      RES_read_data_out;

// ===========================================================================
//  Main FSM
// ===========================================================================
always @(posedge ACLK) begin

    if (!ARESETN) begin
        state <= Idle;
    end else begin

        // ── Default: deassert write enables every cycle ──────────────────
        Start         <= 1'b0;
        X_write_en    <= 1'b0;
        WHID_write_en <= 1'b0;
        WOUT_write_en <= 1'b0;

        case (state)

            // ─────────────────────────────────────────────────────────────
            Idle: begin
                S_AXIS_TREADY <= 1'b0;
                M_AXIS_TVALID <= 1'b0;
                M_AXIS_TLAST  <= 1'b0;
                RES_read_en   <= 1'b0;
                x_counter     <= {`X_DEPTH_BITS{1'b0}};
                whid_counter  <= {`WHID_DEPTH_BITS{1'b0}};
                wout_counter  <= {`WOUT_DEPTH_BITS{1'b0}};
                write_counter <= {`RES_DEPTH_BITS{1'b0}};
                if (S_AXIS_TVALID) begin
                    S_AXIS_TREADY <= 1'b1;
                    state         <= Read_X;
                end
            end

            // ─────────────────────────────────────────────────────────────
            // Receive 512 bytes into X_RAM: X[r][c] at address r*8+c
            // X[r][0] is sent as 0 (bias = 256 is handled implicitly in HW)
            // ─────────────────────────────────────────────────────────────
            Read_X: begin
                S_AXIS_TREADY <= 1'b1;
                if (S_AXIS_TVALID) begin
                    X_write_en      <= 1'b1;
                    X_write_address <= x_counter;
                    X_write_data_in <= S_AXIS_TDATA;
                    if (x_counter == `NUM_X_PACKETS - 1) begin  // 511
                        state <= Read_WHID;
                    end else begin
                        x_counter <= x_counter + 1'b1;
                    end
                end
            end

            Read_WHID: begin
                S_AXIS_TREADY <= 1'b1;
                if (S_AXIS_TVALID) begin
                    WHID_write_en      <= 1'b1;
                    WHID_write_address <= whid_counter;
                    WHID_write_data_in <= S_AXIS_TDATA;
                    if (whid_counter == `NUM_WHID_PACKETS - 1) begin
                        state <= Read_WOUT;
                    end else begin
                        whid_counter <= whid_counter + 1'b1;
                    end
                end
            end

            // ─────────────────────────────────────────────────────────────
            // Receive 3 bytes into W_OUT_RAM:
            //   [0] = bias weight (80)
            //   [1] = hidden-neuron-0 weight (50)
            //   [2] = hidden-neuron-1 weight (200)
            // ─────────────────────────────────────────────────────────────
            Read_WOUT: begin
                S_AXIS_TREADY <= 1'b1;
                if (S_AXIS_TVALID) begin
                    WOUT_write_en      <= 1'b1;
                    WOUT_write_address <= wout_counter;
                    WOUT_write_data_in <= S_AXIS_TDATA;
                    if (wout_counter == `NUM_WOUT_PACKETS - 1) begin
                        S_AXIS_TREADY <= 1'b0;
                        Start         <= 1'b1;   // 1-cycle signal to mlp_accel
                        state         <= Compute;
                    end else begin
                        wout_counter <= wout_counter + 1'b1;
                    end
                end
            end

            // ─────────────────────────────────────────────────────────────
            // Wait for mlp_accel to finish.
            // Start was asserted for exactly 1 cycle (previous state).
            // Mirrors the original lab's two-sub-state Compute pattern to
            // pre-read RES[0] before entering Write_Outputs.
            // ─────────────────────────────────────────────────────────────
            Compute: begin
                // Start deasserted by default at top of always block
                if (Done) begin
                    write_counter    <= {`RES_DEPTH_BITS{1'b0}};
                    M_AXIS_TVALID    <= 1'b0;
                    state            <= Write_Outputs;
                end
            end

            // ─────────────────────────────────────────────────────────────
            // Stream 64 output words.
            // When entering this state:
            //   RES_read_address = 1  (pre-advanced)
            //   RES_read_data_out = RES[0]
            //   write_counter = 0
            //
            // Each cycle: present RES[counter] from read_data_out,
            //             advance address = counter+2 (for counter+1 next cycle).
            // At counter=62: addr wraps to 0 at counter=63 (don't care; RES[63]
            //                is already in the pipeline from addr=63 at counter=61).
            // ─────────────────────────────────────────────────────────────
            Write_Outputs: begin
                RES_read_en   <= 1'b1;
                M_AXIS_TVALID <= 1'b1;
                M_AXIS_TDATA  <= RES_read_data_out;   // zero-pad to 32-bit

                if (M_AXIS_TREADY) begin
                    if (write_counter == `NUM_RES_PACKETS - 1) begin
                        M_AXIS_TLAST <= 1'b1;
                        RES_read_en  <= 1'b0;
                        state        <= Idle;
                    end else begin
                        // advance address (will be addr for write_counter+1, i.e., +2 from current)
                        RES_read_address <= write_counter + 6'd2;
                        write_counter    <= write_counter + 1'b1;
                    end
                end
            end
            default: state <= Idle;
        endcase
    end
end


// ===========================================================================
//  RAM Instantiations
// ===========================================================================

// X_RAM: 512 entries × 8-bit
memory_RAM #(.width(`AXI_DATA_WIDTH), .depth_bits(`X_DEPTH_BITS)) X_RAM (
    .clk(ACLK),
    .write_en(X_write_en),
    .write_address(X_write_address),
    .write_data_in(X_write_data_in),
    .read_en(X_read_en),
    .read_address(X_read_address),
    .read_data_out(X_read_data_out)
);

// W_HID_RAM: 16 entries × 8-bit
memory_RAM #(.width(`AXI_DATA_WIDTH), .depth_bits(`WHID_DEPTH_BITS)) WHID_RAM (
    .clk(ACLK),
    .write_en(WHID_write_en),
    .write_address(WHID_write_address),
    .write_data_in(WHID_write_data_in),
    .read_en(WHID_read_en),
    .read_address(WHID_read_address),
    .read_data_out(WHID_read_data_out)
);

// W_OUT_RAM: 4 entries × 8-bit (only 3 used)
memory_RAM #(.width(`AXI_DATA_WIDTH), .depth_bits(`WOUT_DEPTH_BITS)) WOUT_RAM (
    .clk(ACLK),
    .write_en(WOUT_write_en),
    .write_address(WOUT_write_address),
    .write_data_in(WOUT_write_data_in),
    .read_en(WOUT_read_en),
    .read_address(WOUT_read_address),
    .read_data_out(WOUT_read_data_out)
);

// RES_RAM: 64 entries × 8-bit  (written by mlp_accel, read by myip)
memory_RAM #(.width(`AXI_DATA_WIDTH), .depth_bits(`RES_DEPTH_BITS)) RES_RAM (
    .clk(ACLK),
    .write_en(RES_write_en),
    .write_address(RES_write_address),
    .write_data_in(RES_write_data_in),
    .read_en(RES_read_en),
    .read_address(RES_read_address),
    .read_data_out(RES_read_data_out)
);


// ===========================================================================
//  mlp_accel Instantiation
// ===========================================================================
mlp_accel mlp_accel_0 (
    .clk(ACLK),
    .Start(Start),
    .Done(Done),

    // X_RAM read port
    .x_rd_en(X_read_en),
    .x_rd_addr(X_read_address),
    .x_rd_data(X_read_data_out),

    // W_HID_RAM read port
    .whid_rd_en(WHID_read_en),
    .whid_rd_addr(WHID_read_address),
    .whid_rd_data(WHID_read_data_out),

    // W_OUT_RAM read port
    .wout_rd_en(WOUT_read_en),
    .wout_rd_addr(WOUT_read_address),
    .wout_rd_data(WOUT_read_data_out),

    // RES_RAM write port
    .res_wr_en(RES_write_en),
    .res_wr_addr(RES_write_address),
    .res_wr_data(RES_write_data_in)
);

endmodule
