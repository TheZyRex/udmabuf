#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "u-dma-buf-ioctl.h"

#define TO_MiB (1.0 / (1024.0*1024.0))

const int BYTES_TO_TRANSFER = 16*1024*1024;
const int U_DMA_BUF_SIZE = 16*1024*1024;

void doBenchmark(size_t length, double* result)
{
  const size_t LOOPS = BYTES_TO_TRANSFER / length;

  int fd_sync = open("/dev/udmabuf0", O_RDWR );
  if (fd_sync < 0) {
    perror("Error opening '/dev/udmabuf0'\n");
    exit(-1);
  }

  void* buf = mmap(NULL, U_DMA_BUF_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_sync, 0);
  if (buf == MAP_FAILED) {
    perror("Error mapping dmabuf0\n");
    exit(-1);
  }

  void* destBuf = aligned_alloc(4*1024, U_DMA_BUF_SIZE);

  // for cache flushing
  unsigned long sync_offset    = 0;
  unsigned long sync_size      = length;
  unsigned int  sync_direction = 1; // 1-> DMA_TO_DEVICE ; 2-> DMA_FROM_DEVICE 
  unsigned long sync_for_cpu   = 1; // 1-> sync_for_cpu: best before reads (CPU perspective) ; 0-> sync_for_device: best before writes (CPU perspective)
  unsigned long long combined_value = 0;

  // flush 'configuration'
  combined_value = ((unsigned long long)(sync_offset & 0xFFFFFFFF) << 32) | ((sync_size & 0xFFFFFFF0) | (sync_direction << 2) | sync_for_cpu);

  size_t off = 0;
  struct timespec start, end;

  clock_gettime(CLOCK_MONOTONIC_RAW, &start);
  for (size_t i = 0; i < LOOPS; ++i, off+=length) {
    ioctl(fd_sync, UDMABUF_IOCTL_SYNC, &combined_value);
    memcpy(destBuf+off, buf+off, length);
  }
  clock_gettime(CLOCK_MONOTONIC_RAW, &end);

  *result = (end.tv_sec - start.tv_sec)*1.0e6 + (end.tv_nsec - start.tv_nsec)/1.0e3;

  free(destBuf);
  munmap(buf, U_DMA_BUF_SIZE);
  close(fd_sync);
}

int main()
{
  int reps = 1000;
  int testing_size = 0;
  FILE* fp = fopen("results.txt", "w");
  double* results = (double*)malloc(sizeof(double)*reps);
  
  for (int k = 0; k < 20; ++k)
  {
	  testing_size = 1 << k;
	  for (int i = 0; i < reps; ++i)
	  {
	    doBenchmark(testing_size, &results[i]);
	  }

	  fprintf(fp, "\n\n--- uDMAbuf Read Benchmark Blocksize: %dBytes - Transferd Each: %0.1fMiB  ---\n", testing_size, BYTES_TO_TRANSFER*TO_MiB);
	  for (int i = 0; i < reps; ++i)
	  {
	    double bandwidth = BYTES_TO_TRANSFER * TO_MiB / (results[i] / 1000.0 / 1000.0);
	    fprintf(fp, "Single Operation took: %0.5lf µs\t throughput: %06lf MiB/s\n", results[i]/(BYTES_TO_TRANSFER/testing_size), bandwidth);
	  }

	  memset(results, 0, reps);
  }

  free(results);
  fclose(fp);

  return 0;
}