#include "energy.h"
#include "perf.h"
#include "debug_util.h"
#include <iostream>

#include <unistd.h>
#include <omp.h>

#define ARRAY_SIZE 1000

int main(int argc, char *argv[]) {
    // Example program how to use perf

    /*auto perf = std::make_unique<perf::PerfManager>();
    perf::HandlePtr phandle;
    if (auto tmp = perf->open(getpid())) {
        phandle = std::move(tmp.value());
    } else {
        LOGGER->warning("Failed to initialize perf measurement\n");
        return 1;
    }

    auto energy = std::make_unique<energy::PerfMeasure>();

    auto perf_reading_start = phandle->read();
    auto energy_reading_start = energy->read();

//     My measure loop
    for (long  long int i = 0; i < 1000000000; ++i) {

    }


    auto perf_reading_end = phandle->read();
    auto energy_reading_end = energy->read();

    std::cout << "Performance results: " << std::endl;
    for (auto &[name, val]: perf_reading_end) {
        std::cout << " " << name << " -> " << val - perf_reading_start[name] << std::endl;
    }

    std::cout << "Energy result: " << energy_reading_end - energy_reading_start << " uJ" << std::endl;*/



    // OpenMP program

    int *array = (int *) malloc(ARRAY_SIZE * sizeof(int));

    long long sum = 14;
    int sum2 = 0;

    printf("array = %p\n", array);
    printf("sum = %lld\n", sum);
    printf("------------------------------------\n");

    // Seed the random number generator
    srand(time(NULL));

    // Initialize the array with random values
    for (int i = 0; i < ARRAY_SIZE; i++) {
        array[i] = rand() % 100; // Random values between 0 and 99
    }

    // Parallel region
    for (int j = 0; j < 2; j++) {
#pragma omp parallel reduction(+:sum) shared(array)
        {
#pragma omp for
            for (int i = 0; i < ARRAY_SIZE; i++) {
                sum += array[i];
            }
        }
    }

#pragma omp parallel for num_threads(2)

    for (int i = 0; i < 100; i++) {
        sum2++;
    }


    printf("Sum of array elements: %lld\n", sum);
    printf("Sum2 of array elements: %d\n", sum2);

    free(array);

    return 0;
}
