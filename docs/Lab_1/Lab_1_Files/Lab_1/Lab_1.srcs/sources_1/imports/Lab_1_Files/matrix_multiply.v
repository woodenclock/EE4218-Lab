`timescale 1ns / 1ps
`include "sharedparams.vh"

module matrix_multiply
    #(  parameter width        = `WIDTH,
        parameter A_depth_bits = 3,
        parameter B_depth_bits = 2,
        parameter RES_depth_bits = 1
    )
    (
        input clk,
        input Start,
        output reg Done,

        output reg                       A_read_en,
        output reg [A_depth_bits-1:0]    A_read_address,
        input      [width-1:0]           A_read_data_out,

        output reg                       B_read_en,
        output reg [B_depth_bits-1:0]    B_read_address,
        input      [width-1:0]           B_read_data_out,

        output reg                       RES_write_en,
        output reg [RES_depth_bits-1:0]  RES_write_address,
        output reg [width-1:0]           RES_write_data_in
    );

    localparam integer M = (1 << RES_depth_bits);
    localparam integer N = (1 << B_depth_bits);

    localparam IDLE    = 3'd0;
    localparam WAIT    = 3'd1;
    localparam COMPUTE = 3'd2;
    localparam WRITE   = 3'd3;
    localparam DONE    = 3'd4;

    reg [2:0]              state;
    reg [RES_depth_bits-1:0] row;
    reg [B_depth_bits-1:0]   col;

    reg  [width*2-1:0] acc;
    wire [width*2-1:0] prod     = A_read_data_out * B_read_data_out;
    wire [width*2-1:0] acc_next = acc + prod;
    reg  [width-1:0]   row_result_byte;

    always @(posedge clk) begin
        Done         <= 1'b0;
        RES_write_en <= 1'b0;

        case (state)

            // ----------------------------------------------------------------
            IDLE: begin
                A_read_en         <= 1'b0;
                B_read_en         <= 1'b0;
                A_read_address    <= {A_depth_bits{1'b0}};
                B_read_address    <= {B_depth_bits{1'b0}};
                RES_write_address <= {RES_depth_bits{1'b0}};
                RES_write_data_in <= {width{1'b0}};
                row <= {RES_depth_bits{1'b0}};
                col <= {B_depth_bits{1'b0}};
                acc <= {(2*width){1'b0}};

                if (Start) begin
                    A_read_en      <= 1'b1;
                    B_read_en      <= 1'b1;
                    // Drive addr(row=0, col=0) — RAM latches it at posedge WAIT
                    A_read_address <= {A_depth_bits{1'b0}};
                    B_read_address <= {B_depth_bits{1'b0}};
                    state          <= WAIT;
                end
            end

            // ----------------------------------------------------------------
            // WAIT absorbs the first read latency AND pre-issues addr(row,1).
            //
            //   posedge WAIT   : RAM latches addr(row,0)  → data valid at COMPUTE(col=0) ✓
            //   After WAIT NB  : addr = addr(row,1)
            //   posedge COMPUTE(0): RAM latches addr(row,1) → data valid at COMPUTE(col=1) ✓
            WAIT: begin
                A_read_en      <= 1'b1;
                B_read_en      <= 1'b1;
                A_read_address <= (row << B_depth_bits) + 1;  // addr(row, 1)
                B_read_address <= 1;
                col            <= {B_depth_bits{1'b0}};
                state          <= COMPUTE;
            end

            // ----------------------------------------------------------------
            // Pipelined MAC loop — no WAIT between columns.
            //
            // At entry: read_data_out is valid for current `col`.
            // For col < N-1: issue addr(row, col+2) so that:
            //   posedge COMPUTE(col+1) : RAM latches addr(row, col+2)
            //                           → data valid at COMPUTE(col+2) ✓
            COMPUTE: begin
                if (col == N - 1) begin
                    // ---- Last column: latch result, stop reads ----
                    row_result_byte <= acc_next[15:8];
                    acc             <= {(2*width){1'b0}};
                    A_read_en       <= 1'b0;
                    B_read_en       <= 1'b0;
                    state           <= WRITE;
                end else begin
                    // ---- Not last: accumulate + issue two-ahead address ----
                    acc            <= acc_next;
                    col            <= col + 1'b1;
                    A_read_address <= (row << B_depth_bits) + col + 2;
                    B_read_address <= col + 2;
                    A_read_en      <= 1'b1;
                    B_read_en      <= 1'b1;
                    state          <= COMPUTE;   // ← straight back, no WAIT
                end
            end

            // ----------------------------------------------------------------
            WRITE: begin
                RES_write_en      <= 1'b1;
                RES_write_address <= row;
                RES_write_data_in <= row_result_byte;

                if (row == M - 1) begin
                    Done  <= 1'b1;
                    state <= DONE;
                end else begin
                    row            <= row + 1'b1;
                    col            <= {B_depth_bits{1'b0}};
                    acc            <= {(2*width){1'b0}};
                    // Drive addr(row+1, col=0) — RAM latches it at posedge WAIT
                    A_read_en      <= 1'b1;
                    B_read_en      <= 1'b1;
                    A_read_address <= ((row + 1'b1) << B_depth_bits);
                    B_read_address <= {B_depth_bits{1'b0}};
                    state          <= WAIT;
                end
            end

            // ----------------------------------------------------------------
            DONE: begin
                Done  <= 1'b1;
                state <= IDLE;
            end

            default: state <= IDLE;

        endcase
    end

endmodule


//## Timing Diagram (N=8)
//```
//Cycle:        IDLE  WAIT  C(0)  C(1)  C(2)  C(3)  C(4)  C(5)  C(6)  C(7)  WRITE
//              ─────────────────────────────────────────────────────────────────────
//addr on wire:  [0]  [1]   [2]   [3]   [4]   [5]   [6]   [7]   [8*]   -     -
//RAM latches:        [0]   [1]   [2]   [3]   [4]   [5]   [6]   [7]    -     -
//data valid:          -    [0]✓  [1]✓  [2]✓  [3]✓  [4]✓  [5]✓  [6]✓  [7]✓