`timescale 1ns / 1ps
/*
----------------------------------------------------------------------------------
--  (c) Rajesh C Panicker, NUS  (template)
--  Modified for EE4218 MLP coprocessor project
--  Description : Self-checking testbench for MLP AXI-Stream Coprocessor
--                531 input words (X + W_HID + W_OUT), 64 output words
----------------------------------------------------------------------------------
*/

module tb_myip_v1_0();

    reg                    ACLK = 0;
    reg                    ARESETN;
    wire                   S_AXIS_TREADY;
    reg  [31:0]            S_AXIS_TDATA;
    reg                    S_AXIS_TLAST;
    reg                    S_AXIS_TVALID;
    wire                   M_AXIS_TVALID;
    wire [31:0]            M_AXIS_TDATA;
    wire                   M_AXIS_TLAST;
    reg                    M_AXIS_TREADY;

    myip_v1_0 U1 (
        .ACLK(ACLK),
        .ARESETN(ARESETN),
        .S_AXIS_TREADY(S_AXIS_TREADY),
        .S_AXIS_TDATA(S_AXIS_TDATA),
        .S_AXIS_TLAST(S_AXIS_TLAST),
        .S_AXIS_TVALID(S_AXIS_TVALID),
        .M_AXIS_TVALID(M_AXIS_TVALID),
        .M_AXIS_TDATA(M_AXIS_TDATA),
        .M_AXIS_TLAST(M_AXIS_TLAST),
        .M_AXIS_TREADY(M_AXIS_TREADY)
    );

    // ── Test parameters ─────────────────────────────────────────────────────
    localparam NUMBER_OF_INPUT_WORDS  = 531;  // 512 (X) + 16 (W_HID) + 3 (W_OUT)
    localparam NUMBER_OF_OUTPUT_WORDS = 64;
    localparam NUMBER_OF_TEST_VECTORS = 1;
    localparam width = 8;

    reg [width-1:0] test_input_memory
        [0:NUMBER_OF_TEST_VECTORS*NUMBER_OF_INPUT_WORDS-1];
    reg [width-1:0] test_result_expected_memory
        [0:NUMBER_OF_TEST_VECTORS*NUMBER_OF_OUTPUT_WORDS-1];
    reg [width-1:0] result_memory
        [0:NUMBER_OF_TEST_VECTORS*NUMBER_OF_OUTPUT_WORDS-1];

    integer word_cnt, test_case_cnt;
    reg success = 1'b1;
    reg M_AXIS_TLAST_prev = 1'b0;

    always @(posedge ACLK)
        M_AXIS_TLAST_prev <= M_AXIS_TLAST;

    always #50 ACLK = ~ACLK;   // 10 MHz clock, period = 100 ns

    initial begin
        $display("Loading Memory...");
        $readmemh("test_input_new.mem",          test_input_memory);
        $readmemh("test_result_expected_new.mem", test_result_expected_memory);

        #25
        ARESETN      = 1'b0;
        S_AXIS_TVALID = 1'b0;
        S_AXIS_TLAST  = 1'b0;
        M_AXIS_TREADY = 1'b0;

        #100
        ARESETN = 1'b1;

        for (test_case_cnt = 0;
             test_case_cnt < NUMBER_OF_TEST_VECTORS;
             test_case_cnt = test_case_cnt + 1) begin

            // ── Send input stream ──────────────────────────────────────────
            word_cnt = 0;
            S_AXIS_TVALID = 1'b1;

            while (word_cnt < NUMBER_OF_INPUT_WORDS) begin
                if (S_AXIS_TREADY) begin
                    S_AXIS_TDATA = test_input_memory[
                        word_cnt + test_case_cnt * NUMBER_OF_INPUT_WORDS];
                    S_AXIS_TLAST = (word_cnt == NUMBER_OF_INPUT_WORDS - 1)
                                   ? 1'b1 : 1'b0;
                    word_cnt = word_cnt + 1;
                end
                #100;
            end
            S_AXIS_TVALID = 1'b0;
            S_AXIS_TLAST  = 1'b0;

            // ── Receive output stream ──────────────────────────────────────
            word_cnt = 0;
            M_AXIS_TREADY = 1'b1;

            while (M_AXIS_TLAST | ~M_AXIS_TLAST_prev) begin
                if (M_AXIS_TVALID) begin
                    result_memory[word_cnt + test_case_cnt * NUMBER_OF_OUTPUT_WORDS]
                        = M_AXIS_TDATA;
                    word_cnt = word_cnt + 1;
                end
                #100;
            end
            M_AXIS_TREADY = 1'b0;

        end  // for each test vector

        // ── Check results ──────────────────────────────────────────────────
        for (word_cnt = 0;
             word_cnt < NUMBER_OF_TEST_VECTORS * NUMBER_OF_OUTPUT_WORDS;
             word_cnt = word_cnt + 1) begin
            success = success &
                (result_memory[word_cnt] == test_result_expected_memory[word_cnt]);
        end

        if (success)
            $display("Test Passed.");
        else
            $display("Test Failed.");

        $finish;
    end

endmodule
