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
    metadata = []
    current_offset = 0

    with open(binary_file, 'wb') as bin_file:
        # Write metadata header
        bin_file.write(struct.pack('I', len(data.files)))  # Number of keys

        for key in data.files:
            array = data[key]
            shape = array.shape
            dtype = str(array.dtype)

            # Calculate the byte offset for this key
            metadata.append((key, shape, dtype, current_offset))
            current_offset += array.nbytes

            # Write metadata for this key
            bin_file.write(struct.pack('I', len(key)))  # Key name length
            bin_file.write(key.encode('utf-8'))  # Key name
            bin_file.write(struct.pack('I', len(shape)))  # Shape length
            bin_file.write(struct.pack('I' * len(shape), *shape))  # Shape
            bin_file.write(struct.pack('I', len(dtype)))  # Data type length
            bin_file.write(dtype.encode('utf-8'))  # Data type

        # Write data blocks
        for key in data.files:
            array = data[key]
            bin_file.write(array.tobytes())

    print(f"Data with metadata successfully written to {binary_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python npz_to_binary.py <input.npz> <output.bin>")
    else:
        npz_to_binary(sys.argv[1], sys.argv[2])
