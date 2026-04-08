`timescale 1ns / 1ps
`include "sharedparams.vh"

module hid_compute
	(
		input                                   clk,										
		input                                   Start,										
		output reg                              Done,									
		
        output reg                              x_rd_en,
        output reg [`X_DEPTH_BITS-1:0]          x_rd_addr,
        input      [`AXI_DATA_WIDTH-1:0]        x_rd_data,

		output reg                              whid_rd_en,
        output reg [`WHID_DEPTH_BITS-1:0]       whid_rd_addr,
        input      [`AXI_DATA_WIDTH-1:0]        whid_rd_data,

		output reg                              hid_res_wr_en,
        output reg [`HID_RES_DEPTH_BITS-1:0]    hid_res_wr_addr,
        output reg [`AXI_DATA_WIDTH-1:0]        hid_res_wr_data
	);

reg [7:0] sig_lut [0:255];
initial begin
    sig_lut[0]=8'd12;  sig_lut[1]=8'd12;  sig_lut[2]=8'd12;  sig_lut[3]=8'd12;
    sig_lut[4]=8'd13;  sig_lut[5]=8'd13;  sig_lut[6]=8'd13;  sig_lut[7]=8'd14;
    sig_lut[8]=8'd14;  sig_lut[9]=8'd14;  sig_lut[10]=8'd15; sig_lut[11]=8'd15;
    sig_lut[12]=8'd15; sig_lut[13]=8'd16; sig_lut[14]=8'd16; sig_lut[15]=8'd16;
    sig_lut[16]=8'd17; sig_lut[17]=8'd17; sig_lut[18]=8'd18; sig_lut[19]=8'd18;
    sig_lut[20]=8'd18; sig_lut[21]=8'd19; sig_lut[22]=8'd19; sig_lut[23]=8'd20;
    sig_lut[24]=8'd20; sig_lut[25]=8'd21; sig_lut[26]=8'd21; sig_lut[27]=8'd21;
    sig_lut[28]=8'd22; sig_lut[29]=8'd22; sig_lut[30]=8'd23; sig_lut[31]=8'd23;
    sig_lut[32]=8'd24; sig_lut[33]=8'd24; sig_lut[34]=8'd25; sig_lut[35]=8'd26;
    sig_lut[36]=8'd26; sig_lut[37]=8'd27; sig_lut[38]=8'd27; sig_lut[39]=8'd28;
    sig_lut[40]=8'd28; sig_lut[41]=8'd29; sig_lut[42]=8'd30; sig_lut[43]=8'd30;
    sig_lut[44]=8'd31; sig_lut[45]=8'd32; sig_lut[46]=8'd32; sig_lut[47]=8'd33;
    sig_lut[48]=8'd34; sig_lut[49]=8'd34; sig_lut[50]=8'd35; sig_lut[51]=8'd36;
    sig_lut[52]=8'd36; sig_lut[53]=8'd37; sig_lut[54]=8'd38; sig_lut[55]=8'd39;
    sig_lut[56]=8'd39; sig_lut[57]=8'd40; sig_lut[58]=8'd41; sig_lut[59]=8'd42;
    sig_lut[60]=8'd43; sig_lut[61]=8'd44; sig_lut[62]=8'd44; sig_lut[63]=8'd45;
    sig_lut[64]=8'd46; sig_lut[65]=8'd47; sig_lut[66]=8'd48; sig_lut[67]=8'd49;
    sig_lut[68]=8'd50; sig_lut[69]=8'd51; sig_lut[70]=8'd52; sig_lut[71]=8'd53;
    sig_lut[72]=8'd54; sig_lut[73]=8'd55; sig_lut[74]=8'd56; sig_lut[75]=8'd57;
    sig_lut[76]=8'd58; sig_lut[77]=8'd59; sig_lut[78]=8'd60; sig_lut[79]=8'd61;
    sig_lut[80]=8'd62; sig_lut[81]=8'd63; sig_lut[82]=8'd64; sig_lut[83]=8'd66;
    sig_lut[84]=8'd67; sig_lut[85]=8'd68; sig_lut[86]=8'd69; sig_lut[87]=8'd70;
    sig_lut[88]=8'd72; sig_lut[89]=8'd73; sig_lut[90]=8'd74; sig_lut[91]=8'd75;
    sig_lut[92]=8'd76; sig_lut[93]=8'd78; sig_lut[94]=8'd79; sig_lut[95]=8'd80;
    sig_lut[96]=8'd82; sig_lut[97]=8'd83; sig_lut[98]=8'd84; sig_lut[99]=8'd86;
    sig_lut[100]=8'd87;  sig_lut[101]=8'd88;  sig_lut[102]=8'd90;  sig_lut[103]=8'd91;
    sig_lut[104]=8'd92;  sig_lut[105]=8'd94;  sig_lut[106]=8'd95;  sig_lut[107]=8'd97;
    sig_lut[108]=8'd98;  sig_lut[109]=8'd99;  sig_lut[110]=8'd101; sig_lut[111]=8'd102;
    sig_lut[112]=8'd104; sig_lut[113]=8'd105; sig_lut[114]=8'd107; sig_lut[115]=8'd108;
    sig_lut[116]=8'd110; sig_lut[117]=8'd111; sig_lut[118]=8'd113; sig_lut[119]=8'd114;
    sig_lut[120]=8'd116; sig_lut[121]=8'd117; sig_lut[122]=8'd119; sig_lut[123]=8'd120;
    sig_lut[124]=8'd122; sig_lut[125]=8'd123; sig_lut[126]=8'd125; sig_lut[127]=8'd126;
    sig_lut[128]=8'd128; sig_lut[129]=8'd129; sig_lut[130]=8'd130; sig_lut[131]=8'd132;
    sig_lut[132]=8'd133; sig_lut[133]=8'd135; sig_lut[134]=8'd136; sig_lut[135]=8'd138;
    sig_lut[136]=8'd139; sig_lut[137]=8'd141; sig_lut[138]=8'd142; sig_lut[139]=8'd144;
    sig_lut[140]=8'd145; sig_lut[141]=8'd147; sig_lut[142]=8'd148; sig_lut[143]=8'd150;
    sig_lut[144]=8'd151; sig_lut[145]=8'd153; sig_lut[146]=8'd154; sig_lut[147]=8'd156;
    sig_lut[148]=8'd157; sig_lut[149]=8'd158; sig_lut[150]=8'd160; sig_lut[151]=8'd161;
    sig_lut[152]=8'd163; sig_lut[153]=8'd164; sig_lut[154]=8'd165; sig_lut[155]=8'd167;
    sig_lut[156]=8'd168; sig_lut[157]=8'd169; sig_lut[158]=8'd171; sig_lut[159]=8'd172;
    sig_lut[160]=8'd173; sig_lut[161]=8'd175; sig_lut[162]=8'd176; sig_lut[163]=8'd177;
    sig_lut[164]=8'd179; sig_lut[165]=8'd180; sig_lut[166]=8'd181; sig_lut[167]=8'd182;
    sig_lut[168]=8'd183; sig_lut[169]=8'd185; sig_lut[170]=8'd186; sig_lut[171]=8'd187;
    sig_lut[172]=8'd188; sig_lut[173]=8'd189; sig_lut[174]=8'd191; sig_lut[175]=8'd192;
    sig_lut[176]=8'd193; sig_lut[177]=8'd194; sig_lut[178]=8'd195; sig_lut[179]=8'd196;
    sig_lut[180]=8'd197; sig_lut[181]=8'd198; sig_lut[182]=8'd199; sig_lut[183]=8'd200;
    sig_lut[184]=8'd201; sig_lut[185]=8'd202; sig_lut[186]=8'd203; sig_lut[187]=8'd204;
    sig_lut[188]=8'd205; sig_lut[189]=8'd206; sig_lut[190]=8'd207; sig_lut[191]=8'd208;
    sig_lut[192]=8'd209; sig_lut[193]=8'd210; sig_lut[194]=8'd211; sig_lut[195]=8'd211;
    sig_lut[196]=8'd212; sig_lut[197]=8'd213; sig_lut[198]=8'd214; sig_lut[199]=8'd215;
    sig_lut[200]=8'd216; sig_lut[201]=8'd216; sig_lut[202]=8'd217; sig_lut[203]=8'd218;
    sig_lut[204]=8'd219; sig_lut[205]=8'd219; sig_lut[206]=8'd220; sig_lut[207]=8'd221;
    sig_lut[208]=8'd221; sig_lut[209]=8'd222; sig_lut[210]=8'd223; sig_lut[211]=8'd223;
    sig_lut[212]=8'd224; sig_lut[213]=8'd225; sig_lut[214]=8'd225; sig_lut[215]=8'd226;
    sig_lut[216]=8'd227; sig_lut[217]=8'd227; sig_lut[218]=8'd228; sig_lut[219]=8'd228;
    sig_lut[220]=8'd229; sig_lut[221]=8'd229; sig_lut[222]=8'd230; sig_lut[223]=8'd231;
    sig_lut[224]=8'd231; sig_lut[225]=8'd232; sig_lut[226]=8'd232; sig_lut[227]=8'd233;
    sig_lut[228]=8'd233; sig_lut[229]=8'd234; sig_lut[230]=8'd234; sig_lut[231]=8'd234;
    sig_lut[232]=8'd235; sig_lut[233]=8'd235; sig_lut[234]=8'd236; sig_lut[235]=8'd236;
    sig_lut[236]=8'd237; sig_lut[237]=8'd237; sig_lut[238]=8'd237; sig_lut[239]=8'd238;
    sig_lut[240]=8'd238; sig_lut[241]=8'd239; sig_lut[242]=8'd239; sig_lut[243]=8'd239;
    sig_lut[244]=8'd240; sig_lut[245]=8'd240; sig_lut[246]=8'd240; sig_lut[247]=8'd241;
    sig_lut[248]=8'd241; sig_lut[249]=8'd241; sig_lut[250]=8'd242; sig_lut[251]=8'd242;
    sig_lut[252]=8'd242; sig_lut[253]=8'd243; sig_lut[254]=8'd243; sig_lut[255]=8'd243;
end

    localparam WORDS_PER_ROW    = (`NUM_FEATURES / `AXI_PACKET_SIZE);
    localparam ROW_DEPTH_BITS   = $clog2(`NUM_SAMPLES);
    localparam SET_DEPTH_BITS   = $clog2(WORDS_PER_ROW);
    localparam HID_RES_BITS     = $clog2(`HID_RES_SIZE);

    reg [`DATA_WIDTH-1:0] hid_res [0:`HID_RES_SIZE - 1];

    reg  [`DATA_WIDTH*2-1:0] out;
    
    reg running;
    reg [ROW_DEPTH_BITS-1:0]    row_s1;
    reg [SET_DEPTH_BITS-1:0]   set_s1;
    reg neuron_s1;
    
    wire [ROW_DEPTH_BITS-1:0] row_s1_out = (set_s1 == 1'b1) ? row_s1 + 1 : row_s1;
    wire [SET_DEPTH_BITS-1:0] set_s1_out = set_s1 + 1'b1;
    wire neuron_s1_out = (row_s1 == 6'd63 && set_s1 == 1'b1) ? neuron_s1 + 1 : neuron_s1;

    // stage 1 set read enable
    always @(posedge clk) begin
        if (Start) begin
            x_rd_en <= 1'b1;
            whid_rd_en <= 1'b1;
            row_s1 <= {ROW_DEPTH_BITS{1'b0}};
            set_s1 <= {SET_DEPTH_BITS{1'b0}};
            neuron_s1 <= 1'b0;
            x_rd_addr <= {`X_DEPTH_BITS{1'b0}};
            whid_rd_addr <= {`WHID_DEPTH_BITS{1'b0}};
            running <= 1'b1;
        end
        else if (running) begin
            set_s1 <= set_s1_out;
            row_s1 <= row_s1_out;
            neuron_s1 <= neuron_s1_out;
            x_rd_en <= !(set_s1 == 1'b1 && row_s1 == 6'd63 && neuron_s1 == 1'b1);
            whid_rd_en <= !(set_s1 == 1'b1 && row_s1 == 6'd63 && neuron_s1 == 1'b1);
            x_rd_addr <= row_s1_out * 2 + set_s1_out;
            whid_rd_addr <= neuron_s1_out * 2 + set_s1_out;
        end
        else begin
            x_rd_en <= 1'b0;
            whid_rd_en <= 1'b0;
            row_s1 <= {ROW_DEPTH_BITS{1'b0}};
            set_s1 <= {SET_DEPTH_BITS{1'b0}};
            neuron_s1 <= 1'b0;
            x_rd_addr <= {`X_DEPTH_BITS{1'b0}};
            whid_rd_addr <= {`WHID_DEPTH_BITS{1'b0}};
        end
        if (Done) begin
            running <= 1'b0;
        end    
    end
        
    reg [ROW_DEPTH_BITS-1:0]    row_s2;
    reg [SET_DEPTH_BITS-1:0]   set_s2;
    reg neuron_s2;

    // stage 2 wait for outputs
    always @(posedge clk) begin
        if (!running) begin
            row_s2 <= {ROW_DEPTH_BITS{1'b0}};
            set_s2 <= {SET_DEPTH_BITS{1'b0}};
            neuron_s2 <= 1'b0;
        end
        else begin
            row_s2 <= row_s1;
            set_s2 <= set_s1;
            neuron_s2 <= neuron_s1;
        end
    end

    // unpack X
    wire [`DATA_WIDTH-1:0] x0 = x_rd_data[7:0];
    wire [`DATA_WIDTH-1:0] x1 = x_rd_data[15:8];
    wire [`DATA_WIDTH-1:0] x2 = x_rd_data[23:16];
    wire [`DATA_WIDTH-1:0] x3 = x_rd_data[31:24];

    // unpack whid 
    wire [`DATA_WIDTH-1:0] w0 = whid_rd_data[7:0];
    wire [`DATA_WIDTH-1:0] w1 = whid_rd_data[15:8];
    wire [`DATA_WIDTH-1:0] w2 = whid_rd_data[23:16];
    wire [`DATA_WIDTH-1:0] w3 = whid_rd_data[31:24];

    wire [`DATA_WIDTH*2-1:0] p0 = x0*w0;
    wire [`DATA_WIDTH*2-1:0] p1 = x1*w1;
    wire [`DATA_WIDTH*2-1:0] p2 = x2*w2;
    wire [`DATA_WIDTH*2-1:0] p3 = x3*w3;

    wire [`DATA_WIDTH*2-1:0] sum = p0 + p1 + p2 + p3;

    reg  [`DATA_WIDTH*2-1:0] acc;
    reg [ROW_DEPTH_BITS-1:0]   row_s3;
    reg [SET_DEPTH_BITS-1:0]   set_s3;
    reg neuron_s3;

    // stage 3 compute products
    always @(posedge clk) begin
        if (!running) begin
            acc <= {`DATA_WIDTH*2{1'b0}};
            row_s3 <= {ROW_DEPTH_BITS{1'b0}};
            set_s3 <= {SET_DEPTH_BITS{1'b0}};
            neuron_s3 <= 1'b0;
        end
        else begin
            row_s3 <= row_s2;
            set_s3 <= set_s2;
            neuron_s3 <= neuron_s2;
            acc <= (set_s2 == 0)? sum : acc + sum;
        end
    end
    wire [`DATA_WIDTH-1:0] res = sig_lut[acc >> 8];
    
    wire [HID_RES_BITS - 1:0] idx3 = (row_s3)<<1;
    wire [HID_RES_BITS - 1 + 1:0] idx2 = ((row_s3 - 1)<<1) + 1;
    wire [HID_RES_BITS - 1 + 1:0] idx1 = (row_s3 - 1)<<1;
    
    wire [`DATA_WIDTH-1:0] res3 = hid_res[idx3];
    wire [`DATA_WIDTH-1:0] res2 = hid_res[idx2];
    wire [`DATA_WIDTH-1:0] res1 = hid_res[idx1];
    
    // stage 4 write outputs
    always @(posedge clk) begin
        Done <= 1'b0;
        if (running) begin
            if (set_s3 >= 1'b1) begin
                hid_res[row_s3 * 2 + neuron_s3]  <= res;
            end
            // check if we are computing the last neuron (means we would have 2 columns of data)
            // and we are on an even row (for packing the 4 data values into a single 32 bit word)
            if (neuron_s3 >= 1'b1 && row_s3[0] == 1'b1) begin 
                hid_res_wr_en <= 1'b1;
                hid_res_wr_addr <= (row_s3 >> 1);
                hid_res_wr_data <= {res, res3, res2, res1};
                if (row_s3 >= 63) begin
                    Done <= 1'b1;
                end
            end
        end
        else begin
            hid_res_wr_en <= 1'b0;
        end
    end

endmodule