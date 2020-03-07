# Storage Test Bench for Python 3

This is a Storage API JSON and XML API emulator.

## Install Dependencies

```bash
pip install -r requirements.txt
```

## Run Test Bench

```bash
python3 testbench.py --port 8080
```

For more information use: `python3 testbench.py -h`

## Force Failures

You can force the following failures by using the `x-goog-testbench-instructions` header.

### return-broken-stream

Set request headers with `x-goog-testbench-instructions: return-broken-stream`.
Emulator will fail after sending 1024*1024 bytes.

### return-corrupted-data

Set request headers with `x-goog-testbench-instructions: return-corrupted-data`.
Emulator will return corrupted data.

### stall-always

Set request headers with `x-goog-testbench-instructions: stall-always`.
Emulator will stall at the beginning.

### stall-at-256KiB

Set request headers with `x-goog-testbench-instructions: stall-at-256KiB`.
Emulator will stall at 256KiB bytes.

### return-503-after-256K

Set request headers with `x-goog-testbench-instructions: return-503-after-256K`.
Emulator will return a `HTTP 503` after sending 256KiB bytes.

### return-503-after-256K/retry-1

Set request headers with `x-goog-testbench-instructions: return-503-after-256K/retry-1`.
?

### return-503-after-256K/retry-2

Set request headers with `x-goog-testbench-instructions: return-503-after-256K/retry-2`.
?

