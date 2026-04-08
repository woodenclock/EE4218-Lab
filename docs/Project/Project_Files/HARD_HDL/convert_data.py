# python file to automate data conversion - we want to pack 8 bits of data into 32 bits 
X_csv = '../X.csv'
WHID_csv = '../w_hid.csv'
WOUT_csv = '../w_out.csv'
labels_csv = "../labels.csv"
test_input_file = 'nn_test_input.mem'
# test_output_file = 'nn_test_result_expected.mem'
bias = 'FF'

with open(test_input_file, 'w') as word_file:
    word_file.write("// X\n")
    # X is packed in row major order
    with open(X_csv, 'r') as X:
        for line in X:
            word = ""
            numbers = line.strip().split(',')
            for i in range(3):
                word += f"{int(numbers[2-i]):02X}"
            word_file.write(word + bias + "\n")
            word = ""
            for i in range(4):
                word += f"{int(numbers[6-i]):02X}"
            word_file.write(word + "\n")

    # WHID is packed in column major order
    with open(WHID_csv, 'r') as WHID:
        word_file.write("// WHID\n")
        word = ""
        col1 = []
        col2 = []
        for line in WHID:
            nums = line.strip().split(',')
            print(nums)
            col1.append(nums[0])
            col2.append(nums[1])
            # if len(numbers) == 4:
            #     for i in range(4):
            #         word += f"{int(numbers[3-i]):02X}"
            #     word_file.write(word + "\n")
            #     word = ""
        for i in range(2):
            col = col1 if i == 0 else col2
            for chunk in range(len(col) // 4):
                chunk = col[chunk*4:chunk*4+4]
                print(chunk)
                for i in range(4):
                    word += f"{int(chunk[3-i]):02X}"
                word_file.write(word + "\n")
                word = ""
    
    with open(WOUT_csv, 'r') as WOUT:
        word_file.write("// WOUT\n")
        numbers = []
        word = "00"
        for line in WOUT:
            numbers.append(int(line.strip()))
        for i in range(3):
            word += f"{numbers[2-i]:02X}"
        word_file.write(word + "\n")

# with open(test_output_file, 'w') as word_file:
#     word_file.write("// output labels \n")
#     with open(labels_csv, 'r') as labels:
#         count = 0
#         numbers = []
#         word = ""
#         for line in labels:
#             numbers.append(line.strip())
#             if len(numbers) >= 32:
#                 for i in range(32):
#                     word += numbers[31-i]
#                 word = f'{int(word, 2):08X}'
#                 word_file.write(word + "\n")
#                 word = ""
#                 numbers = []