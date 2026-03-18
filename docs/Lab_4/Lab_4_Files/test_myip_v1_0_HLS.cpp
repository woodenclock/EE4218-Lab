/*
----------------------------------------------------------------------------------
--	(c) Rajesh C Panicker, NUS,
--  Description : Self-checking testbench for AXI Stream Coprocessor (HLS) implementing the sum of 4 numbers
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

#include <stdio.h>
#include "hls_stream.h"
#include "ap_axi_sdata.h"

#include <stdint.h>

typedef ap_axis<32,0,0,0> AXIS;

/***************** Coprocessor function declaration *********************/

void myip_v1_0_HLS(hls::stream<AXIS>& S_AXIS, hls::stream<AXIS>& M_AXIS);


/***************** Macros *********************/
#define NUMBER_OF_INPUT_WORDS (64 * 8 + 8)  // length of an input vector
#define NUMBER_OF_OUTPUT_WORDS 64  // length of an input vector
#define NUMBER_OF_TEST_VECTORS 1  // number of such test vectors (cases)

#define A_ROWS 64
#define A_COLS 8
#define B_ROWS 8
#define B_COLS 1


/************************** Variable Definitions *****************************/
// hard coded input
int test_input_memory [NUMBER_OF_TEST_VECTORS*NUMBER_OF_INPUT_WORDS] = 
	{160,129,104,71,184,193,55,86,
	39,31,145,150,32,65,90,45,
	153,159,93,24,149,136,108,61,
	133,73,183,124,151,39,68,67,
	91,24,115,144,125,0,255,42,
	148,92,126,71,175,204,100,156,
	170,149,168,58,198,192,48,86,
	91,79,47,91,104,71,146,75,
	64,73,151,145,78,41,78,39,
	96,12,126,98,177,40,118,78,
	26,108,116,129,0,85,54,92,
	135,167,172,73,161,159,58,64,
	116,104,171,124,132,166,108,72,
	57,64,157,177,83,71,156,22,
	146,14,127,144,151,94,0,45,
	138,111,101,84,175,136,48,103,
	137,140,111,45,157,255,98,75,
	161,156,172,124,170,189,110,125,
	69,64,77,98,92,178,136,67,
	151,181,181,82,175,152,95,89,
	104,90,177,106,121,128,85,89,
	61,25,71,117,129,53,63,45,
	255,34,255,236,192,36,60,136,
	95,2,118,98,124,85,115,50,
	166,219,156,82,253,225,43,120,
	92,184,107,84,120,153,83,67,
	43,41,119,98,57,55,68,117,
	137,117,144,131,147,165,88,83,
	117,73,127,91,142,149,33,92,
	89,111,129,137,210,63,75,86,
	83,60,214,124,60,49,136,92,
	140,19,141,137,162,57,15,33,
	86,12,114,157,83,94,93,106,
	156,225,129,98,170,207,141,97,
	118,18,111,137,138,70,43,42,
	100,138,116,71,143,201,88,72,
	148,41,47,58,173,140,108,22,
	177,159,141,11,184,251,141,58,
	96,0,88,111,122,65,173,45,
	149,226,137,112,162,172,100,100,
	160,214,149,61,200,214,90,117,
	136,116,197,116,170,191,141,103,
	124,141,107,8,143,146,90,83,
	73,86,81,55,74,88,93,184,
	64,40,130,161,63,76,161,56,
	58,43,131,157,51,136,38,42,
	180,167,145,45,255,188,80,114,
	117,135,101,65,120,137,123,78,
	83,6,79,111,116,80,43,61,
	137,23,116,98,161,85,183,50,
	152,184,183,157,175,182,125,86,
	120,110,203,65,138,152,100,111,
	191,255,153,78,203,213,136,106,
	0,44,0,0,81,90,90,50,
	83,21,48,71,78,135,125,45,
	118,73,164,99,122,90,125,78,
	123,175,183,117,170,184,141,173,
	111,185,149,53,129,226,83,72,
	176,184,156,80,212,181,50,106,
	146,183,186,124,157,220,95,139,
	96,120,115,111,129,97,118,255,
	126,200,181,86,175,185,110,125,
	111,80,115,108,101,114,118,45,
	141,144,146,66,157,216,88,159,
	75,
	66,
	57,
	0,
	1,
	46,
	14,
	37
	};
int test_result_expected_memory [NUMBER_OF_TEST_VECTORS*NUMBER_OF_OUTPUT_WORDS];
int result_memory [NUMBER_OF_TEST_VECTORS*NUMBER_OF_OUTPUT_WORDS]; // same size as test_result_expected_memory

/*****************************************************************************
* Main function
******************************************************************************/
int main()
{
	int word_cnt, test_case_cnt = 0;
	int success;
	AXIS read_output, write_input;
	hls::stream<AXIS> S_AXIS;
	hls::stream<AXIS> M_AXIS;

	int A_IN[A_ROWS][A_COLS];
	int B_IN[B_ROWS][B_COLS];

	/************** Run a software version of the hardware function to validate results ************/
	// instead of hard-coding the results in test_result_expected_memory

	for (int i = 0; i < 512; i++){
		A_IN[i / A_COLS][i % A_COLS] = test_input_memory[i];
	}
	for (int i = 0; i < 8; i++){
		B_IN[i / B_COLS][i % B_COLS] = test_input_memory[i + 512];
	}

	for(int i = 0; i < A_ROWS; i++){
		for (int k = 0; k < B_COLS; k++){
			int32_t acc = 0;
			for (int j = 0; j < A_COLS; j++){
				acc += (int32_t)A_IN[i][j] * (int32_t)B_IN[j][k];
			}
		test_result_expected_memory[i] = (int)(acc >> 8);
		}
	}

	for (test_case_cnt=0 ; test_case_cnt < NUMBER_OF_TEST_VECTORS ; test_case_cnt++){

		/******************** Input to Coprocessor : Transmit the Data Stream ***********************/

		printf(" Transmitting Data for test case %d ... \r\n", test_case_cnt);

		for (word_cnt=0 ; word_cnt < NUMBER_OF_INPUT_WORDS ; word_cnt++){

			write_input.data = test_input_memory[word_cnt+test_case_cnt*NUMBER_OF_INPUT_WORDS];
			write_input.last = 0;
			if(word_cnt==NUMBER_OF_INPUT_WORDS-1)
			{
				write_input.last = 1;
				// S_AXIS_TLAST is asserted for the last word.
				// Actually, doesn't matter since we are not making using of S_AXIS_TLAST.
			}
			S_AXIS.write(write_input); // insert one word into the stream
		}

		/* Transmission Complete */

		/********************* Call the hardware function (invoke the co-processor / ip) ***************/

		myip_v1_0_HLS(S_AXIS, M_AXIS);


		/******************** Output from Coprocessor : Receive the Data Stream ***********************/

		printf(" Receiving data for test case %d ... \r\n", test_case_cnt);

		for (word_cnt=0 ; word_cnt < NUMBER_OF_OUTPUT_WORDS ; word_cnt++){

			read_output = M_AXIS.read(); // extract one word from the stream
			result_memory[word_cnt+test_case_cnt*NUMBER_OF_OUTPUT_WORDS] = read_output.data;
		}

		/* Reception Complete */
	}

	/************************** Checking correctness of results *****************************/

	success = 1;

	/* Compare the data send with the data received */
	printf(" Comparing data ...\r\n");
	for(word_cnt=0; word_cnt < NUMBER_OF_TEST_VECTORS*NUMBER_OF_OUTPUT_WORDS; word_cnt++){
		success = success & (result_memory[word_cnt] == test_result_expected_memory[word_cnt]);
	}

	if (success != 1){
		printf("Test Failed\r\n");
		return 1;
	}

	printf("Test Success\r\n");

	return 0;
}
