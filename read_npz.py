import numpy as np
import struct
import sys

def npz_to_binary(npz_file, binary_file):
    """
    Converts an .npz file to a binary file for use in C programs.
    
    Args:
        npz_file (str): Path to the input .npz file.
        binary_file (str): Path to the output binary file.
    """
    # Load the .npz file
    data = np.load(npz_file)

    with open(binary_file, 'wb') as bin_file:
        for key in data.files:
            array = data[key]

            # Write the shape of the array as integers
            shape = array.shape
            bin_file.write(struct.pack('I' * len(shape), *shape))

            # Write the array data as binary
            if array.dtype == np.float64:
                bin_file.write(array.astype(np.float64).tobytes())
            elif array.dtype == np.float32:
                bin_file.write(array.astype(np.float32).tobytes())
            elif array.dtype == np.int32:
                bin_file.write(array.astype(np.int32).tobytes())
            elif array.dtype == np.uint8:
                bin_file.write(array.astype(np.uint8).tobytes())
            else:
                raise ValueError(f"Unsupported data type: {array.dtype}")

    print(f"Data successfully written to {binary_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python npz_to_binary.py <input.npz> <output.bin>")
    else:
        npz_to_binary(sys.argv[1], sys.argv[2])
