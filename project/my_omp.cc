#include <stdio.h>
#include <unistd.h>     // getpid

#include <dlfcn.h>      // dlsym
#include <vector>       // std:vector
#include <map>          // std::map
#include <fstream>      // maps
#include <sstream>      // iss
#include <algorithm>    // find
#include <cstdint>      // uintptr_t
#include <iostream>
#include <mutex>
#include <pthread.h>

#include "perf.h"
#include "energy.h"
#include "debug_util.h"
#include "predictor_py.h"

#include "MyAllocator.h"

#define ARRAY_SIZE 8192
#define NR_METRICS 10       // 9 for array sizes + 1 for number of threads

extern "C" {

typedef struct llsp_s llsp_t;

llsp_t *llsp_new(size_t count);

void llsp_add(llsp_t *llsp, const double *metrics, double target);

const double *llsp_solve(llsp_t *llsp);

double llsp_predict(llsp_t *llsp, const double *metrics);

void llsp_dispose(llsp_t *llsp);
}

enum Event {
    INSTRUCTIONS = 0,
    CACHE_MISSES = 1,
    ENERGY       = 2
};

enum Predictor {
    LLSP = 0,
    POLY = 1,
    GPR  = 2,
    NN   = 3,
    SVM  = 4,
};

std::map<uint64_t, std::string> EventNames = {{Event::INSTRUCTIONS, "Instructions"},
                                              {Event::CACHE_MISSES, "Cache-Misses"},
                                              {Event::ENERGY,       "Energy"}};
std::map<uint64_t, std::string> PredictorNames __attribute__ ((init_priority(101))) = {{Predictor::LLSP, "llsp"},
                                                                                       {Predictor::POLY, "poly"},
                                                                                       {Predictor::GPR,  "gpr"},
                                                                                       {Predictor::NN,   "nn"},
                                                                                       {Predictor::SVM,  "svm"},};

struct llsps_s {
    std::pair<llsp_s *, std::string> events[3] = {
            {llsp_new(NR_METRICS), EventNames[Event::CACHE_MISSES]},
            {llsp_new(NR_METRICS), EventNames[Event::ENERGY]},
            {llsp_new(NR_METRICS), EventNames[Event::INSTRUCTIONS]}
    };
};

const char *current_predictor;

struct python_predictors {
    std::pair<python::Predictor *, std::string> events[3] = {
            {new python::Predictor(current_predictor, NR_METRICS), EventNames[Event::CACHE_MISSES]},
            {new python::Predictor(current_predictor, NR_METRICS), EventNames[Event::ENERGY]},
            {new python::Predictor(current_predictor, NR_METRICS), EventNames[Event::INSTRUCTIONS]}
    };
};

std::mutex map_lock;
std::mutex perf_lock;
std::mutex thread_num_lock;
std::mutex accessible_and_count_lock;

int thread_num;

// map for saving mallocs after this constructor ran -> key = address, value = size
std::map<long long, size_t, std::less<long long>, MyAllocator<std::pair<const long long, size_t >>> malloc_map __attribute__ ((init_priority(101)));

// array for saving mallocs before map is initialized
std::pair<void *, size_t> malloc_info[ARRAY_SIZE];

// if having found addresses, save the position at which they have to be hand over to the predictor
std::map<long long, std::pair<int, size_t>> found_addresses_map;

// solvers for prediction
std::map<void (*)(void *), llsps_s> llsp_solvers __attribute__ ((init_priority(101)));
std::map<void (*)(void *), python_predictors> python_solvers __attribute__ ((init_priority(101)));

// save each function that we learn
std::map<void (*)(void *), uint64_t> funcmap;

// perf stuff
std::unique_ptr<perf::PerfManager> perfManager __attribute__ ((init_priority(101)));
std::unique_ptr<energy::PerfMeasure> ehandle __attribute__ ((init_priority(101)));
perf::HandlePtr phandle __attribute__ ((init_priority(101)));
std::map<std::string, uint64_t> perf_results;
std::map<std::string, uint64_t> thread_results __attribute__ ((init_priority(101)));

// files
std::ofstream monitoring_file __attribute__ ((init_priority(101)));
std::ofstream progress_file __attribute__ ((init_priority(101)));
std::map<uint64_t, std::ofstream *> measurements;
std::map<uint64_t, std::ofstream *> predictions;

int count = 0;
bool accessible = false;
bool running = true;

void getStackBounds(uintptr_t &stack_start, uintptr_t &stack_end) {
    std::ifstream maps("/proc/self/maps");
    std::string line;

    while (std::getline(maps, line)) {
        if (line.find("[stack]") != std::string::npos) {
            std::istringstream iss(line);
            std::string address_range;
            iss >> address_range;
            sscanf(address_range.c_str(), "%lx-%lx", &stack_start, &stack_end);
            break;
        }
    }
}

int get_nr_data_elems(void *data) {     // look for the number of elements we can access until end of stack is reached

    if (data == 0) {
        return 0;
    }

    uintptr_t begin;
    uintptr_t end;

    getStackBounds(begin, end);

    // check if data on stack (theoretically also on heap possible)
    if(!((reinterpret_cast<uintptr_t> (data) >= begin) && (reinterpret_cast<uintptr_t> (data) <= end))){
        LOGGER->warning("Data element not on stack\n");
        return 0;
    }

    long long size = end - reinterpret_cast<uintptr_t> (data);
    int num_elems = size / sizeof(long long *);

    return num_elems;
}

std::vector<long long> found_addresses(void *data) {
    std::vector<long long> addresses;
    auto my_data = (long long *) data;
    int num_elems = get_nr_data_elems(data);    // check how many elements we can access until stack ends
    for (int i = 0; i < num_elems; i++) {
        if (malloc_map.contains(my_data[i])) {  // if malloc has seen this address
            if(found_addresses_map.size() < (NR_METRICS - 1) && !found_addresses_map.contains(my_data[i])) found_addresses_map[my_data[i]] = std::make_pair(found_addresses_map.size(), malloc_map[my_data[i]]);
            addresses.push_back(my_data[i]);    // save it as found address
            if (addresses.size() == NR_METRICS - 1) break;
        }
    }
    return addresses;
}

int already_there(void *address) {      // check at which position the address is already in the map and if so return the position
    std::pair<void *, size_t> element;
    element.first = address;
    for (int i = 0; i < count; i++) {
        if (malloc_info[i].first == element.first) return i;
    }
    return -1;
}

void start_up_perf() {
    perfManager = std::make_unique<perf::PerfManager>();
    auto tmp = perfManager->open(getpid());
    if (tmp) {
        phandle = std::move(tmp.value());
    } else {
        LOGGER->warning("Failed to initialize perf measurement\n");
        exit(1);
    }
    ehandle = std::make_unique<energy::PerfMeasure>();
    for (const auto &name: {"Instructions", "Cache-Misses", "Energy"}) {
        thread_results[name] = 0;
    }
}

double *get_metrics(void *data) {
    double *ret = (double *) calloc(NR_METRICS, sizeof(double));
    std::vector<long long> address_sizes = found_addresses(data);   // look for addresses that can be found on the stack
    thread_num_lock.lock();
    ret[0] = thread_num;    // get the number of threads that are currently used and use it as the first metric
    thread_num_lock.unlock();
    for (auto address : address_sizes){
        if(found_addresses_map.contains(address))   // if we have seen the address before (through malloc)
            ret[found_addresses_map[address].first + 1] = (double ) found_addresses_map[address].second; // use the corresponding size as metric (always at the same position)
    }
    return ret;
}

void create_csvs() {    // create a measurements and predictions file with the name of the to be measured values as header
    std::string size = std::to_string(funcmap.size() + 1);
    if (funcmap.size() + 1 < 10) size.insert(0, "0");
    std::ofstream *newMeasurementFile = new std::ofstream("./csvs/measurements/" + size + ".csv");
    std::ofstream *newPredictionFile = new std::ofstream("./csvs/predictions/" + size + ".csv");
    if (!newMeasurementFile->is_open() || !newPredictionFile->is_open()) {
        std::cout << "failed to open a file" << std::endl;
        exit(1);
    }
    (*newMeasurementFile) << "Cache_Misses,Energy,Instructions," << std::endl;
    (*newPredictionFile) << "Cache_Misses,Energy,Instructions," << std::endl;
    measurements[funcmap.size() + 1] = newMeasurementFile;
    predictions[funcmap.size() + 1] = newPredictionFile;
}

void predict_and_start_perf(void (*fn)(void *), void *data) {
    if (!funcmap.contains(fn)) {           // if we see a new function (= new loop), then save it
        create_csvs();                     // create a measurement and prediction csvs for each new function
        if (current_predictor == PredictorNames[Predictor::LLSP]) llsp_solvers[fn] = llsps_s(); // if LLSP should be used, create a new llsp solver for each new function
        else python_solvers[fn] = python_predictors(); // if a python predictor should be used, create a new python solver for each new function
        funcmap[fn] = funcmap.size() + 1;   // assign this function pointer an ID (1, 2, 3,...)
    }
    std::cout << "here in func: " << funcmap[fn] << std::endl;
    progress_file << funcmap[fn];   // save which function is executed currently

    double *metrics;

    if (current_predictor == PredictorNames[Predictor::LLSP]) {     // if the LLSP should be used
        for (auto solver: llsp_solvers[fn].events) {    // for each metric the solver for the current function should make a prediction
            metrics = get_metrics(data);    // get workload metrics
            double predicted = llsp_predict(solver.first, metrics);
            (*predictions[funcmap[fn]]) << predicted << ",";    // save the predictions in a file for later evaluation
            printf("predicted for %s: %f\n", solver.second.c_str(), predicted);
        }
    } else {     // if a python predictor should be used
        for (auto solver: python_solvers[fn].events) {
            metrics = get_metrics(data);
            double predicted = solver.first->predict(metrics, NR_METRICS);
            (*predictions[funcmap[fn]]) << predicted << ",";
            printf("predicted for %s: %f\n", solver.second.c_str(), predicted);
        }
    }

    for(int i = 0; i < NR_METRICS; i++){
        progress_file << "," << metrics[i];     // save the workload metrics that were used for post-mortem analysis
    }
    free(metrics);
    progress_file << std::endl;
    
    (*predictions[funcmap[fn]]) << std::endl;

    perf_lock.lock();       // lock because monitoring thread also currently accesses the handles
    perf_results = phandle->read();     // read out the current perf values to calculate the difference after the function execution
    perf_results[EventNames[Event::ENERGY]] = ehandle->read();
    perf_lock.unlock();
}

void end_perf_and_feed_predictor(void (*fn)(void *), void *data) {
    perf_lock.lock();
    auto perf_reading_end = phandle->read();    // read out the current perf values to calculate the difference
    auto energy_reading_end = ehandle->read();
    perf_lock.unlock();

    std::cout << "Performance results: " << std::endl;

    if (current_predictor == PredictorNames[Predictor::LLSP]) {     // if LLSP
        for (auto solver: llsp_solvers[fn].events) {    // for each perf value
            double result;
            if (solver.second == EventNames[Event::ENERGY]) {
                result = (double) (energy_reading_end - perf_results[solver.second]);   // calculate the difference
            } else {
                result = (double) (perf_reading_end[solver.second] - perf_results[solver.second]);
            }
            std::cout << " " << solver.second << " -> " << result << std::endl;
            (*measurements[funcmap[fn]]) << result << ",";      // save it in a file
            llsp_add(solver.first, get_metrics(data), result);      // feed the predictor with it
            llsp_solve(solver.first);
        }
    } else {
        for (auto solver: python_solvers[fn].events) {
            double result;
            if (solver.second == EventNames[Event::ENERGY]) {
                result = (double) (energy_reading_end - perf_results[solver.second]);
            } else {
                result = (double) (perf_reading_end[solver.second] - perf_results[solver.second]);
            }
            std::cout << " " << solver.second << " -> " << result << std::endl;
            (*measurements[funcmap[fn]]) << result << ",";
            solver.first->fit(get_metrics(data), NR_METRICS, result);
        }
    }

    (*measurements[funcmap[fn]]) << std::endl;
}

void clean_up_map() {   // now maps can be used and the content of the malloc array can be put in the malloc map
    for (int i = 0; i < count; i++) {
        void *address = malloc_info[i].first;
        int size = malloc_info[i].second;
        malloc_map[reinterpret_cast<long long> (address)] = size;
    }
}

void create_files() {   // for monitoring and post-mortem analysis
    monitoring_file.open("./csvs/monitoring.csv");
    progress_file.open("./csvs/progress.csv");
    if (!monitoring_file.is_open() || !progress_file.is_open()) {
        std::cout << "failed to open monitoring or progress file" << std::endl;
        exit(1);
    }
    progress_file << "Functions,Metrics,,,,,,,,," << std::endl;
}

void *perf_stuff(void *arg) {
    monitoring_file << "Cache_Misses,Energy,Instructions," << std::endl;
    while (running) {
        perf_lock.lock();
        std::map<std::string, uint64_t> new_perf_results = phandle->read();     // read out the new perf values
        new_perf_results[EventNames[Event::ENERGY]] = ehandle->read();
        perf_lock.unlock();

        for (auto &[name, val]: thread_results) {

            double result = (double) (new_perf_results[name] - val);    // calculate the difference
            monitoring_file << result << std::fixed << ",";
            thread_results[name] = new_perf_results[name];  // save the new perf values
        }
        monitoring_file << std::endl;
        usleep(50000);  // sleep for 50 ms
    }
    return nullptr;
}

extern "C" void
__attribute__((constructor(65535))) setup(void) {   // is executed after all other constructors are executed and before main starts
    current_predictor = getenv("PREDICTOR") ?: "llsp";      // get the predictor that should be used

    std::cout << "predictor: " << current_predictor << std::endl;

    if (current_predictor != PredictorNames[Predictor::LLSP]) python::init();   // if it is a python predictor, init is needed

    accessible_and_count_lock.lock();
    accessible = true;      // now the malloc map can be used
    accessible_and_count_lock.unlock();

    start_up_perf();

    clean_up_map();     // transfer the content from the malloc array to the malloc map

    create_files();

    pthread_t perf_thread;      // create the monitoring thread
    pthread_create(&perf_thread, nullptr, perf_stuff, nullptr);
}

extern "C" void
__attribute__((destructor)) teardown(void) { // is executed after program terminates
    running = false;
}

extern "C" void *
malloc(size_t __size) {
    void *address = ((void *(*)(size_t)) dlsym(RTLD_NEXT, "malloc"))(__size);   // call the real malloc
    accessible_and_count_lock.lock();
    int position = count;
    if (accessible) {   // if maps can be used
        map_lock.lock();
        malloc_map[reinterpret_cast<long long> (address)] = __size;     // safe the address + size in the malloc map
        map_lock.unlock();
    } else if (!accessible && count < ARRAY_SIZE) {     // if maps cannot yet be accessed and the array is not full
        int array_pos = already_there(address);
        if (array_pos > -1) {   // address already in the array
            position = array_pos;
            count--;    // no new address
        }
        malloc_info[position].first = address;  // safe address and size
        malloc_info[position].second = __size;
        count++;
    }
    accessible_and_count_lock.unlock();
    return address;
}

extern "C" int
omp_get_num_threads(void) {
    auto func = (int (*)(void)) dlsym(RTLD_NEXT, "omp_get_num_threads");    // call the real omp_get_num_threads
    int result = func();
    thread_num_lock.lock();
    thread_num = result;    // safe the number of threads for the prediction metrics
    thread_num_lock.unlock();
    return result;
}

extern "C" void
omp_set_num_threads(int set_thread_num) {
    auto func = (void (*)(int)) dlsym(RTLD_NEXT, "omp_set_num_threads");    // call the real omp_set_num_threads
    func(set_thread_num);
    thread_num_lock.lock();
    thread_num = set_thread_num;    // safe the number of threads for the prediction metrics
    thread_num_lock.unlock();
}

extern "C" void
GOMP_parallel(void (*fn)(void *), void *data, unsigned num_threads,
              unsigned int flags) {

    auto func = (void (*)(void (*)(void *), void *, unsigned, unsigned int)) dlsym(RTLD_NEXT, "GOMP_parallel");     // get the real GOMP_parallel

    predict_and_start_perf(fn, data);   // make predictions about the function that will be run right away and start perf for measuring

    func(fn, data, num_threads, flags); // call the function

    end_perf_and_feed_predictor(fn, data);  // end perf for feeding the actual values in the predictor

    printf("------------------------------------\n");
}
