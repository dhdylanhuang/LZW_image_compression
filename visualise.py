import numpy as np
import matplotlib.pyplot as plt

# Path to the .npz file
npz_file_path = "S2A_2LMK_20170326_0_L2A.npz"

# Load the .npz file
data = np.load(npz_file_path)

# List keys and shapes of arrays
for key in data.files:
    print(f"Key: {key}, Shape: {data[key].shape}, Data Type: {data[key].dtype}")
    

    
gsd_10 = data['gsd_10']
scl = data['scl'] 
gsd_60 = data['gsd_60']

# Normalize gsd_10 for visualization (using the first 3 channels as RGB)
rgb_image = gsd_10[:, :, :3]  # Take the first 3 channels (R, G, B)
rgb_image = (rgb_image - rgb_image.min()) / (rgb_image.max() - rgb_image.min())  # Normalize to [0, 1]
rgb_image = (rgb_image * 255).astype(np.uint8)  # Convert to 8-bit

# Normalize gsd_60 channels for grayscale visualization
gsd_60_channel_1 = gsd_60[:, :, 0]  # First channel
gsd_60_channel_1 = (gsd_60_channel_1 - gsd_60_channel_1.min()) / (gsd_60_channel_1.max() - gsd_60_channel_1.min())

gsd_60_channel_2 = gsd_60[:, :, 1]  # Second channel
gsd_60_channel_2 = (gsd_60_channel_2 - gsd_60_channel_2.min()) / (gsd_60_channel_2.max() - gsd_60_channel_2.min())

# Create a figure with 4 subplots
plt.figure(figsize=(16, 8))  # Adjust the figure size

# First subplot: RGB image
plt.subplot(2, 2, 1)
plt.title("RGB Image (gsd_10)")
plt.imshow(rgb_image)
plt.axis("off")  # Remove axes for a cleaner display

# Second subplot: SCL layer
plt.subplot(2, 2, 2)
plt.title("SCL Layer")
plt.imshow(scl, cmap='tab10')  # Use a qualitative colormap
plt.colorbar(label="SCL Classes")  # Add a colorbar for the classification
plt.axis("off")

# Third subplot: GSD_60 Channel 1
plt.subplot(2, 2, 3)
plt.title("GSD_60 Channel 1")
plt.imshow(gsd_60_channel_1, cmap='gray')  # Grayscale visualization
plt.colorbar(label="Intensity")
plt.axis("off")

# Fourth subplot: GSD_60 Channel 2
plt.subplot(2, 2, 4)
plt.title("GSD_60 Channel 2")
plt.imshow(gsd_60_channel_2, cmap='gray')  # Grayscale visualization
plt.colorbar(label="Intensity")
plt.axis("off")

# Display all plots
plt.tight_layout()
plt.show()
