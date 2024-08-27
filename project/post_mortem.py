from scripts import all_in_one
import predictor
import sys
import os
import csv
import matplotlib.pyplot as plt
from matplotlib.ticker import ScalarFormatter
from sklearn.model_selection import train_test_split

predictors = {}

def read_metrics_from_csv(file_path):
    """
    Reads the metrics from the CSV file.

    :param file_path: Path to the CSV file.
    :return: List of lists containing metrics.
    """
    metrics = []
    with open(file_path, mode='r') as file:
        reader = csv.reader(file)
        next(reader)  # Skip the header
        for row in reader:
            metrics.append([float(value) for value in row])  # For using one predictor for all functions: skip the first column => row[1:]
    return metrics

my_metrics = read_metrics_from_csv('csvs/progress.csv')

def use_predictor(predictor_name):
    num_events = len(all_in_one.aligned_measurements)
    fig, axs = plt.subplots(num_events, 1, figsize=(10, 6 * num_events))

    if num_events == 1:
        axs = [axs]  # Ensure axs is always iterable if there's only one subplot

    # Dictionary to store absolute errors for each event
    absolute_errors = {event: [] for event in all_in_one.aligned_measurements}

    for i, event in enumerate(all_in_one.aligned_measurements):
        # Create the predictor
        predictors[event] = predictor.create(predictor_name)
        # Get the measured values
        result_values = all_in_one.aligned_measurements[event]

        # Split measured values in 80% training set and 20% test set
        x_train, x_test, y_train, y_test = train_test_split(my_metrics, result_values, test_size=0.2, random_state=42)

        # Train the predictor
        predictor.fit(predictors[event], x_train, y_train)
        # Get the predictions
        predictions = predictor.predict(predictors[event], x_test)

        # Calculate absolute errors
        abs_errors = [abs(y_true - y_pred) for y_true, y_pred in zip(y_test, predictions)]
        absolute_errors[event].extend(abs_errors)

        # Create a scatter plot for the event
        axs[i].scatter(range(len(y_test)), y_test, color='blue', label='Measured', s=10, marker='o')
        axs[i].scatter(range(len(predictions)), predictions, color='red', label='Predicted', s=10, marker='o')
        axs[i].set_xlabel('Test Sample Index')
        axs[i].set_ylabel(event)
        axs[i].set_title(f'Measured vs. Predicted {event}')
        axs[i].legend()

        # Set y-axis to scientific notation
        axs[i].yaxis.set_major_formatter(ScalarFormatter(useMathText=True))
        axs[i].ticklabel_format(style='sci', axis='y', scilimits=(0,0))

    plt.tight_layout()
    plt.savefig(f'./post_mortem/scatters.png')

    # Plot box plots for absolute errors
    fig, axs = plt.subplots(nrows=1, ncols=num_events, figsize=(6 * num_events, 6))
    if num_events == 1:
        axs = [axs]  # Ensure axs is always iterable if there's only one subplot

    for i, event in enumerate(all_in_one.aligned_measurements):
        axs[i].boxplot(absolute_errors[event], showfliers=False)  # showfliers=False to hide outliers
        axs[i].set_title(f'Absolute Error Distribution for {event}')
        axs[i].set_ylabel('Absolute Error')
        axs[i].set_xticklabels([])
        axs[i].grid(True)

        # Set y-axis to scientific notation
        axs[i].yaxis.set_major_formatter(ScalarFormatter(useMathText=True))
        axs[i].ticklabel_format(style='sci', axis='y', scilimits=(0,0))

    # Adjust layout for box plots and save
    plt.tight_layout()
    plt.savefig('./post_mortem/box_plots.png')

def main():
    os.makedirs('./post_mortem', exist_ok=True)

    predictor_name = "gpr"
    if len(sys.argv) >= 2:
        predictor_name = sys.argv[1]
        print(f"Using {predictor_name}")
    else:
        print(f"Using default: {predictor_name}")

    use_predictor(predictor_name)

if __name__ == "__main__":
    main()
