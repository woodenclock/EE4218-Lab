`timescale 1ns / 1ps
`include "sharedparams.vh"

module out_compute
	(
		input                                   clk,										
		input                                   Start,										
		output reg                              Done,									
		
        output reg                              hid_res_rd_en,
        output reg [`HID_RES_DEPTH_BITS-1:0]    hid_res_rd_addr,
        input      [`AXI_DATA_WIDTH-1:0]        hid_res_rd_data,

        output reg                              wout_rd_en,
        output reg [`WOUT_DEPTH_BITS-1:0]       wout_rd_addr,
        input      [`AXI_DATA_WIDTH-1:0]        wout_rd_data,

		output  [`NUM_SAMPLES-1:0]           RES
	);

localparam NUM_SETS = `NUM_SAMPLES / 2;
localparam SET_DEPTH_BITS = $clog2(NUM_SETS);

reg running;

reg [`NUM_SAMPLES-1:0]RES_reg;
assign RES = RES_reg;

reg [SET_DEPTH_BITS-1:0] set_s1;
wire [SET_DEPTH_BITS-1:0] set_s1_out = (set_s1 >= NUM_SETS-1) ? {SET_DEPTH_BITS{1'b0}} : set_s1 + 1'b1;

// stage 1 set read enable
always @(posedge clk) begin
    if (Start) begin
        hid_res_rd_en <= 1'b1;
        wout_rd_en <= 1'b1;
        set_s1 <= {SET_DEPTH_BITS{1'b0}};
        hid_res_rd_addr <= {`HID_RES_DEPTH_BITS{1'b0}};
        wout_rd_addr <= {`WOUT_DEPTH_BITS{1'b0}};
        running <= 1'b1;
        Done <= 1'b0;
    end
    else if (running) begin
        set_s1 <= set_s1_out;
        hid_res_rd_en <= (set_s1 < NUM_SETS)? 1'b1 : 1'b0;
        wout_rd_en <= (set_s1 < NUM_SETS)? 1'b1 : 1'b0;
        hid_res_rd_addr <= set_s1_out;
        wout_rd_addr <= {`WOUT_DEPTH_BITS{1'b0}};
    end
    else begin
        hid_res_rd_en <= 1'b0;
        wout_rd_en <= 1'b0;
        set_s1 <= {SET_DEPTH_BITS{1'b0}};
        hid_res_rd_addr <= {`HID_RES_DEPTH_BITS{1'b0}};
        wout_rd_addr <= {`WOUT_DEPTH_BITS{1'b0}};
    end
    if (Done) begin
        running <= 1'b0;
    end    
end
    
reg [SET_DEPTH_BITS-1:0]   set_s2;

// stage 2 wait for outputs
always @(posedge clk) begin
    if (!running) begin
        set_s2 <= {SET_DEPTH_BITS{1'b0}};
    end
    else begin
        set_s2 <= set_s1;
    end
end

// unpack X
wire [`DATA_WIDTH-1:0] x0 = hid_res_rd_data[7:0];
wire [`DATA_WIDTH-1:0] x1 = hid_res_rd_data[15:8];
wire [`DATA_WIDTH-1:0] x2 = hid_res_rd_data[23:16];
wire [`DATA_WIDTH-1:0] x3 = hid_res_rd_data[31:24];

// unpack whid 
wire [`DATA_WIDTH-1:0] w0 = wout_rd_data[7:0];
wire [`DATA_WIDTH-1:0] w1 = wout_rd_data[15:8];
wire [`DATA_WIDTH-1:0] w2 = wout_rd_data[23:16];

wire [`DATA_WIDTH*2-1:0] bias = 8'hFF*w0;
wire [`DATA_WIDTH*2-1:0] r0p1 = x0*w1;
wire [`DATA_WIDTH*2-1:0] r0p2 = x1*w2;
wire [`DATA_WIDTH*2-1:0] r1p1 = x2*w1;
wire [`DATA_WIDTH*2-1:0] r1p2 = x3*w2;


wire [`DATA_WIDTH*2-1:0] r0sum = bias + r0p1 + r0p2;
wire [`DATA_WIDTH*2-1:0] r1sum = bias + r1p1 + r1p2;

wire [`NUM_SAMPLES-1:0] idx0 = (set_s2<<1);
wire [`NUM_SAMPLES-1:0] idx1 = (set_s2<<1)+1;

wire eval0 = ((r0sum >> 8) > 8'd128)? 1'b1 : 1'b0;
wire eval1 = ((r1sum >> 8) > 8'd128)? 1'b1 : 1'b0;

// stage 3 compute products and write to res
always @(posedge clk) begin
    Done <= 1'b0;
    if (running) begin
        RES_reg[idx0] <= eval0;
        RES_reg[idx1] <= eval1;
        if (set_s2 >= NUM_SETS-1) begin
            Done <= 1'b1;
        end
    end
end



endmodule