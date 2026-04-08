`ifndef SHARED_PARAMS_VH
`define SHARED_PARAMS_VH

// ── MLP architecture dimensions ──────────────────────────────────────────────
// Input layer : 7 features + 1 bias = 8 columns   (bias handled implicitly)
// Hidden layer: 2 neurons, sigmoid activation
// Output layer: 1 neuron, linear activation
// Dataset     : 64 samples

`define NUM_SAMPLES             64
`define NUM_FEATURES            8        // 7 features + 1 bias column slot
`define NUM_HID_LAYERS          2
`define DATA_WIDTH              8       // each data is 8 bits
`define AXI_DATA_WIDTH          32      // AXI transmit 32 bits at once

`define AXI_PACKET_SIZE         (`AXI_DATA_WIDTH / `DATA_WIDTH)         // 4 sets of data in 1 address

`define X_SIZE                  (`NUM_SAMPLES * `NUM_FEATURES)          // 64 * 8 = 512
`define WHID_SIZE               (`NUM_FEATURES * `NUM_HID_LAYERS)       // 2 * 8 = 16
`define HID_RES_SIZE            (`NUM_SAMPLES * `NUM_HID_LAYERS)        // 64 * 2 = 128
`define WOUT_SIZE               (`NUM_HID_LAYERS + 1)                   // 3 

`define NUM_X_PACKETS           ((`X_SIZE + `AXI_PACKET_SIZE - 1)/ `AXI_PACKET_SIZE)        // 128
`define NUM_WHID_PACKETS        ((`WHID_SIZE + `AXI_PACKET_SIZE - 1) / `AXI_PACKET_SIZE)   // 4
`define NUM_HID_RES_PACKETS     ((`HID_RES_SIZE + `AXI_PACKET_SIZE - 1) / `AXI_PACKET_SIZE) 
`define NUM_WOUT_PACKETS        ((`WOUT_SIZE + `AXI_PACKET_SIZE - 1) / `AXI_PACKET_SIZE)   // 1
`define NUM_RES_PACKETS         ((`NUM_SAMPLES / `AXI_DATA_WIDTH))                         // 8

`define X_ADDRESS_WIDTH         $clog2(`X_SIZE)
`define WHID_ADDRESS_WIDTH      $clog2(`WHID_SIZE)
`define WOUT_ADDRESS_WIDTH      1
`define RES_ADDRESS_WIDTH       $clog2(`NUM_SAMPLES)

// ── AXI-Stream word counts ────────────────────────────────────────────────────
// X      : 64 rows × 8 cols        = 512 bytes  (col-0 is bias slot, sent as 0)
// W_HID  : 2 neurons × 8 weights   =  16 bytes  (neuron-0 first, then neuron-1)
// W_OUT  : 1 × 3 weights           =   3 bytes  (bias, hid-0, hid-1)
`define TOTAL_IN_WORDS          ((`NUM_X_PACKETS + \
                                    `NUM_WHID_PACKETS +  \
                                    `NUM_WOUT_PACKETS))
`define TOTAL_OUT_WORDS         (`NUM_RES_PACKETS)

// ── RAM depth bits ────────────────────────────────────────────────────────────
`define X_DEPTH_BITS            $clog2(`NUM_X_PACKETS)   // 2^9 = 512
`define WHID_DEPTH_BITS         $clog2(`NUM_WHID_PACKETS)   // 2^4 = 16
`define HID_RES_DEPTH_BITS      $clog2(`NUM_HID_RES_PACKETS)
`define WOUT_DEPTH_BITS         1   // 2^2 = 4 (only 3 used)
`define RES_DEPTH_BITS          $clog2(`NUM_RES_PACKETS)   // 2^6 = 64

`endif // SHARED_PARAMS_VH
