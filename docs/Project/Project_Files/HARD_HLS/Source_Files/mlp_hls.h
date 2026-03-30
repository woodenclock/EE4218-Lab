#pragma once

#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "ap_int.h"

// ── AXI-Stream word type (matches myip_v1_0 / lab setup: 32-bit bus, lower 8 used) ──
typedef ap_axiu<32, 0, 0, 0> AXI_word;
typedef hls::stream<AXI_word>  AXI_stream;

// ── MLP dimensions ────────────────────────────────────────────────────────────
#define NUM_SAMPLES      64
#define NUM_FEATURES_BIAS 8    // 7 features + 1 bias column (col-0 handled implicitly)
#define NUM_HID          2     // hidden neurons
#define NUM_IN_WORDS     531   // 512 (X) + 16 (W_HID) + 3 (W_OUT)
#define NUM_OUT_WORDS    64

// ── Top-level function ────────────────────────────────────────────────────────
void mlp_hls(AXI_stream &in_stream, AXI_stream &out_stream);
