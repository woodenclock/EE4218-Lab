# PYNQ program to exercise AXI DMA with a streaming accelerator
# Mirrors the logic in your standalone C example (no SG, polling, 2 test vectors)

from pynq import Overlay, allocate
import numpy as np
import time

# --- Configuration constants (match your C code) ---
A_ROWS = 64
A_COLS = 8
B_ROWS = A_COLS
B_COLS = 1
RES_ROWS = A_ROWS
RES_COLS = B_COLS

NUMBER_OF_INPUT_A = A_ROWS * A_COLS
NUMBER_OF_INPUT_B = B_ROWS * B_COLS
NUMBER_OF_INPUT_WORDS  = NUMBER_OF_INPUT_A + NUMBER_OF_INPUT_B
NUMBER_OF_OUTPUT_WORDS = RES_ROWS * RES_COLS

## --- Allocate contiguous buffers in DDR (pynq.allocate handles cache coherency) ---
#in_buf  = allocate(shape=(NUMBER_OF_INPUT_WORDS,),  dtype=np.int32)
#out_buf = allocate(shape=(NUMBER_OF_OUTPUT_WORDS,), dtype=np.int32)
#exp = allocate(shape=(NUMBER_OF_OUTPUT_WORDS,), dtype=np.int32)

# with open ('A.csv', 'r') as a:
#     for i, line in enumerate(a):
#         val = np.int32(line.strip())
#         in_buf[i] = val

# with open ('B.csv', 'r') as b:
#     for i, line in enumerate(b):
#         val = np.int32(line.strip())
#         in_buf[i + NUMBER_OF_INPUT_A] = val

# with open('Labels.csv', 'r') as res:
#     for i, line in enumerate(res):
#         val = np.int32(line.strip())
#         exp[i] = val

A = np.genfromtxt('A.csv', delimiter=',', dtype=np.int32)
B = np.genfromtxt('B.csv', delimiter=',', dtype=np.int32)
RES = np.genfromtxt('Labels.csv', delimiter=',', dtype=np.int32)

A = A.reshape(A_ROWS, A_COLS)
B = B.reshape(B_ROWS, B_COLS)

#in_buf[:NUMBER_OF_INPUT_A] = A.flatten()
#in_buf[NUMBER_OF_INPUT_A:] = B.flatten()
        
# --- Load overlay and get DMA ---
# Replace with your actual bitstream path
overlay = Overlay('/home/ubuntu/Pynq/nonopt.bit')

# Inspect IPs if you’re unsure about names:
# print("Loaded Overlay")
# print("bitfile =", overlay.bitfile_name)
# print("C_INCLUDE_SG =", overlay.ip_dict['axi_dma_0']['parameters']['C_INCLUDE_SG'])

# Replace with your actual DMA instance name from the overlay
dma = overlay.axi_dma_0
# print("Got DMA")

#####################################################################################
# --- Allocate contiguous buffers in DDR (pynq.allocate handles cache coherency) ---
in_buf  = allocate(shape=(NUMBER_OF_INPUT_WORDS,),  dtype=np.int32)
out_buf = allocate(shape=(NUMBER_OF_OUTPUT_WORDS,), dtype=np.int32)
exp = allocate(shape=(NUMBER_OF_OUTPUT_WORDS,), dtype=np.int32)

in_buf[:NUMBER_OF_INPUT_A] = A.flatten()
in_buf[NUMBER_OF_INPUT_A:] = B.flatten()
#####################################################################################

# --- Run tests ---
ok = True
start = time.time()

# Start DMA transfers: send input vector, receive output vector
# print("Starting send transfer")
dma.sendchannel.transfer(in_buf)

# print("Starting recv transfer")
dma.recvchannel.transfer(out_buf)

# Polling wait for completion (equivalent to XAxiDma_Busy loops)
# print("Waiting for send")
dma.sendchannel.wait()

# print("Waiting for recv")
dma.recvchannel.wait()
# print("Waiting for recv")

end = time.time()

# Validate results
elapsed = end - start
elapsed_us = elapsed * 1e6
print("Execution time (us):", elapsed_us)
exp =  (np.matmul(A, B) // 256).flatten()
ok = np.array_equal(out_buf, exp)

total_bytes = (NUMBER_OF_INPUT_WORDS + NUMBER_OF_OUTPUT_WORDS) * 4
throughput = total_bytes / elapsed
throughput_MBps = throughput / 1e6
print("Throughput (MB/s):", throughput_MBps)

# --- Final result ---
if ok:
    print("Test Success")
else:
    print("Test Failed")
