#!/bin/bash

target=$1

predictors=("llsp" "poly" "gpr" "nn" "svm")

mkdir -p "results"

for predictor in "${predictors[@]}"; do
    PREDICTOR="$predictor" LD_PRELOAD=./build/my_omp.so $target
    ./eval.sh
    mkdir -p "results/$predictor"
    mv csvs plots "results/$predictor"
    mkdir -p "csvs"
    mkdir -p "csvs/measurements"
    mkdir -p "csvs/predictions"
done
