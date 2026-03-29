`timescale 1ns / 1ps
// =============================================================================
//  mlp_accel.v  –  MLP inference engine
//
//  Architecture (all values in unsigned 0.8 fixed-point, scale = 256):
//    Hidden layer : N[64][2]     = X[64][8] × W_HID[2][8] / 256
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
//    N_RAM     : 128 entries – N[row*2+neuron]
//    N_sig_RAM : 128 entries – sigmoid(N)[row*2+neuron]
//    sig_lut   : 256 entries – sigmoid lookup table (ROM)
// =============================================================================

module mlp_accel (
    input  clk,
    input  Start,
    output reg Done,

    // X_RAM read (512 entries: row*8 + col, col in 0..7)
    output reg        x_rd_en,
    output reg [8:0]  x_rd_addr,
    input      [7:0]  x_rd_data,

    // W_HID_RAM read (16 entries: neuron*8 + k)
    output reg        whid_rd_en,
    output reg [3:0]  whid_rd_addr,
    input      [7:0]  whid_rd_data,

    // W_OUT_RAM read (3 entries: 0=bias, 1=hid0, 2=hid1)
    output reg        wout_rd_en,
    output reg [1:0]  wout_rd_addr,
    input      [7:0]  wout_rd_data,

    // RES_RAM write (64 entries)
    output reg        res_wr_en,
    output reg [5:0]  res_wr_addr,
    output reg [7:0]  res_wr_data
);

// ---------------------------------------------------------------------------
// Sigmoid LUT (256×8 ROM, initialised as constants – synthesises as LUT ROM)
// Values from sigmoid.csv: y = round(256 / (1 + exp(-(6x/256 - 3))))
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Internal intermediate storage (combinational/registered reads, no wait)
// ---------------------------------------------------------------------------
// N_RAM[row*2 + neuron]  – hidden layer pre-sigmoid (after divide-by-256)
reg [7:0] N_RAM     [0:127];
// N_sig_RAM[row*2 + neuron] – hidden layer post-sigmoid
reg [7:0] N_sig_RAM [0:127];

// ---------------------------------------------------------------------------
// FSM state encoding
// ---------------------------------------------------------------------------
localparam IDLE              = 5'd0;
// Phase 1: hidden layer  (for each row × neuron × k=0..7)
localparam HID_BIAS_REQ      = 5'd1;   // issue W_HID[neuron][0] read
localparam HID_BIAS_WAIT     = 5'd2;   // wait 1 cycle for RAM
localparam HID_BIAS_COMPUTE  = 5'd3;   // acc = bias_weight << 8, set k=1
localparam HID_READ_REQ      = 5'd4;   // issue X[row][k] & W_HID[neuron][k]
localparam HID_READ_WAIT     = 5'd5;   // wait 1 cycle
localparam HID_COMPUTE       = 5'd6;   // acc += x * w
localparam HID_WRITE         = 5'd7;   // N_RAM[row*2+neuron] = acc[15:8]
// Phase 2: sigmoid  (128 iterations, 1 cycle each – combinational LUT read)
localparam SIG_APPLY         = 5'd8;
// Phase 3: output layer  (for each row, k=1..2 after bias)
localparam OUT_BIAS_REQ      = 5'd9;
localparam OUT_BIAS_WAIT     = 5'd10;
localparam OUT_BIAS_COMPUTE  = 5'd11;  // acc = W_OUT[0] << 8, set k=1
localparam OUT_READ_REQ      = 5'd12;  // issue W_OUT[k], latch N_sig[row][k-1]
localparam OUT_READ_WAIT     = 5'd13;
localparam OUT_COMPUTE       = 5'd14;  // acc += nsig_latch * wout
localparam OUT_WRITE         = 5'd15;  // RES_RAM[row] = threshold(acc)
localparam DONE_STATE        = 5'd16;

reg [4:0] state;

// Counters
reg [5:0] row;      // 0..63
reg       neuron;   // 0..1
reg [2:0] k;        // 1..7 (hidden) or 1..2 (output)
reg [6:0] sig_idx;  // 0..127  (sigmoid phase counter)

// Accumulator – wide enough for: 256*255 + 7*255*255 = 520,455 < 2^20
reg [31:0] acc;

// Latched N_sig value for the output phase (combinational reg array read)
reg [7:0]  nsig_latch;

// ---------------------------------------------------------------------------
// Main FSM
// ---------------------------------------------------------------------------
always @(posedge clk) begin
    // ── Defaults: deassert all control outputs each cycle ──────────────────
    Done       <= 1'b0;
    res_wr_en  <= 1'b0;
    x_rd_en    <= 1'b0;
    whid_rd_en <= 1'b0;
    wout_rd_en <= 1'b0;

    case (state)

        // ─────────────────────────────────────────────────────────────────
        IDLE: begin
            row     <= 6'd0;
            neuron  <= 1'b0;
            k       <= 3'd1;
            acc     <= 32'd0;
            sig_idx <= 7'd0;
            if (Start)
                state <= HID_BIAS_REQ;
        end

        // ═══════════════════════════════════════════════════════════════════
        //  PHASE 1 – HIDDEN LAYER
        //  For each (row, neuron):
        //    acc  = W_HID[neuron][0] << 8           (bias: X[:,0] = 256)
        //    acc += X[row][k] * W_HID[neuron][k]    (k = 1..7)
        //    N_RAM[row*2+neuron] = acc[15:8]
        // ═══════════════════════════════════════════════════════════════════

        HID_BIAS_REQ: begin
            // Issue synchronous read for W_HID[neuron][0] (bias weight)
            // W_HID_RAM address = neuron*8 + 0 = {neuron, 3'd0}
            whid_rd_en   <= 1'b1;
            whid_rd_addr <= {neuron, 3'd0};
            state        <= HID_BIAS_WAIT;
        end

        HID_BIAS_WAIT: begin
            whid_rd_en <= 1'b1;   // keep asserted so RAM holds address
            state      <= HID_BIAS_COMPUTE;
        end

        HID_BIAS_COMPUTE: begin
            // acc = bias_weight * 256  (X[row][0] is always 256 in fixed-point)
            // whid_rd_data is valid now
            acc   <= {16'b0, whid_rd_data, 8'b0};  // = whid_rd_data << 8
            k     <= 3'd1;
            state <= HID_READ_REQ;
        end

        HID_READ_REQ: begin
            // Issue reads for X[row][k] and W_HID[neuron][k]
            // X_RAM address    = {row[5:0], k[2:0]} = row*8 + k
            // WHID_RAM address = {neuron,  k[2:0]}  = neuron*8 + k
            x_rd_en      <= 1'b1;
            x_rd_addr    <= {row, k};
            whid_rd_en   <= 1'b1;
            whid_rd_addr <= {neuron, k};
            state        <= HID_READ_WAIT;
        end

        HID_READ_WAIT: begin
            x_rd_en    <= 1'b1;
            whid_rd_en <= 1'b1;
            state      <= HID_COMPUTE;
        end

        HID_COMPUTE: begin
            // acc += X[row][k] * W_HID[neuron][k]   (unsigned 8×8 multiply)
            acc <= acc + ({24'b0, x_rd_data} * {24'b0, whid_rd_data});
            if (k == 3'd7) begin
                state <= HID_WRITE;
            end else begin
                k     <= k + 1'b1;
                state <= HID_READ_REQ;   // fetch next column
            end
        end

        HID_WRITE: begin
            // N_RAM[row*2 + neuron] = acc >> 8 = acc[15:8]
            // {row, neuron} gives a 7-bit index = row*2 + neuron
            N_RAM[{row, neuron}] <= acc[15:8];
            acc <= 32'd0;

            if (neuron == 1'b0) begin
                neuron <= 1'b1;
                state  <= HID_BIAS_REQ;   // compute neuron 1 for same row
            end else begin
                neuron <= 1'b0;
                if (row == 6'd63) begin
                    // All 64×2 hidden values computed; start sigmoid phase
                    sig_idx <= 7'd0;
                    state   <= SIG_APPLY;
                end else begin
                    row   <= row + 1'b1;
                    state <= HID_BIAS_REQ;
                end
            end
        end

        // ═══════════════════════════════════════════════════════════════════
        //  PHASE 2 – SIGMOID (128 cycles, 1 per entry)
        //  N_sig_RAM is an internal reg array → combinational read of N_RAM
        //  is safe here (no synchronous RAM wait needed).
        // ═══════════════════════════════════════════════════════════════════

        SIG_APPLY: begin
            // N_sig_RAM[i] = sig_lut[ N_RAM[i] ]
            // Both reads are combinational (reg arrays), result registered.
            N_sig_RAM[sig_idx] <= sig_lut[N_RAM[sig_idx]];

            if (sig_idx == 7'd127) begin
                row   <= 6'd0;
                state <= OUT_BIAS_REQ;
            end else begin
                sig_idx <= sig_idx + 1'b1;
            end
        end

        // ═══════════════════════════════════════════════════════════════════
        //  PHASE 3 – OUTPUT LAYER
        //  For each row:
        //    acc  = W_OUT[0] << 8                  (bias: N_sig bias = 256)
        //    acc += N_sig_RAM[row*2+0] * W_OUT[1]  (k=1)
        //    acc += N_sig_RAM[row*2+1] * W_OUT[2]  (k=2)
        //    RES_RAM[row] = (acc[15:8] > 128) ? 1 : 0
        // ═══════════════════════════════════════════════════════════════════

        OUT_BIAS_REQ: begin
            wout_rd_en   <= 1'b1;
            wout_rd_addr <= 2'd0;   // W_OUT[0] = bias weight (80)
            state        <= OUT_BIAS_WAIT;
        end

        OUT_BIAS_WAIT: begin
            wout_rd_en <= 1'b1;
            state      <= OUT_BIAS_COMPUTE;
        end

        OUT_BIAS_COMPUTE: begin
            acc   <= {16'b0, wout_rd_data, 8'b0};  // W_OUT[0] << 8
            k     <= 3'd1;
            state <= OUT_READ_REQ;
        end

        OUT_READ_REQ: begin
            // Read W_OUT[k] from external RAM (synchronous)
            wout_rd_en   <= 1'b1;
            wout_rd_addr <= k[1:0];   // k is 1 or 2

            // Latch N_sig_RAM[row*2 + (k-1)] combinationally now:
            //   k=1 → addr = row*2+0  (neuron 0 sigmoid output)
            //   k=2 → addr = row*2+1  (neuron 1 sigmoid output)
            nsig_latch <= (k == 3'd1) ? N_sig_RAM[{row, 1'b0}]
                                      : N_sig_RAM[{row, 1'b1}];
            state <= OUT_READ_WAIT;
        end

        OUT_READ_WAIT: begin
            wout_rd_en <= 1'b1;
            state      <= OUT_COMPUTE;
        end

        OUT_COMPUTE: begin
            // acc += N_sig_latch * W_OUT[k]
            acc <= acc + ({24'b0, nsig_latch} * {24'b0, wout_rd_data});
            if (k == 3'd2) begin
                state <= OUT_WRITE;
            end else begin
                k     <= k + 1'b1;
                state <= OUT_READ_REQ;
            end
        end

        OUT_WRITE: begin
            // Threshold: prediction = (acc[15:8] > 128) ? 1 : 0
            res_wr_en   <= 1'b1;
            res_wr_addr <= row;
            res_wr_data <= (acc[15:8] > 8'd128) ? 8'd1 : 8'd0;
            acc         <= 32'd0;

            if (row == 6'd63) begin
                state <= DONE_STATE;
            end else begin
                row   <= row + 1'b1;
                state <= OUT_BIAS_REQ;
            end
        end

        // ─────────────────────────────────────────────────────────────────
        DONE_STATE: begin
            Done  <= 1'b1;
            state <= IDLE;
        end

        default: state <= IDLE;

    endcase
end

endmodule
