import numpy as np
import struct
import sys

def binary_to_npz(binary_file, npz_file):
    """
    Reads a binary file and converts it to an .npz file with specified keys, shapes, and data types.

    Args:
        binary_file (str): Path to the input binary file.
        npz_file (str): Path to the output .npz file.
    """
    # Define the structure of the output .npz file
    data_structure = {
        "gsd_10": {"shape": (192, 192, 4), "dtype": np.float32},
        "gsd_20": {"shape": (96, 96, 6), "dtype": np.float32},
        "gsd_60": {"shape": (32, 32, 2), "dtype": np.float32},
        "scl": {"shape": (96, 96), "dtype": np.uint8},
        "bad_percent": {"shape": (), "dtype": np.float64},
    }

    data_dict = {}

    with open(binary_file, 'rb') as bin_file:
        for key, details in data_structure.items():
            shape = details["shape"]
            dtype = details["dtype"]

            # Calculate the number of elements in the array
            num_elements = np.prod(shape) if shape else 1

            # Read the binary data and reshape
            data = np.frombuffer(bin_file.read(num_elements * np.dtype(dtype).itemsize), dtype=dtype)
            data_dict[key] = data if not shape else data.reshape(shape)

    # Save the data to an .npz file
    np.savez(npz_file, **data_dict)
    print(f"Data successfully written to {npz_file}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python binary_to_npz.py <input.bin> <output.npz>")
    else:
        binary_to_npz(sys.argv[1], sys.argv[2])
