import pandas as pd
import matplotlib.pyplot as plt

# Read CSV file
file_path = 'csvs/monitoring.csv'  # Path to the CSV file
data = pd.read_csv(file_path)

# Ensure that the columns are correctly named
assert 'Cache_Misses' in data.columns and 'Energy' in data.columns and 'Instructions' in data.columns, "The CSV file must contain the columns Cache_Misses, Energy, and Instructions."

# Create a time axis (x-axis)
time = range(len(data))

# Create a figure with multiple subplots
fig, axs = plt.subplots(3, 1, figsize=(10, 15))

# Function to plot a specific subplot
def plot_column(ax, column_name, color):
    ax.plot(time, data[column_name], label=column_name, color=color)
    ax.set_xlabel('Time')
    ax.set_ylabel('Value')
    ax.set_title(f'Temporal Trend of {column_name}')
    ax.legend()
    ax.grid(True)

# Plot for the Cache_Misses
plot_column(axs[0], 'Cache_Misses', 'blue')

# Plot for the Energy
plot_column(axs[1], 'Energy', 'orange')

# Plot for the Instructions
plot_column(axs[2], 'Instructions', 'green')

# Adjust layout and save the figure
plt.tight_layout()
plt.savefig('./plots/monitoring_all_plots.png')
