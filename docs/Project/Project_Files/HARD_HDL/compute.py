import pandas as pd
import numpy as np

# ---------------- Load CSVs ----------------
X_csv = '../X.csv'
WHID_csv = '../w_hid.csv'
WOUT_csv = '../w_out.csv'
sigmoid_csv = "../sigmoid.csv"

X = pd.read_csv(X_csv, header=None).to_numpy()        # 64x7
WHID = pd.read_csv(WHID_csv, header=None).to_numpy() # 8x2
WOUT = pd.read_csv(WOUT_csv, header=None).to_numpy().flatten() # 3 entries: bias, hid0, hid1
sigmoid = pd.read_csv(sigmoid_csv, header=None).to_numpy().flatten()

# ---------------- Hidden Layer ----------------
bias_column = np.full((X.shape[0], 1), 255, dtype=int)  # fixed-point bias
X_with_bias = np.hstack((bias_column, X))               # 64x8

# Compute hidden layer (X * WHID)
hid_result = np.dot(X_with_bias, WHID).astype(np.int32) // 256

# Apply sigmoid lookup
hid_result_mod = hid_result & 0xFF
hid_sigmoid = sigmoid[hid_result_mod].astype(int)      # 64x2, each value 0..255

# ---------------- Output Layer ----------------
# WOUT = [bias, hid0, hid1]
bias = WOUT[0]                 # bias for output
w_hid0 = WOUT[1]               # weight for first hidden neuron
w_hid1 = WOUT[2]               # weight for second hidden neuron

# Compute output:  RES = (bias*255 + hid0*w_hid0 + hid1*w_hid1) / 256
output_raw = (bias*255 + hid_sigmoid[:,0]*w_hid0 + hid_sigmoid[:,1]*w_hid1) // 256

# Apply threshold: 1 if >128 else 0
output_thresh = (output_raw > 128).astype(int)

# ---------------- Print ----------------
print("Hidden layer (after sigmoid):")
print(hid_sigmoid)

print("Output (before threshold):")
print(output_raw)

print("Output (thresholded 0/1):")
print(output_thresh)

packed_res = []

# Process 64 bits in two 32-bit chunks
for i in range(0, len(output_thresh), 32):
    chunk = output_thresh[i:i+32]  # 32 bits
    word = 0
    for j, bit in enumerate(chunk):
        word |= (bit & 1) << j  # LSB = first element
    packed_res.append(f"{word:08X}")

test_output_file = 'nn_test_result_expected.mem'

with open(test_output_file, 'w') as word_file:
    for word in packed_res:
        word_file.write(word + "\n")

print("Packed RES (hex):")
for word in packed_res:
    print(word)