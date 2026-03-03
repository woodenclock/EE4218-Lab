import numpy as np

np.random.seed(42)
A = np.random.randint(0, 256, (64, 8), dtype=np.uint8)
B = np.random.randint(0, 256, (8, 1), dtype=np.uint8)
C = np.matmul(A.astype(np.uint32), B.astype(np.uint32))

with open("test_input_new.mem", "w") as f:
    f.write("// matrix A\n")
    for val in A.flatten(order='C'):
        f.write(f"{val:02X}\n")
    f.write("// matrix B\n")
    for val in B.flatten(order='C'):
        f.write(f"{val:02X}\n")

with open("test_result_expected_new.mem", "w") as f:
    for val in C.flatten(order = 'C'):
        f.write(f"{((val >> 8) & 0xFF):02X}\n")