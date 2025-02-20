import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

# Check if the filename and storage_type are provided as command-line arguments
if len(sys.argv) < 3:
    print("Usage: python plot_time_lookup.py <filename> <storage_type>")
    sys.exit(1)

# Get the filename and storage_type from the command-line arguments
data_filename = sys.argv[1]
storage_type = sys.argv[2]

# Read the data from the provided filename
try:
    df = pd.read_csv(data_filename, sep='\s+')
except FileNotFoundError:
    print(f"Error: File '{data_filename}' not found.")
    sys.exit(1)

# Convert relevant columns to numeric types
numeric_columns = ['file_size', 'search_bytes', 'time_lookup_ms']
for column in numeric_columns:
    df[column] = pd.to_numeric(df[column], errors='coerce')

# Drop rows with NaN in critical columns
df.dropna(subset=numeric_columns, inplace=True)

# Convert 'file_size' from bytes to gigabytes for better readability
df['file_size_gb'] = df['file_size'] / (1024 ** 3)

# Keep 'search_bytes' as is, without converting to kilobytes
# So we will use 'search_bytes' directly in the plot

# Create a pivot table to reshape the data for plotting
pivot_df = df.pivot_table(
    index='file_size_gb',
    columns='search_bytes',
    values='time_lookup_ms',
    aggfunc='mean'
)

# Plotting the bar chart
ax = pivot_df.plot(kind='bar', figsize=(12, 6), width=0.8)

# Set plot labels and title, including the filename and storage_type in the title
ax.set_xlabel('File Size (GB)')
ax.set_ylabel('Time Lookup (ms)')
ax.set_title(f'Time Lookup vs File Size for Different Search Bytes\nData Source: {os.path.basename(data_filename)}, Storage Type: {storage_type}')
ax.legend(title='Search Bytes (Bytes)', bbox_to_anchor=(1.05, 1), loc='upper left')

# Adjust layout to prevent clipping of labels and legend
plt.tight_layout()

# Use storage_type to augment the output filename
output_filename = f"time_lookup_plot_{storage_type}.jpg"
plt.savefig(output_filename, format='jpg', dpi=300)

# Optionally, display the plot
# plt.show()

print(f"Plot saved as '{output_filename}'")