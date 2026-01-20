import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import argparse

# Set up command-line argument parsing
parser = argparse.ArgumentParser(description='Generate a 3D surface plot from a CSV file.')
parser.add_argument('filename', type=str, help='The CSV file containing the data.')
args = parser.parse_args()

# Load the data from the specified file into a pandas DataFrame
df = pd.read_csv(args.filename)

# Prepare data for plotting
X = df['memory'].values
Y = df['threads'].values
Z = 1024 / (df['hash_time'].values + df['sort_time'].values + df['sync_time'].values)  # You can change this to 'sort_time' or 'sync_time' if needed

# Create a 2D grid of X and Y values
X_unique = np.unique(X)
Y_unique = np.unique(Y)
X_grid, Y_grid = np.meshgrid(X_unique, Y_unique)

# Reshape Z to match the grid shape of X and Y
Z_grid = np.zeros_like(X_grid, dtype=float)
for i, y in enumerate(Y_unique):
    Z_grid[i, :] = Z[df['threads'] == y]

# Plot the surface
fig = plt.figure()
ax = fig.add_subplot(111, projection='3d')

surf = ax.plot_surface(X_grid, Y_grid, Z_grid, cmap='viridis')



# Add labels and a color bar
ax.set_xlabel('Memory')
ax.set_ylabel('Threads')
ax.set_zlabel('Hash Time')
fig.colorbar(surf)

# Show the plot
plt.show()
