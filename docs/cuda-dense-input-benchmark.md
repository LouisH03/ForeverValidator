# CUDA dense-input benchmark matrix

`run_cuda_dense_input_matrix.py` exercises every CUDA search modifier at
approximately 10 and 100 synthetic input events per second. It also covers
early and late branch/mutable-suffix boundaries, ordered mixed modifier passes,
overlapping smooth steering, held multi-channel insertion and restoration,
multi-channel deletion, finish refinement, winner capture, capacity growth,
cancellation, and candidate-ID rollover.

The benchmark executable preserves its original positional interface. Dense
workloads add two optional arguments after an explicit evaluator. The boundary
offset delays modifier eligibility within the suffix; the matrix uses distinct
branch times for the actual early/late suffix boundary:

```sh
build/cuda-dense/cuda_search_benchmark PACKS REPLAY \
  4096 100 7 5000 mixed optimized velocity \
  --input-rate 100 --boundary-offset-ticks 75
```

The matrix runner can execute a preserved base binary and the integration
binary, reject any exact-result difference, run the optimized-versus-legacy
differential for every workload, and emit raw JSONL, summarized JSONL, and a
Markdown before/after table:

```sh
tools/run_cuda_dense_input_matrix.py PACKS REPLAY \
  --before-benchmark /tmp/forevervalidator-dense-before/cuda_search_benchmark \
  --after-benchmark build/cuda-dense/cuda_search_benchmark \
  --output-dir docs/cuda-dense-input-results
```

Without `--candidates`, the runner calibrates the batch size on the dense
late-boundary mixed workload. The first repetition is treated as warmup.
`mutation_buffer_working_set_gbps` is only the touched-buffer-size/time proxy;
it is not a hardware-counter claim. Use Nsight Compute for achieved DRAM
throughput when the profiler is installed.

Replay parity remains a separate gate:

```sh
build/cuda-dense/cuda_replay_parity PACKS REPLAY both
ctest --test-dir build/cuda-dense --output-on-failure
```
