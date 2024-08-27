import pandas as pd
import matplotlib.pyplot as plt
import os
import glob
from matplotlib.ticker import ScalarFormatter

if not os.path.exists('plots'):
    os.makedirs('plots')

# Directories containing the CSV files
execution_order_file = './csvs/progress.csv'
measurements_dir = './csvs/measurements/'
predictions_dir = './csvs/predictions/'

# Read the execution order file and consider only the first column
execution_order = pd.read_csv(execution_order_file, usecols=['Functions'])

# Discover the function names (1, 2, 3,...) from the available files
measurement_files = glob.glob(os.path.join(measurements_dir, '*.csv'))
prediction_files = glob.glob(os.path.join(predictions_dir, '*.csv'))

# Extract function names from the filenames
measurement_functions = {os.path.splitext(os.path.basename(f))[0] for f in measurement_files}
prediction_functions = {os.path.splitext(os.path.basename(f))[0] for f in prediction_files}

# Read the measurements files into a dictionary
measurements = {func: pd.read_csv(os.path.join(measurements_dir, func + '.csv')) for func in measurement_functions}

# Read the predictions files into a dictionary
predictions = {func: pd.read_csv(os.path.join(predictions_dir, func + '.csv')) for func in prediction_functions}

# Ensure we have the same set of functions for measurements and predictions
functions = measurement_functions.intersection(prediction_functions)

# Create empty lists to store the aligned measurements, predictions, and errors
aligned_measurements = {'Cache_Misses': [], 'Energy': [], 'Instructions': []}
aligned_predictions = {'Cache_Misses': [], 'Energy': [], 'Instructions': []}
errors = {'Cache_Misses': [], 'Energy': [], 'Instructions': []}

def format_function_name(func):
    return f'{int(func):02d}'

# Iterate through the execution order and extract the corresponding measurements and predictions
for func in execution_order['Functions']:
    formatted_func = format_function_name(func)  # Format function name with leading zeros
    if formatted_func in functions:
        # Get the next measurement for this function
        func_measurements = measurements[formatted_func].iloc[0]
        # Get the next prediction for this function
        func_predictions = predictions[formatted_func].iloc[0]

        # Append the values to the aligned lists
        for metric in aligned_measurements.keys():
            aligned_measurements[metric].append(func_measurements[metric])
            aligned_predictions[metric].append(func_predictions[metric])
            # Calculate the absolute error (|measured - predicted|)
            error = abs(func_measurements[metric] - func_predictions[metric])
            errors[metric].append(error)

        # Remove the used measurement and prediction
        measurements[formatted_func] = measurements[formatted_func].iloc[1:].reset_index(drop=True)
        predictions[formatted_func] = predictions[formatted_func].iloc[1:].reset_index(drop=True)

# Create a figure with vertical subplots for the scatter plots
fig, axs = plt.subplots(nrows=3, ncols=1, figsize=(10, 18), sharex=True)

# List of metrics
metrics = ['Cache_Misses', 'Energy', 'Instructions']

# Plot each metric in a separate subplot for scatter plot
for i, metric in enumerate(metrics):
    axs[i].scatter(range(len(aligned_measurements[metric])), aligned_measurements[metric], color='blue', label='Measured', s=10, marker='o')  # s=10 for smaller dots
    axs[i].scatter(range(len(aligned_predictions[metric])), aligned_predictions[metric], color='red', label='Predicted', s=10, marker='o')  # s=10 for smaller dots
    axs[i].set_ylabel(metric)
    axs[i].set_title(f'Measured vs. Predicted {metric}')
    axs[i].legend()
    axs[i].grid(True)
    axs[i].yaxis.set_major_formatter(ScalarFormatter(useMathText=True))  # Use scientific notation for y-axis
    axs[i].ticklabel_format(style='sci', axis='y', scilimits=(0,0))

# Set common labels
axs[-1].set_xlabel('Function call')

# Adjust layout to prevent overlap
plt.tight_layout()

# Save the scatter plot
plt.savefig("plots/all_in_one.png")

# Now create a separate figure for the box plots of errors
fig, axs = plt.subplots(nrows=1, ncols=3, figsize=(18, 6))

# Plot each metric in a separate subplot for box plots of absolute errors without outliers
for i, metric in enumerate(metrics):
    axs[i].boxplot(errors[metric], showfliers=False)  # showfliers=False to hide outliers
    axs[i].set_title(f'Absolute Error Distribution for {metric}')
    axs[i].set_ylabel('Absolute Error')
    axs[i].set_xticklabels([])
    axs[i].grid(True)
    axs[i].yaxis.set_major_formatter(ScalarFormatter(useMathText=True))  # Use scientific notation for y-axis
    axs[i].ticklabel_format(style='sci', axis='y', scilimits=(0,0))

# Adjust layout to prevent overlap
plt.tight_layout()

# Save the box plot
plt.savefig("plots/box_plots.png")
