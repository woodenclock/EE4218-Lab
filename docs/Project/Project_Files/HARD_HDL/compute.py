# python file for computation to check hidden layer result
import pandas as pd
import numpy as np

# Load CSVs
X_csv = '../X.csv'
WHID_csv = '../w_hid.csv'

sigmoid_csv = "../sigmoid.csv"

X = pd.read_csv(X_csv, header=None)       # shape: 64x7 (assuming original features)
WHID = pd.read_csv(WHID_csv, header=None) # shape: 8x2 (including bias row)

sigmoid = pd.read_csv(sigmoid_csv, header=None).to_numpy().flatten()

# Add bias column (1s) to X
bias_column = np.full((X.shape[0], 1), 255, dtype=int)
X_with_bias = np.hstack((bias_column, X.to_numpy()))  # shape: 64x8

# Convert WHID to numpy array
WHID_np = WHID.to_numpy()  # shape: 8x2

# Matrix multiplication
result = np.dot(X_with_bias, WHID_np)  # shape: 64x2
result = result.astype(np.int32)
result = result // 256

hex_result = np.vectorize(lambda x: f"{x & 0xFF:02X}")(result)

# Print result
print("X (with bias) * WHID =")
print(result)

print("Hex Result:")
for row in hex_result:
    print(" ".join(row))
    
indices = result & 0xFF
sigmoid_result = sigmoid[indices]
hex_sigmoid = np.vectorize(lambda x: f"{int(x):02X}")(sigmoid_result)

print("After Sigmoid (Hex):")
for row in hex_sigmoid:
    print(" ".join(row))