`timescale 1ns / 1ps
// =============================================================================
//  mlp_accel.v  –  MLP inference engine
//
//  Architecture (all values in unsigned 0.8 fixed-point, scale = 256):
//    Hidden layer : N[64][2]     = X[64][8] × W_HID[8][2] / 256
//                   bias handled implicitly: col-0 of X is always 256, so the
//                   contribution is W_HID[n][0]<<8 added to the accumulator.
//    Sigmoid      : N_sig[64][2] = sigmoid_LUT[ N[64][2] ]
//    Output layer : RES[64][1]   = (256·W_OUT[0] + N_sig·W_OUT[1..2]) / 256
//    Threshold    : prediction   = (RES > 128) ? 1 : 0
//
//  External RAM interfaces (all synchronous, 1-cycle read latency):
//    X_RAM    : 512 entries, 8-bit  (read-only from this module)
//    WHID_RAM : 16  entries, 8-bit  (read-only)
//    WOUT_RAM : 3   entries, 8-bit  (read-only; depth_bits=2 so addr 0..3)
//    RES_RAM  : 64  entries, 8-bit  (write-only from this module)
//
//  Internal storage (reg arrays, combinational read, no wait cycles):
//    HID_RES     : 128 entries – N[row*2+neuron]
//    N_sig_RAM : 128 entries – sigmoid(N)[row*2+neuron]
//    sig_lut   : 256 entries – sigmoid lookup table (ROM)
// =============================================================================

`include "sharedparams.vh"

module mlp_accel (
    input  clk,
    input  Start,
    output reg Done,

    // X_RAM read (512 entries: row*8 + col, col in 0..7)
    output        x_rd_en,
    output [`X_DEPTH_BITS-1:0]  x_rd_addr,
    input      [`AXI_DATA_WIDTH-1:0]  x_rd_data,

    // W_HID_RAM read (16 entries: neuron*8 + k)
    output        whid_rd_en,
    output [`WHID_DEPTH_BITS-1:0]  whid_rd_addr,
    input      [`AXI_DATA_WIDTH-1:0]  whid_rd_data,

    // W_OUT_RAM read (3 entries: 0=bias, 1=hid0, 2=hid1)
    output         wout_rd_en,
    output  [`WOUT_DEPTH_BITS-1:0]  wout_rd_addr,
    input      [`AXI_DATA_WIDTH-1:0]  wout_rd_data,

    // RES_RAM write (64 entries)
    output  [`NUM_SAMPLES-1:0] RES
);

// ---------------------------------------------------------------------------
// Internal intermediate storage (combinational/registered reads, no wait)
// ---------------------------------------------------------------------------
// HID_RES[row*2 + neuron]  – hidden layer result (after divide-by-256 AND sigmoid)
reg [`AXI_DATA_WIDTH - 1:0] HID_RES [`HID_RES_SIZE - 1:0];

// ---------------------------------------------------------------------------
// FSM state encoding
// ---------------------------------------------------------------------------
localparam IDLE              = 3'b001;
// Phase 1: hidden layer  (for each row × neuron × k=0..7)
localparam HID_COMPUTE      = 3'b010;
localparam OUT_COMPUTE      = 3'b100;

reg [2:0] state = IDLE;

reg hid_start;
wire hid_done;

reg out_start;
wire out_done;

// Counters
reg [`NUM_SAMPLES:0] row;      // 0..63 
reg       neuron;   // 0..1
reg [2:0] k;        // 1..7 (hidden) or 1..2 (output)

wire hid_res_wr_en;
wire [`HID_RES_DEPTH_BITS-1:0] hid_res_wr_addr;
wire [`AXI_DATA_WIDTH-1:0] hid_res_wr_data;
wire hid_res_rd_en;
wire [`HID_RES_DEPTH_BITS-1:0] hid_res_read_addr;
wire [`AXI_DATA_WIDTH-1:0] hid_res_rd_data;

// ---------------------------------------------------------------------------
// Main FSM
// ---------------------------------------------------------------------------
always @(posedge clk) begin
    Done <= 1'b0;
    hid_start <= 1'b0;

    case (state) 
        IDLE: begin
            if (Start) begin
                state <= HID_COMPUTE;
                hid_start <= 1'b1;
            end
        end
        HID_COMPUTE: begin
            if (hid_done) begin
                state <= OUT_COMPUTE;
                out_start <= 1'b1;
            end
        end
        OUT_COMPUTE: begin
            if (out_start) begin
                state <= IDLE;
            end
        end
        default: state <= IDLE; 
    endcase
end

hid_compute hid_compute0 (
    .clk(clk),	
    .Start(hid_start),
    .Done(hid_done),
    .x_rd_en(x_rd_en),
    .x_rd_addr(x_rd_addr),
    .x_rd_data(x_rd_data),
    .whid_rd_en(whid_rd_en),
    .whid_rd_addr(whid_rd_addr),
    .whid_rd_data(whid_rd_data),
    .hid_res_wr_en(hid_res_wr_en),
    .hid_res_wr_addr(hid_res_wr_addr),
    .hid_res_wr_data(hid_res_wr_data)
);

memory_RAM #(.width(`AXI_DATA_WIDTH), .depth_bits(`HID_RES_DEPTH_BITS)) HID_RES_RAM (
    .clk(clk),
    .write_en(hid_res_wr_en),
    .write_address(hid_res_wr_addr),
    .write_data_in(hid_res_wr_data),
    .read_en(hid_res_rd_en),
    .read_address(hid_res_read_addr),
    .read_data_out(hid_res_rd_data)
);

endmodule
