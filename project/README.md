# Analysis of the LLSP predictor and comparison of prediction approaches for OpenMP-based applications

This project uses the LLSP predictor from ATLAS [[1]](#[1]) to make predictions about program behaviour, e.g. how many instructions it will use or how much energy it will need. The project is designed to be used for the prediction of OpenMP-based applications, especially the NAS Parallel Benchmarks [[2]](#[2]). As an alternative to the LLSP predictor, there is also the option to choose from a set of python predictors from the scikit-learn library [[3]](#[3]) to make the predictions.

## Features

- online prediction of performance counters and energy consumption with the LLSP predictor and the python predictors
- post-mortem prediction with the python predictors
- monitoring the used performance counters and energy consumption during program execution
- evaluation of the results with scatter and box plots

## Usage

### Building

```bash
make
```
- will build the project and prepare it for the first run
    - creates a *build* directory with
        - a *test* program and
        - a shared library *my_omp.so* that will be linked to the *test* program or to the respective benchmark that should be executed
    - creates a *csvs* directory for
      - the measurements and
      - the predictions

### Online Prediction

```bash
LD_PRELOAD=./build/my_omp.so ./build/test
```
- links the shared library to the test program and starts the test program
- makes predictions with the LLSP predictor
- fills the *csvs* directory which is needed for the [Evaluation](#evaluation)
- to run other programs, e.g. one of the [NAS Parallel Benchmarks](#NAS-Parallel-Benchmarks), replace the *./build/test* by the path to the respective program
```bash
LD_PRELOAD=./build/my_omp.so ./NAS/NPB3.4.2/NPB3.4-OMP/bin/bt.B.x
```
- to use a different predictor, add the environment variable *PREDICTOR* and set it to one of the [available predictors](#available-predictors)

```bash
PREDICTOR="nn" LD_PRELOAD=./build/my_omp.so ./NAS/NPB3.4.2/NPB3.4-OMP/bin/bt.B.x
```
- to let all predictors run the same program, use the *run_all_predictors.sh* file
```bash
./run_all_predictors.sh ./NAS/NPB3.4.2/NPB3.4-OMP/bin/bt.B.x
```
- it creates a *results* directory and for each predictor a directory with csv files and plots

### Post-Mortem Prediction

```bash
python3 post_mortem.py
```
- starts a post-mortem prediction with the gpr predictor as default
- creates a *post_mortem* directory with scatter and box plots
- creates a *plots* directory with scatter and box plots for the program that was run before
- to use some of the other [available python predictors](#available-predictors), add them as a second argument
```bash
python3 post_mortem.py poly
```

### Evaluation

- if using the post-mortem prediction or the *run_all_predictors.sh* file, the evaluation is already done
- if running a single online prediction, it can be evaluated via the *eval.sh* file
```bash
./eval.sh
```
- creates a plots directory with
  - a scatter plot for each parallel region that was found during program execution
  - a scatter plot with all parallel regions in the order they were executed
  - a box plot that shows the absolute error of the predictions
  - a monitoring plot

### Cleanup

```bash
make clean
```
- removes
  - the *csvs* directory
  - the *plots* directory
  - the *post_mortem* directory
```bash
make clean_all
```
- removes
    - the *csvs* directory
    - the *plots* directory
    - the *post_mortem* directory
    - the *build* directory
    - the *\_\_pycache\_\_* directory

## Hints

- To enable the perf measurements that are used to train the predictor
  - Linux 3.14 is required and
  - the value that is in the */proc/sys/kernel/perf_event_paranoid* file needs to be less than 1. This can be done via:
```bash
echo "0" | sudo tee /proc/sys/kernel/perf_event_paranoid
```
- The <a id="NAS-Parallel-Benchmarks"></a>NAS Parallel Benchmarks can be found in the *NAS* directory. There are separate READMEs for building them.
- <a id="available-predictors"></a>available predictors:
  - llsp
  - python predictors
    - poly
    - gpr
    - nn
    - svm
- The post-mortem prediction needs an online prediction to be run before. 
- The monitoring has a resolution of 20 measurements per second.
- Before running a new online or post-mortem prediction, it is recommended to do a *make clean* before or to move the affected files in a separate directory

## Sources

<a id="[1]"></a>[1] Michael Roitzsch, Stefan Wächtler, and Hermann Härtig. “Atlas: Look-ahead
scheduling using workload metrics.”

<a id="[2]"></a>[2] https://www.nas.nasa.gov/software/npb.html

<a id="[3]"></a>[3] https://scikit-learn.org/stable/index.html
