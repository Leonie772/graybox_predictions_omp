#!/bin/bash

# Parent and plot directory path
parent_directory="csvs"
plot_directory="plots"

mkdir $plot_directory

# Directories within parent directory
files1_dir="$parent_directory/measurements"
files2_dir="$parent_directory/predictions"

# Check if parent directory exists
if [ ! -d "$parent_directory" ]; then
    echo "Parent directory $parent_directory does not exist."
    exit 1
fi

# Check if both directories exist within the parent directory
if [ ! -d "$files1_dir" ]; then
    echo "Directory $files1_dir does not exist within $parent_directory."
    exit 1
fi

if [ ! -d "$files2_dir" ]; then
    echo "Directory $files2_dir does not exist within $parent_directory."
    exit 1
fi

# Get list of files in directories
files1=("$files1_dir"/*)
files2=("$files2_dir"/*)

# Check if both lists have the same number of files
if [ "${#files1[@]}" -ne "${#files2[@]}" ]; then
    echo "Number of files in $files1_dir does not match number of files in $files2_dir."
    exit 1
fi

count=1

# Iterate over the index of files array
for (( i=0; i<${#files1[@]}; i++ ))
do
    file1="${files1[$i]}"
    file2="${files2[$i]}"
    
    # Check if both are regular files
    if [ -f "$file1" ] && [ -f "$file2" ]; then
        echo "Processing $file1 and $file2"
        python3 scripts/eval_predictions.py "$file1" "$file2" "$count"
        ((count++))
    else
        echo "Skipping $file1 and $file2 as one or both files are not regular files."
    fi
done

python3 scripts/eval_monitoring.py
python3 scripts/all_in_one.py
