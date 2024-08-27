import sys
import pandas as pd
import matplotlib.pyplot as plt

# Function to plot columns from two files using scatter plots
def plot_columns(file1, file2, count, save_prefix='scatter_plot'):
    # Read data from both files
    data1 = pd.read_csv(file1)
    data2 = pd.read_csv(file2)

    # Ensure both files have the same structure
    assert all(data1.columns == data2.columns), "Both CSV files must have the same column structure"

    # Ensure columns Cache_Misses, Energy, and Instructions are present
    assert 'Cache_Misses' in data1.columns and 'Energy' in data1.columns and 'Instructions' in data1.columns, "CSV files must contain columns Cache_Misses, Energy, and Instructions"

    # Plotting function for a single column using scatter plot
    def scatter_single_column(ax, x1, y1, x2, y2, label1, label2, color1, color2):
        ax.scatter(x1, y1, label=label1, color=color1, s=10)
        ax.scatter(x2, y2, label=label2, color=color2, s=10)
        ax.set_xlabel('Time')
        ax.set_ylabel('Value')
        ax.legend()
        ax.grid(True)

    # Create subplots for each column
    fig, axs = plt.subplots(3, 1, figsize=(10, 15))

    # Time axis (x-axis)
    time1 = range(len(data1))
    time2 = range(len(data2))

    # Scatter plot for column Cache_Misses
    scatter_single_column(axs[0], time1, data1['Cache_Misses'], time2, data2['Cache_Misses'], 'Cache_Misses from File 1', 'Cache_Misses from File 2', 'blue', 'red')
    axs[0].set_title('Column Cache_Misses Comparison')

    # Scatter plot for column Energy
    scatter_single_column(axs[1], time1, data1['Energy'], time2, data2['Energy'], 'Energy from File 1', 'Energy from File 2', 'orange', 'purple')
    axs[1].set_title('Column Energy Comparison')

    # Scatter plot for column Instructions
    scatter_single_column(axs[2], time1, data1['Instructions'], time2, data2['Instructions'], 'Instructions from File 1', 'Instructions from File 2', 'green', 'brown')
    axs[2].set_title('Column Instructions Comparison')

    # Adjust layout and save the figure

    if int(count) < 10: count = "0" + count
    plt.tight_layout()
    plt.savefig(f'./plots/{save_prefix}_{count}.png')

# Usage
if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python script_name.py <file1.csv> <file2.csv>")
        sys.exit(1)

    file1_path = sys.argv[1]
    file2_path = sys.argv[2]
    count = sys.argv[3]
    plot_columns(file1_path, file2_path, count)

