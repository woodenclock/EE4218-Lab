/*
----------------------------------------------------------------------------------
--	(c) Rajesh C Panicker, NUS,
--  Description : AXI Stream Coprocessor (HLS), implementing the sum of 4 numbers
--	License terms :
--	You are free to use this code as long as you
--		(i) DO NOT post a modified version of this on any public repository;
--		(ii) use it only for educational purposes;
--		(iii) accept the responsibility to ensure that your implementation does not violate any intellectual property of any entity.
--		(iv) accept that the program is provided "as is" without warranty of any kind or assurance regarding its suitability for any particular purpose;
--		(v) send an email to rajesh.panicker@ieee.org briefly mentioning its use (except when used for the course EE4218/CEG5203 at the National University of Singapore);
--		(vi) retain this notice in this file or any files derived from this.
----------------------------------------------------------------------------------
*/

#include "hls_stream.h"
#include "ap_int.h"
#include "ap_axi_sdata.h"

#include <stdint.h>

#define A_ROWS 64
#define A_COLS 8
#define B_ROWS A_COLS
#define B_COLS 1
#define RES_ROWS A_ROWS
#define RES_COLS B_COLS

#define NUMBER_OF_INPUT_A (A_ROWS * A_COLS)
#define NUMBER_OF_INPUT_B (B_ROWS * B_COLS) 
#define NUMBER_OF_OUTPUT_WORDS (RES_ROWS * RES_COLS)  

// ACLK, ARESETN, TREADY, TDATA, TVALID are essential signals for AXIS. New version of AXI DMA seems to expect TSTRB and/or TKEEP as well.

typedef ap_axis<32,0,0,0> AXIS;  //data, user, id, dest

void myip_v1_0_HLS(hls::stream<AXIS>& S_AXIS, hls::stream<AXIS>& M_AXIS){
#pragma HLS INTERFACE ap_ctrl_none port=return
#pragma HLS INTERFACE axis port=S_AXIS
#pragma HLS INTERFACE axis port=M_AXIS

	int A_IN[A_ROWS][A_COLS];
	int B_IN[B_ROWS][B_COLS];
	int RES_OUT[RES_ROWS][RES_COLS];

	#pragma HLS array_partition variable=A_IN
	#pragma HLS array_partition variable=B_IN
	#pragma HLS array_partition variable=RES_OUT

	int word_cnt;
	//ap_uint<8> sum = 0; // using arbitrary precision
	//int sum = 0;		 // using 32 bit precision
	AXIS read_input, write_output;

		//read A matrix input
		myip_v1_0_HLS_forA:for(word_cnt = 0; word_cnt < NUMBER_OF_INPUT_A; word_cnt++){
			read_input = S_AXIS.read();
			A_IN[word_cnt / A_COLS][word_cnt % A_COLS] = read_input.data;
		}

		//read B matrix input
		myip_v1_0_HLS_forB:for(word_cnt = 0; word_cnt < NUMBER_OF_INPUT_B; word_cnt++){
			read_input = S_AXIS.read();
			B_IN[word_cnt / B_COLS][word_cnt % B_COLS] = read_input.data;
		}

		//matrix multiplication
		myip_v1_0_HLS_forMult:for(int i = 0; i < A_ROWS; i++){
		#pragma HLS UNROLL
			for (int k = 0; k < B_COLS; k++){
				int32_t acc = 0;
				for (int j = 0; j < A_COLS; j++){
					acc += (int32_t)A_IN[i][j] * (int32_t)B_IN[j][k];
				}
			RES_OUT[i][k] = (int)(acc >> 8);
			}
		}

		myip_v1_0_HLS_for2:for(word_cnt = 0; word_cnt < NUMBER_OF_OUTPUT_WORDS; word_cnt++){
			//write_output.data = sum.to_int() + word_cnt;	// using arbitrary precision internally but int for interfacing
			write_output.data = RES_OUT[word_cnt / RES_COLS][word_cnt % RES_COLS];	// using 32 bit precision or arbitrary precision all the way
			// write_output is the element sent by our ip through M_AXIS in one clock cycle.
			write_output.last = 0;
			write_output.keep = 0xFU;
			write_output.strb = 0xFU;
			if(word_cnt==NUMBER_OF_OUTPUT_WORDS-1)
			{
				write_output.last = 1;
				// M_AXIS_TLAST is required to be asserted for the last word.
				// Else, the AXI Stream FIFO / AXI DMA will not know if all the words have been received from the co-processor.
			}
			M_AXIS.write(write_output);
			// write() inserts it into the stream. Overloaded operator << can also be used.
		}
}