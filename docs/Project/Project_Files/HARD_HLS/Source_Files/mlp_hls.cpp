/*
 * mlp_hls.cpp  –  HLS implementation of MLP inference coprocessor
 *
 * Matches the SOFT C implementation in matrix.c exactly:
 *   Hidden  : N[r][n]      = (256*W_HID[n][0] + sum_{k=1..7} X[r][k]*W_HID[n][k]) >> 8
 *   Sigmoid : N_sig[r][n]  = sigmoid_LUT[ N[r][n] ]
 *   Output  : out[r]       = (256*W_OUT[0] + N_sig[r][0]*W_OUT[1] + N_sig[r][1]*W_OUT[2]) >> 8
 *   Predict : pred[r]      = (out[r] > 128) ? 1 : 0
 *
 * AXI-Stream input  (531 × 32-bit words, lower 8 bits used):
 *   [  0..511] X[64][8]    row-major; X[r][0] = 0 (bias = 256 handled in HW)
 *   [512..527] W_HID[2][8] neuron-0 first, then neuron-1
 *   [528..530] W_OUT[3]    { bias_w=80, hid0_w=50, hid1_w=200 }
 *
 * AXI-Stream output (64 × 32-bit words):
 *   Predictions 0 or 1 per sample, TLAST on last word.
 */

#include "mlp_hls.h"

// ── Sigmoid LUT (256 entries, matches sigmoid.csv) ────────────────────────────
static const unsigned char SIG_LUT[256] = {
    12, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 16, 16, 16,
    17, 17, 18, 18, 18, 19, 19, 20, 20, 21, 21, 21, 22, 22, 23, 23,
    24, 24, 25, 26, 26, 27, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33,
    34, 34, 35, 36, 36, 37, 38, 39, 39, 40, 41, 42, 43, 44, 44, 45,
    46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    62, 63, 64, 66, 67, 68, 69, 70, 72, 73, 74, 75, 76, 78, 79, 80,
    82, 83, 84, 86, 87, 88, 90, 91, 92, 94, 95, 97, 98, 99,101,102,
   104,105,107,108,110,111,113,114,116,117,119,120,122,123,125,126,
   128,129,130,132,133,135,136,138,139,141,142,144,145,147,148,150,
   151,153,154,156,157,158,160,161,163,164,165,167,168,169,171,172,
   173,175,176,177,179,180,181,182,183,185,186,187,188,189,191,192,
   193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,
   209,210,211,211,212,213,214,215,216,216,217,218,219,219,220,221,
   221,222,223,223,224,225,225,226,227,227,228,228,229,229,230,231,
   231,232,232,233,233,234,234,234,235,235,236,236,237,237,237,238,
   238,239,239,239,240,240,240,241,241,241,242,242,242,243,243,243
};

// ── Top-level function ─────────────────────────────────────────────────────────
void mlp_hls(AXI_stream &in_stream, AXI_stream &out_stream)
{
#pragma HLS INTERFACE axis port=in_stream
#pragma HLS INTERFACE axis port=out_stream
#pragma HLS INTERFACE ap_ctrl_none port=return

    // ── Local storage ─────────────────────────────────────────────────────────
    unsigned char X[NUM_SAMPLES][NUM_FEATURES_BIAS];
    unsigned char W_HID[NUM_HID][NUM_FEATURES_BIAS];
    unsigned char W_OUT[3];
    unsigned char N_sig[NUM_SAMPLES][NUM_HID];  // post-sigmoid hidden outputs

    // Partition weights across their neuron/weight dimension so inner-loop
    // reads are resolved in parallel (enables II=1 pipelining of the k-loop).
    #pragma HLS ARRAY_PARTITION variable=W_HID  complete dim=2
    #pragma HLS ARRAY_PARTITION variable=W_OUT  complete dim=1
    #pragma HLS ARRAY_PARTITION variable=N_sig  complete dim=2

    // ── 1. Read X[64][8] from input stream (512 words) ───────────────────────
    READ_X_ROW:
    for (int r = 0; r < NUM_SAMPLES; r++) {
        READ_X_COL:
        for (int c = 0; c < NUM_FEATURES_BIAS; c++) {
#pragma HLS PIPELINE II=1
            AXI_word w = in_stream.read();
            X[r][c] = (unsigned char)(w.data & 0xFF);
        }
    }

    // ── 2. Read W_HID[2][8] from input stream (16 words) ────────────────────
    //    Word order: neuron-0 weights (k=0..7), then neuron-1 weights (k=0..7)
    READ_WHID:
    for (int n = 0; n < NUM_HID; n++) {
        for (int k = 0; k < NUM_FEATURES_BIAS; k++) {
#pragma HLS PIPELINE II=1
            AXI_word w = in_stream.read();
            W_HID[n][k] = (unsigned char)(w.data & 0xFF);
        }
    }

    // ── 3. Read W_OUT[3] from input stream (3 words) ─────────────────────────
    READ_WOUT:
    for (int k = 0; k < 3; k++) {
#pragma HLS PIPELINE II=1
        AXI_word w = in_stream.read();
        W_OUT[k] = (unsigned char)(w.data & 0xFF);
    }

    // ── 4. Hidden layer + sigmoid ─────────────────────────────────────────────
    //  acc = W_HID[n][0] * 256        (bias: X[r][0] is always 256)
    //      + sum_{k=1..7} X[r][k] * W_HID[n][k]
    //  N[r][n] = acc >> 8
    //  N_sig[r][n] = SIG_LUT[ N[r][n] ]
    HIDDEN_ROW:
    for (int r = 0; r < NUM_SAMPLES; r++) {
#pragma HLS PIPELINE II=1
        HIDDEN_NEURON:
        for (int n = 0; n < NUM_HID; n++) {
            int acc = ((int)W_HID[n][0]) << 8;  // bias contribution
            HIDDEN_K:
            for (int k = 1; k < NUM_FEATURES_BIAS; k++) {
                acc += (int)X[r][k] * (int)W_HID[n][k];
            }
            unsigned char hid_out = (unsigned char)(acc >> 8);
            N_sig[r][n] = SIG_LUT[hid_out];
        }
    }

    // ── 5. Output layer → threshold → stream out ─────────────────────────────
    //  acc = W_OUT[0] * 256              (bias: N_sig bias = 256)
    //      + N_sig[r][0] * W_OUT[1]
    //      + N_sig[r][1] * W_OUT[2]
    //  pred = (acc >> 8) > 128 ? 1 : 0
    OUTPUT_ROW:
    for (int r = 0; r < NUM_SAMPLES; r++) {
#pragma HLS PIPELINE II=1
        int acc = ((int)W_OUT[0]) << 8;
        acc += (int)N_sig[r][0] * (int)W_OUT[1];
        acc += (int)N_sig[r][1] * (int)W_OUT[2];
        int out_val = acc >> 8;
        unsigned char pred = (out_val > 128) ? 1 : 0;

        AXI_word w;
        w.data = (ap_uint<32>)pred;
        w.keep = 0xF;
        w.strb = 0xF;
        w.last = (r == NUM_SAMPLES - 1) ? 1 : 0;
        out_stream.write(w);
    }
}