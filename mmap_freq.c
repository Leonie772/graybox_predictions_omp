#include <stdio.h>
#include <linux/perf_event.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <omp.h>
#include <time.h>

#define PAGE_SIZE 4096
#define BUFFER_PAGES 8
#define ARRAY_SIZE 1000000

static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags) {
    return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct perf_format {
    uint64_t nr;
    struct {
        uint64_t value;
        u_int64_t id;
    } values[];
};

int
main(void) {
    int fd_leader, fd_cache_misses, fd_bus;
    struct perf_event_attr pe;
    void *mmap_buf;
    size_t mmap_len = (BUFFER_PAGES + 1) * PAGE_SIZE;  // +1 for metadata page
    struct perf_event_mmap_page *header;

    // Initialize the perf_event_attr structure for the leader event
    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.disabled = 1;  // Start disabled
    pe.exclude_kernel = 1;  // Don't count kernel instructions
    pe.exclude_hv = 1;  // Don't count hypervisor instructions
    pe.sample_freq = 20;  // Aim for 20 samples per second
    pe.freq = 1;  // Use frequency mode instead of period mode
    pe.sample_type = PERF_SAMPLE_READ;  // Only collect the sample values
    pe.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;  // Read grouped values with IDs

    // Open the leader event for counting instructions
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    fd_leader = perf_event_open(&pe, 0, -1, -1, 0);
    if (fd_leader == -1) {
        perror("Error opening leader");
        exit(EXIT_FAILURE);
    }

    // Open the cache misses event and attach it to the leader
    pe.config = PERF_COUNT_HW_CACHE_MISSES;
    fd_cache_misses = perf_event_open(&pe, 0, -1, fd_leader, 0);
    if (fd_cache_misses == -1) {
        perror("Error opening cache misses event");
        close(fd_leader);
        exit(EXIT_FAILURE);
    }

    pe.config = PERF_COUNT_HW_BUS_CYCLES;
    fd_bus = perf_event_open(&pe, 0, -1, fd_leader, 0);
    if (fd_bus == -1) {
        perror("Error opening bus event");
        close(fd_leader);
        close(fd_cache_misses);
        exit(EXIT_FAILURE);
    }

    // Set up mmap for the event
    mmap_buf = mmap(NULL, mmap_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd_leader, 0);
    if (mmap_buf == MAP_FAILED) {
        perror("mmap");
        close(fd_leader);
        close(fd_cache_misses);
        close(fd_bus);
        exit(EXIT_FAILURE);
    }

    header = (struct perf_event_mmap_page *) mmap_buf;

    // Start counting
    ioctl(fd_leader, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    ioctl(fd_leader, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    
    // Simulate some work
    int *array = (int *) malloc(ARRAY_SIZE * sizeof(int));

    long long sum = 14;
    int sum2 = 0;

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

    // Stop counting
    ioctl(fd_leader, PERF_EVENT_IOC_DISABLE, 0);

    /* Extract the data from perf */
    while (header->data_head != header->data_tail) {
        struct perf_event_header *event;
        __sync_synchronize();  // Memory barrier

        // Pointer to the start of the data in the ring buffer
        char *data = (char *) header + PAGE_SIZE;

        // Event starts at data_tail
        event = (struct perf_event_header *) (data + (header->data_tail % (BUFFER_PAGES * PAGE_SIZE)));

        // Process the event here
        if (event->type == PERF_RECORD_SAMPLE) {
            char buf[4096];
            struct perf_format *formatted_buf = (struct perf_format *) buf;
            // Read the values for the events
            memcpy(buf, (char *) event + sizeof(struct perf_event_header), sizeof(buf));

            printf("nr elems: %lu\n", formatted_buf->nr);
            for (uint64_t i = 0; i < formatted_buf->nr; ++i) {
                printf("ID: %lu, value: %lu\n", formatted_buf->values[i].id, formatted_buf->values[i].value);
            }
        }

        // Update the tail to the next event
        header->data_tail += event->size;
        header->data_tail &= (BUFFER_PAGES * PAGE_SIZE) - 1;  // Wrap around the buffer
    }

    // Cleanup
    munmap(mmap_buf, mmap_len);
    close(fd_leader);
    close(fd_cache_misses);
    close(fd_bus);
    
    return 0;
}

