#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <getopt.h>

typedef int sort_t;

static void merge(sort_t * A, size_t q, size_t r, sort_t * s)
{
	register size_t i, j;
	size_t k, n1, n2;

	n1 = q + 1;
	n2 = r - q;

	sort_t * L = s;
	sort_t * R = s + n1 + 1; /* +1 for INT_MAX terminator */

	memcpy(L, A, sizeof(int) * n1);
	memcpy(R, A+q+1, sizeof(int) * n2);

	L[n1] = INT_MAX;
	R[n2] = INT_MAX;

	i = 0;
	j = 0;

	for(k = 0; k <= r; k++) {
		if(L[i] <= R[j]) {
			A[k] = L[i++];
		} else {
			A[k] = R[j++];
		}
	}
}

static void __merge_sort(sort_t * a, size_t r, sort_t * s)
{
	int q;

	if(r <= 0)
		return;

	q = r/2;	
	__merge_sort(a, q, s);
	__merge_sort(a+q+1, r-q-1, s);
	merge(a, q, r, s);
}


struct merge_sort_args {
	sort_t * a, * s; 
	size_t r;
};

static void * __run_merge_sort(void * __args)
{
	struct merge_sort_args * args = __args;	
	__merge_sort(args->a, args->r, args->s);
	free(args);
	pthread_exit(NULL);
}

static void start_merge_sort(pthread_t * t, sort_t * a, size_t r, sort_t * s)
{
	struct merge_sort_args * args = malloc(sizeof(struct merge_sort_args));
	args->a = a;
	args->r = r;
	args->s = s;
	if(pthread_create(t, NULL, __run_merge_sort, args) != 0) {
		perror("pthread_create");
		exit(1);
	}
}

static inline void * getbuf(sort_t * a, size_t size, size_t order)
{
	return malloc(sizeof(sort_t) * (size + 2 + (order * 2)));
}

static void __merge_sort_n(sort_t * a, size_t size, size_t n, sort_t * s)
{
	pthread_t t[n];
	size_t i, coff = 0, p = 0, fs = (size - 1) / n, q = fs, splits[n];

	for(i = 0; i < n; i++) {
		if(i == (n-1)) {
			q = size - 1;
		}
		splits[i] = q;
		start_merge_sort(&t[i], a+p, q-p, s + p + coff);
		p = q + 1;
		q = p + fs;
		coff += 2;
	}

	for(i = 0; i < n; i++) {
		pthread_join(t[i], NULL);
	}

	for(i = 0; i < n - 1; i++) {
		merge(a, splits[i],  splits[i+1], s);	
	}

	return;
}

static void merge_sort_n(sort_t * a, size_t size, size_t order)
{
	sort_t * s = getbuf(a, size, order);
	__merge_sort_n(a, size, order, s);
	free(s);
}

static void merge_sort(sort_t * a, size_t size)
{
	int * s = getbuf(a, size, 0);
	__merge_sort(a, size-1, s);
	free(s);
}

static void fill_random(sort_t * array, size_t size)
{
	size_t i;
	for(i = 0; i < size; i++) {
		array[i] = rand();
		while(array[i] == INT_MAX)
			array[i] = rand();
	}
}

static int validate(sort_t * array, size_t size)
{
	size_t i, offenders = 0;

	// printf("%s: checking if array is sorted...\n", __func__);

	for(i = 1; i < size; i++) {
		if(array[i] < array[i - 1]) {
			offenders++;
		}
	}

	if(offenders) {	
		printf("%s: array is not sorted. offenders=%llu\n", 
			__func__, offenders);
		return -1;
	}

	// printf("%s: looks ok\n", __func__);

	return 0;
}

static void measure_sort_n(sort_t * array, size_t size, size_t n)
{
	struct timespec start, end;
	float elapsed;
	fill_random(array, size);
	
	clock_gettime(CLOCK_MONOTONIC, &start);
	
	if(n == 1)
		merge_sort(array, size);
	else
		merge_sort_n(array, size, n);

	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed = end.tv_sec - start.tv_sec;
	elapsed += (end.tv_nsec - start.tv_nsec) / 1e9;

	printf("cpus=%d time=%.3fs.\n", n, elapsed);
	
	validate(array, size);
}

#define ARR_SIZE (1e7)

int main(int argc, char ** argv)
{
	char c;
	int cpus[20], i, num_cpus = 0;

	while((c = getopt(argc, argv, "c:")) != -1) {
		switch(c) {
		case 'c':
			if(num_cpus >= sizeof(cpus)/sizeof(*cpus)) {
				fprintf(stderr, "Too many cpus\n");
				return EXIT_FAILURE;
			}
			cpus[num_cpus++] = atoi(optarg);
			if(cpus[num_cpus-1] <= 0) {
				fprintf(stderr, "%d is not valid number of cpus\n", 
						cpus[num_cpus]-1);
				return EXIT_FAILURE;
			}
			break;
		default:
			fprintf(stderr, "Usage: a.out -c 1 -c 2 -c 5 ...\n"
						"-c is amount of cpus on which to run benchmark\n");
			return EXIT_FAILURE;
		}
	}

	if(num_cpus == 0) {
		num_cpus = 1;
		cpus[0] = 1;
	}

	printf("initializing array with %.1e elements\n", ARR_SIZE);
	int * array = malloc(ARR_SIZE * sizeof(int));

	for(i = 0; i < num_cpus; i++) {
		measure_sort_n(array, ARR_SIZE, cpus[i]);
	}

	return 0;
}

