# Cold-Cache MPI (ccMPI)
Traditional MPI microbenchmark suites measure performance by repeatedly exe-
cuting the same operation within nested loops. Although this approach increases
statistical robustness, it also introduces an artificial scenario. In this setting, the
majority of data accesses originate from the cache. This includes accesses to the
message buffer, MPI internal data structures, and the instruction stream. As a
result, the measured performance reflects the impact of this artificial scenario,
thus creating unrealistic expectations for the MPI performance in real-world ap-
plications. MPI microbenchmark suites such as OSU Micro-Benchmarks (OMB)
[22] and Intel MPI Benchmarks (IMB) [14] follow the traditional methodology
described above. IMB additionally provides options to control message-buffer
locality for point-to-point measurements. Latency is typically measured using a
ping-pong communication pattern, which estimates the cost of a send–receive
pair from the round-trip time of a message, assuming half of this time corre-
sponds to a one-way transfer. Because we expect above caching effects to differ  
significantly between send and receive operations, the model is unsuitable for our
study. Hence, we develop a benchmark suite that executes either MPI_Send or
MPI_Receive only once per measurement. For each call, we record the number of
CPU cycles. This allows us to observe changes in latency on both the send and
receive side. Cycle counts and hardware performance counters are obtained via
the Performance Application Programming Interface (PAPI) [15]. This method-
ology enables an independent analysis of cold- and hot-cache effects on MPI
point-to-point communication.

## Dependencies
* PAPI
* MPI

## Build
```bash
$ mkdir build && cd build
$ cmake ..
$ make -j
```

## Available Benchmarks
### Blocking / Non-blocking Send / Recv
```bash
$ ./send_recv.bin -h
MPI Cache Flush Benchmark
Usage: ./send_recv.bin [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -i,--iterations UINT        Number of timed iterations
  -w,--warmup UINT            Number of warmup iterations
  -e,--min-exp UINT           Minimum exponent used to generate message sizes.
  -x,--max-exp UINT           Maximum exponent used to generate message sizes.
  -p,--papi-event-file TEXT   File with PAPI event names to record
  -s,--send-result-file TEXT  File in which send results will appear
  -a,--recv-result-file TEXT  File in which recv results will appear
  -o,--only-bench TEXT        Execute only one benchmark defined by the given string.
  -r,--raw-data Excludes: --median
                              Print per iteration values.
  -m,--median Excludes: --raw-data
                              Report median values.
```
### Prefetch Send / Recv
* Can be used to examine speedup for prefetched bounce buffers
* Relies on [ucx_mpool_tracer](https://github.com/NeuwirthLab/ucx_mpool_tracer)
