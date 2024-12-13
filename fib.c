#include <omp.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>    // For pow()
#include "fib.h"

size_t BATCH_SIZE = 4;

void handler(int sig) {
    void *array[10];
    size_t size;

    size = backtrace(array, 10);
    fprintf(stderr, "Error: signal %d:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

long long fib(int n) {
    long long x, y;
    if (n < 2) return n;

    #pragma omp task shared(x) firstprivate(n)
    x = fib(n - 1);

    #pragma omp task shared(y) firstprivate(n)
    y = fib(n - 2);

    #pragma omp taskwait
    return x + y;
}

// Recursive function to generate numbers using divide and conquer
void generateNumbers(int start, int end) {
    //if (start > end) {
    //    return; // Base case: no numbers to print
    //}
     if (end - start < BATCH_SIZE) {
     //if (start == end) {
        for (int i=start;i<=end;i++)
        	printf("%d\n", i); // Print a single number
        return;
    }

    int mid = (start + end) / 2;

    // Recursive calls for the left and right halves
    #pragma omp task shared(start, mid) // Task for the left half
    generateNumbers(start, mid);    // Generate numbers in the left half
    
    #pragma omp task shared(mid, end) // Task for the right half
    generateNumbers(mid + 1, end); // Generate numbers in the right half
    
    #pragma omp taskwait // Wait for both tasks to complete
}


static long long result;
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <n> <num_threads>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    if (num_threads <= 0) {
        fprintf(stderr, "Number of threads must be a positive integer.\n");
        return 1;
    }

	printf("Numbers from 0 to %d:\n",n);

    omp_set_num_threads(num_threads);

    double itime, ftime;
    clock_t start, end;

    itime = omp_get_wtime();
    start = clock();

    #pragma omp parallel
    {
        #pragma omp single
        {
            //result = fib(n);
            
    		generateNumbers(0,n); // Start with the range 0 to 16
    	

        }
    }

    end = clock();
    ftime = omp_get_wtime();

    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;
    double wall_time_used = ftime - itime;

    // Compute the number of function calls as 2^n
    double calls = pow(2.0, (double)n);
    double calls_per_sec = calls / wall_time_used;

    printf("Result is: %lld\n", result);
    printf("Number of threads used: %d\n", num_threads);
    printf("CPU clock time used: %f\n", cpu_time_used);
    printf("Wall-clock time used: %f\n", wall_time_used);
    printf("Function calls (approx): %.0f\n", calls);
    printf("Function calls per second: %f\n", calls_per_sec);

    //#pragma omp parallel
    //xomp_perflog_dump();

    return 0;
}