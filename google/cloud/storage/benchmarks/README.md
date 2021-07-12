# Google Cloud Storage C++ Client Library Benchmarks

This directory contains benchmarks for the GCS C++ client library. The objective
is not to benchmark GCS, but to verify that (a) the library *can* reach the
expected performance for a single GCS client, and (b) that the library does not
consume excessive CPU or memory resources to do so.

In general, a single threaded process, running on a n1-standard-8 GCE instance
should be able to download data at around 400 MiB/s (with MD5 hashes disabled),
and upload data at about 50 MiB/s.  The time to first byte (TTFB) is high, so
these throughput numbers only make sense for relatively large objects (say
over 500 MiB).

## Compiling for Benchmarks

Make sure to compile all the dependencies with the right optimization level.
The [contributor documentation](/doc/contributor/README.md) has the relevant
information.

## Running the Benchmarks

### Evaluating Latency

Start by running the `throughput_vs_cpu` benchmark using these parameters:

```console
${BINARY_DIR}/google/cloud/storage/benchmarks/storage_throughput_vs_cpu_benchmark \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --region=us-central1 \
    --thread-count=1 \
    --minimum-object-size=1KiB \
    --maximum-object-size=1MiB \
    --minimum-sample-count=100 \
    --duration=5s |
  tee tp-vs-cpu.ttfb.txt
```

Then tweak the parameters to obtain enough samples, or cover a wide range of
scenarios.

```console
${BINARY_DIR}/google/cloud/storage/benchmarks/storage_throughput_vs_cpu_benchmark \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --region=us-central1 \
    --thread-count=16 \
    --minimum-object-size=1KiB \
    --maximum-object-size=1MiB \
    --minimum-sample-count=1000 \
    --duration=5s |
  tee tp-vs-cpu.ttfb.txt
```

Generate plots for this data using (see below on how to install the Python
dependencies):

```console
python google/cloud/storage/benchmarks/storage_throughput_vs_cpu_plots.py \
    --input-file ~/tp-vs-cpu.ttfb.txt  --output-prefix ttfb
```

### Evaluating Throughput

Start by running the `throughput_vs_cpu` benchmark using these parameters:

```console
${BINARY_DIR}/google/cloud/storage/benchmarks/storage_throughput_vs_cpu_benchmark \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --region=us-central1 \
    --thread-count=1 \
    --minimum-object-size=16MiB \
    --maximum-object-size=256MiB \
    --minimum-sample-count=100 \
    --duration=5s |
  tee tp-vs-cpu.tp.txt
```

Then adjust as needed, for example:

```console
${BINARY_DIR}/google/cloud/storage/benchmarks/storage_throughput_vs_cpu_benchmark \
    --project-id=${GOOGLE_CLOUD_PROJECT} \
    --region=us-central1 \
    --thread-count=16 \
    --minimum-object-size=16MiB \
    --maximum-object-size=2512MiB \
    --minimum-sample-count=1000 \
    --duration=5s |
  tee tp-vs-cpu.tp.txt
```

```console
python google/cloud/storage/benchmarks/storage_throughput_vs_cpu_plots.py \
    --input-file ~/tp-vs-cpu.tp.txt  --output-prefix tp
```

### Avoid MD5 Hashes

The client library runs MD5 hashes by default, these can be computational
expensive, to the point that the client cannot achieve over 300 MiB/s of
download speed. Consider excluding results with MD5 enabled from your analysis.

## Appendix: Installing Python Dependencies

There are probably multiple ways to install the Python dependencies used to
generate the benchmark result plots. If you prefer a different procedure then
by all means use it.

The Python scripts use Python3 and assume that the `venv` module is
installed on your workstation.

We will use a Python [virtual environment][python-venv] to install the
dependencies, this is a self-contained directory tree, in your `$HOME` so we can
install the dependencies without affecting set setup on your workstation or any
other virtual environments you might have.

We will use `vebm` (for Virtual Environment for BenchMarks) in this document,
you can use a different name in your case. First create the virtual environment:

```console
python3 -m venv ~/vebm
```

Then activate this environment

```console
source ~/vebm/bin/activate
```

Now install the dependencies using `pip`:

```console
pip install plotnine pandas
```

You can now run the Python scripts as described above.

[python-venv]: https://docs.python.org/3/tutorial/venv.html
