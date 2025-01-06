import numpy as np
import os
import sys

def process_npy_folder(folder_path, output_file):
    """
    Reads all .npy files in a folder and processes them.

    Args:
        folder_path (str): Path to the folder containing .npy files.
        output_file (str): Path to save the combined data (optional).
    """
    # Dictionary to store loaded arrays
    combined_data = {}

    # Iterate through all files in the folder
    for filename in os.listdir(folder_path):
        if filename.endswith('.npy'):
            file_path = os.path.join(folder_path, filename)

            # Load the .npy file
            array = np.load(file_path)
            key = os.path.splitext(filename)[0]  # Use filename without extension as the key
            combined_data[key] = array

            # Print details about the loaded array
            print(f"Loaded: {filename}, Shape: {array.shape}, Dtype: {array.dtype}")

    # Optionally save the combined data as an .npz file
    if output_file:
        with open(output_file, 'wb') as f:
            np.save(f, combined_data)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python read_npz.py <folder_path> <output_file>")
    else:
        folder_path = sys.argv[1]
        output_file = sys.argv[2]
        process_npy_folder(folder_path, output_file)
