#define _GNU_SOURCE

#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <inttypes.h>
#include <semaphore.h>

/* MACRO DEFINITIONS */
//#define AFFINITY_SET

#define WARM_UP             (1000U)

#define SHM_NAME                "/ipc_shm_benchmark"
#define SEM_PRODUCER_NAME       "/ipc_sem_message_producer"
#define SEM_CONSUMER_NAME       "/ipc_sem_message_consumer"

/* FUNCTION PROTOTYPES */
static void producer(void);
static void consumer(void);
static int parse_command_line_arguments(int argc, char* argv[]);
static long double standard_deviation(uint64_t *arr, int n);
static double calculate_median(uint64_t *data, size_t n);
static int compare_uint64(const void *a, const void *b);
static void exit_sys(const char *msg);

/* GLOBAL VARIABLES */
pid_t pid;
uint32_t number_of_message;
struct sched_param consumer_param;
struct sched_param producer_param;

int shm_fd = -1;
uint64_t *shared_mem = NULL;
sem_t *sem_message_producer = NULL;
sem_t *sem_message_consumer = NULL;

/*
 * Usage:
 * ./shm <number_of_messages> <consumer_priority> <producer_priority>
 */

int main(int argc, char* argv[])
{
    if (parse_command_line_arguments(argc, argv) == -1)
        return -1;

    shm_unlink(SHM_NAME);
    sem_unlink(SEM_PRODUCER_NAME);
    sem_unlink(SEM_CONSUMER_NAME);

    if((shm_fd = shm_open(SHM_NAME, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH ) )== -1)
        exit_sys("shm_open");

    if (ftruncate(shm_fd, sizeof(uint64_t)) == -1)
        exit_sys("ftruncate");

    if((shared_mem = mmap(NULL, sizeof(uint64_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0)) == MAP_FAILED)
        exit_sys("mmap");

    memset(shared_mem, 0, sizeof(uint64_t));

    if((sem_message_producer = sem_open(SEM_PRODUCER_NAME, O_CREAT | O_EXCL, 0600, 0)) == SEM_FAILED)
       exit_sys("sem_open sem_message_producer");

    if((sem_message_consumer = sem_open(SEM_CONSUMER_NAME, O_CREAT | O_EXCL, 0600, 1)) == SEM_FAILED)
       exit_sys("sem_open sem_message_consumer");

    if ((pid = fork()) == -1)
        exit_sys("fork");

    if (pid != 0) /* Producer */
    {
        struct timespec start, end;

#ifdef AFFINITY_SET
        cpu_set_t producer_cpu_set;
        CPU_ZERO(&producer_cpu_set);
        CPU_SET(1, &producer_cpu_set);

        if (sched_setaffinity(0, sizeof(producer_cpu_set), &producer_cpu_set) == -1)
            exit_sys("sched_setaffinity producer");

        printf("#CPU AFFINITY IS AVAILABLE\n");
#endif

        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
            exit_sys("mlockall producer");

        if (sched_setscheduler(0, SCHED_FIFO, &producer_param) == -1)
            exit_sys("sched_setscheduler producer");

        if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
            exit_sys("clock_gettime start");

        producer();

        waitpid(pid, NULL, 0);

        if (clock_gettime(CLOCK_MONOTONIC, &end) == -1)
            exit_sys("clock_gettime end");

        printf("Total elapsed time (ns) : %llu\n",
               (unsigned long long)(
                   (((uint64_t)end.tv_sec * 1000000000ULL) + (uint64_t)end.tv_nsec) -
                   (((uint64_t)start.tv_sec * 1000000000ULL) + (uint64_t)start.tv_nsec)
               ));

        sem_close(sem_message_producer);
        sem_close(sem_message_consumer);
        sem_unlink(SEM_PRODUCER_NAME);
        sem_unlink(SEM_CONSUMER_NAME);

        munmap(shared_mem, sizeof(uint64_t));
        close(shm_fd);
        shm_unlink(SHM_NAME);
    }
    else /* Consumer */
    {
#ifdef AFFINITY_SET
        cpu_set_t consumer_cpu_set;
        CPU_ZERO(&consumer_cpu_set);
        CPU_SET(2, &consumer_cpu_set);

        if (sched_setaffinity(0, sizeof(consumer_cpu_set), &consumer_cpu_set) == -1)
            exit_sys("sched_setaffinity consumer");
#endif

        if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1)
            exit_sys("mlockall consumer");

        if (sched_setscheduler(0, SCHED_FIFO, &consumer_param) == -1)
            exit_sys("sched_setscheduler consumer");

        consumer();

        sem_close(sem_message_producer);
        sem_close(sem_message_consumer);

        munmap(shared_mem, sizeof(uint64_t));
        close(shm_fd);
    }

    return 0;
}

void producer(void)
{
    struct timespec ts_start_time;
	
    for (uint32_t i = 0; i < number_of_message + WARM_UP; ++i)
    {
        if(sem_wait(sem_message_consumer) == -1)
            exit_sys("sem_wait");

        if (clock_gettime(CLOCK_MONOTONIC, &ts_start_time) == -1)
              exit_sys("clock_gettime producer");

        *shared_mem = ((uint64_t)ts_start_time.tv_sec * 1000000000ULL) + (uint64_t)ts_start_time.tv_nsec;

        if(sem_post(sem_message_producer) == -1)
            exit_sys("sem_post");
    }
}

void consumer(void)
{
    struct timespec ts_recv_time;
    uint64_t *elapsed_time = NULL;
    uint64_t local_sequence = 0;

    if ((elapsed_time = calloc(number_of_message, sizeof(uint64_t))) == NULL)
        exit_sys("calloc elapsed_time");

    while (local_sequence < (uint64_t)(number_of_message + WARM_UP))
    {
        if(sem_wait(sem_message_producer) == -1)
            exit_sys("sem_wait");

        if (clock_gettime(CLOCK_MONOTONIC, &ts_recv_time) == -1)
            exit_sys("clock_gettime consumer");

        if (local_sequence >= WARM_UP)
        {
            uint64_t recv_ns = ((uint64_t)ts_recv_time.tv_sec * 1000000000ULL) + (uint64_t)ts_recv_time.tv_nsec;
            elapsed_time[local_sequence - WARM_UP] = recv_ns - *shared_mem;
        }

        local_sequence++;

        if(sem_post(sem_message_consumer) == -1)
            exit_sys("sem_post");

    }

    uint64_t min_val = elapsed_time[0];
    uint64_t max_val = elapsed_time[0];
    long double average = 0;

    for (uint32_t i = 0; i < number_of_message; ++i)
    {
        printf("%llu\n", (unsigned long long)elapsed_time[i]);

        average += elapsed_time[i];

        if (elapsed_time[i] < min_val)
            min_val = elapsed_time[i];

        if (elapsed_time[i] > max_val)
            max_val = elapsed_time[i];
    }

    printf("Min: %llu\n", (unsigned long long)min_val);
    printf("Max: %llu\n", (unsigned long long)max_val);
    printf("Average: %.3Lf\n", average / number_of_message);
    printf("Standard Deviation: %.3Lf\n", standard_deviation(elapsed_time, number_of_message));
    printf("Median: %.3Lf\n", (long double)calculate_median(elapsed_time, number_of_message));


    free(elapsed_time);
}

int parse_command_line_arguments(int argc, char* argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "INVALID ARGUMENTS\n");
        return -1;
    }

    errno = 0;
    number_of_message = strtoul(argv[1], NULL, 10);

    if (errno != 0)
        exit_sys("strtoul number_of_message");

    if (number_of_message < 1)
    {
        fprintf(stderr, "NUMBER OF MESSAGE INVALID VALUE\n");
        return -1;
    }

    printf("#number of message : %u\n", number_of_message);

    errno = 0;
    consumer_param.sched_priority = strtoul(argv[2], NULL, 10);

    if (errno != 0)
        exit_sys("strtoul consumer_priority");

    if ((consumer_param.sched_priority > 99) ||
        (consumer_param.sched_priority == 0))
    {
        fprintf(stderr, "CONSUMER PRIORITY FAILURE\n");
        return -1;
    }

    printf("#consumer priority : %d\n", consumer_param.sched_priority);

    errno = 0;
    producer_param.sched_priority = strtoul(argv[3], NULL, 10);

    if (errno != 0)
        exit_sys("strtoul producer_priority");

    if ((producer_param.sched_priority > 99) ||
        (producer_param.sched_priority == 0))
    {
        fprintf(stderr, "PRODUCER PRIORITY FAILURE\n");
        return -1;
    }

    printf("#producer priority : %d\n", producer_param.sched_priority);

    fflush(stdout);

    return 0;
}

long double standard_deviation(uint64_t *arr, int n)
{
    if (n < 2)
        return 0.0;

    long double sum = 0;

    for (int i = 0; i < n; i++)
        sum += arr[i];

    long double average = sum / n;
    long double square_sum = 0.0;

    for (int i = 0; i < n; i++)
    {
        long double diff = arr[i] - average;
        square_sum += diff * diff;
    }

    long double variance = square_sum / (n - 1);

    return sqrtl(variance);
}

static int compare_uint64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;

    if (x < y)
        return -1;

    if (x > y)
        return 1;

    return 0;
}

double calculate_median(uint64_t *data, size_t n)
{
    if (data == NULL || n == 0)
    {
        fprintf(stderr, "INVALID INPUT FOR MEDIAN\n");
        exit(EXIT_FAILURE);
    }

    qsort(data, n, sizeof(uint64_t), compare_uint64);

    if (n % 2 == 1)
    {
        return (double)data[n / 2];
    }
    else
    {
        return ((double)data[(n / 2) - 1] + (double)data[n / 2]) / 2.0;
    }
}

void exit_sys(const char *msg)
{
    perror(msg);

    if (sem_message_producer != NULL && sem_message_producer != SEM_FAILED)
        sem_close(sem_message_producer);

    if (sem_message_consumer != NULL && sem_message_consumer != SEM_FAILED)
        sem_close(sem_message_consumer);

    sem_unlink(SEM_PRODUCER_NAME);
    sem_unlink(SEM_CONSUMER_NAME);

    if (shared_mem != NULL && shared_mem != MAP_FAILED)
        munmap(shared_mem, sizeof(uint64_t));

    if (shm_fd != -1)
        close(shm_fd);

    shm_unlink(SHM_NAME);

    exit(EXIT_FAILURE);
}
