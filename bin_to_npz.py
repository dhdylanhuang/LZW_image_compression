import numpy as np
import struct
import sys

def binary_to_npz(binary_file, npz_file):
    """
    Converts a binary file with a key-index mapping back to an .npz file.
    """    
    data_dict = {}

    with open(binary_file, 'rb') as bin_file:
        # Read number of keys
        num_keys = struct.unpack('I', bin_file.read(4))[0]

        # Read metadata
        metadata = []
        for _ in range(num_keys):
            key_length = struct.unpack('I', bin_file.read(4))[0]
            key = bin_file.read(key_length).decode('utf-8')

            shape_length = struct.unpack('I', bin_file.read(4))[0]
            shape = struct.unpack('I' * shape_length, bin_file.read(4 * shape_length))

            dtype_length = struct.unpack('I', bin_file.read(4))[0]
            dtype = bin_file.read(dtype_length).decode('utf-8')

            metadata.append((key, shape, dtype))

        # Read binary data
        for key, shape, dtype in metadata:
            num_elements = np.prod(shape) if shape else 1
            array = np.frombuffer(bin_file.read(num_elements * np.dtype(dtype).itemsize), dtype=dtype)
            data_dict[key] = array.reshape(shape)

    # Save to .npz
    np.savez(npz_file, **data_dict)
    print(f"Data successfully written to {npz_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python binary_to_npz.py <input.bin> <output.npz>")
    else:
        binary_to_npz(sys.argv[1], sys.argv[2])
