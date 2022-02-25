#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <time.h>
#include <string.h>
#include <pthread.h>

typedef int sort_t;

static void merge(sort_t * A, size_t p, size_t q, size_t r, sort_t * s)
{
	register size_t i, j;
	size_t k, n1, n2;

	n1 = q - p + 1;
	n2 = r - q;

	sort_t * L = s;
	sort_t * R = s + n1 + 1; /* +1 for INT_MAX terminator */

	memcpy(L, A+p, sizeof(int) * n1);
	memcpy(R, A+q+1, sizeof(int) * n2);

	L[n1] = INT_MAX;
	R[n2] = INT_MAX;

	i = 0;
	j = 0;

	for(k = p; k <= r; k++) {
		if(L[i] <= R[j]) {
			A[k] = L[i++];
		} else {
			A[k] = R[j++];
		}
	}
}

static void __merge_sort(sort_t * a, size_t p, size_t r, sort_t * s)
{
	int q;

	if(p >= r)
		return;

	q = (p+r)/2;	
	__merge_sort(a, p, q, s);
	__merge_sort(a, q+1, r, s);
	merge(a, p, q, r, s);
}


struct merge_sort_args {
	sort_t * a, * s; 
	size_t p, r;	
};

static void * __run_merge_sort(void * __args)
{
	struct merge_sort_args * args = __args;	
	__merge_sort(args->a, args->p, args->r, args->s);
	free(args);
	pthread_exit(NULL);
}

static void start_merge_sort(pthread_t * t, sort_t * a, size_t p, size_t r, sort_t * s)
{
	struct merge_sort_args * args = malloc(sizeof(struct merge_sort_args));
	args->a = a;
	args->p = p;
	args->r = r;
	args->s = s;
	if(pthread_create(t, NULL, __run_merge_sort, args) != 0) {
		perror("pthread_create");
		exit(1);
	}
}

/*
static void __merge_sort_2(int * a, int p, int r, int * s)
{
	pthread_t t1, t2;
	int q = (p+r)/2;
	start_merge_sort(&t1, a, p, q, s);
	start_merge_sort(&t2, a, q+1, r, s + q + 1 + 2);
	pthread_join(t1, NULL);
	pthread_join(t2, NULL);
	merge(a, p, q, r, s);
}
*/

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
		start_merge_sort(&t[i], a, p, q, s + p + coff);
		p = q + 1;
		q = p + fs;
		coff += 2;
	}

	for(i = 0; i < n; i++) {
		pthread_join(t[i], NULL);
	}

	for(i = 0; i < n - 1; i++) {
		merge(a, 0, splits[i],  splits[i+1], s);	
	}

	return;
}

/*
static void merge_sort_2(int * a, int size)
{
	int * s = getbuf(a, size, 1);
	__merge_sort_2(a, 0, size-1, s);
	free(s);
}
*/

static void merge_sort_n(sort_t * a, size_t size, size_t order)
{
	sort_t * s = getbuf(a, size, order);
	__merge_sort_n(a, size, order, s);
	free(s);
}

static void merge_sort(sort_t * a, size_t size)
{
	int * s = getbuf(a, size, 0);
	__merge_sort(a, 0, size-1, s);
	free(s);
}

#define ARR_SIZE (1e7)

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
	fill_random(array, ARR_SIZE);
	
	printf("sorting using %d cpus...\n", n);

	clock_gettime(CLOCK_MONOTONIC, &start);
	
	if(n == 1)
		merge_sort(array, ARR_SIZE);
	else
		merge_sort_n(array, ARR_SIZE, n);

	clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed = end.tv_sec - start.tv_sec;
	elapsed += (end.tv_nsec - start.tv_nsec) / 1e9;

	printf("sort took %.3fs.\n", elapsed);
	
	validate(array, ARR_SIZE);
}

int main(void) 	
{

	srand(time(NULL));
	char * lb = NULL;
	size_t lbl = 4;

	printf("gimme number of cpus: ");
	fflush(stdout);
	getline(&lb, &lbl, stdin);

	int pi = 0, i;
	char cb[10];
	int cpus[10];
	int numcpus = 0;
	for(i = 0; i < lbl; i++) {
		if(lb[i] == ',' || lb[i] == '\n') {
			if(i - pi > sizeof(cb) -1) {
				goto err;
			}
			memcpy(cb, lb+pi, i-pi);
			cb[i-pi] = 0;
			pi = i + 1;
			if(numcpus >= (sizeof(cpus)/sizeof(*cpus)))
				goto err;
			cpus[numcpus++] = atoi(cb);
		}
	}

	if(!numcpus)
		goto err;

	for(i = 0; i < numcpus; i++) {
		if(cpus[i] <= 0) {
			goto err;
		}
	}

	free(lb);

	printf("initializing array with %.1e elements\n", ARR_SIZE);
	int * array = malloc(ARR_SIZE * sizeof(int));

	for(i = 0; i < numcpus; i++) {
		measure_sort_n(array, ARR_SIZE, cpus[i]);
	}

	return 0;

err:
	printf("nope, not falling for it\n");
	return -1;
}

