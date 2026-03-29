`ifndef SHARED_PARAMS_VH
`define SHARED_PARAMS_VH

// ── MLP architecture dimensions ──────────────────────────────────────────────
// Input layer : 7 features + 1 bias = 8 columns   (bias handled implicitly)
// Hidden layer: 2 neurons, sigmoid activation
// Output layer: 1 neuron, linear activation
// Dataset     : 64 samples

`define NUM_SAMPLES    64
`define NUM_FEATURES_BIAS 8    // 7 features + 1 bias column slot
`define NUM_HID        2
`define DATA_WIDTH     8

// ── AXI-Stream word counts ────────────────────────────────────────────────────
// X      : 64 rows × 8 cols        = 512 bytes  (col-0 is bias slot, sent as 0)
// W_HID  : 2 neurons × 8 weights   =  16 bytes  (neuron-0 first, then neuron-1)
// W_OUT  : 1 × 3 weights           =   3 bytes  (bias, hid-0, hid-1)
`define TOTAL_IN_WORDS  531   // 512 + 16 + 3
`define TOTAL_OUT_WORDS  64

// ── RAM depth bits ────────────────────────────────────────────────────────────
`define X_DEPTH_BITS    9   // 2^9 = 512
`define WHID_DEPTH_BITS 4   // 2^4 = 16
`define WOUT_DEPTH_BITS 2   // 2^2 = 4 (only 3 used)
`define RES_DEPTH_BITS  6   // 2^6 = 64

`endif // SHARED_PARAMS_VH
