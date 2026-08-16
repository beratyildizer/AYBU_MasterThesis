# Performance Analysis of IPC Mechanisms on Real-Time Linux

This repository contains the source code, test scripts, and experimental results developed as part of my master's thesis at **Ankara Yıldırım Beyazıt University (AYBU)**.

The study investigates the performance characteristics of Linux Inter-Process Communication (IPC) mechanisms on an ARM-based embedded platform under different system load conditions and kernel preemption models.

The experiments mainly compare the standard **PREEMPT** configuration with **PREEMPT_RT**, focusing on latency, worst-case behavior, and predictability.

## Thesis

**Title:**
*Performance Analysis of Inter-Process Communication Mechanisms on Real-Time Linux Kernel (PREEMPT_RT) for ARM-Based Embedded Devices*

The primary objective of this work is to investigate how different Linux kernel preemption models affect the latency characteristics and determinism of IPC mechanisms on an embedded Linux platform.

Particular attention is given to the effect of **PREEMPT_RT** on worst-case latency and latency variation rather than only average IPC performance.

## IPC Mechanisms

The following Linux IPC mechanisms are evaluated:

* Pipes
* POSIX Message Queues
* Shared Memory

The benchmarks are implemented using a **producer-consumer architecture**, where separate processes exchange messages and end-to-end communication latency is measured.

## Experimental Platform

The experiments were performed on a **Raspberry Pi 5** single-board computer.

### Hardware

* Raspberry Pi 5
* Broadcom BCM2712 SoC
* 64-bit ARM Cortex-A76 CPU
* 64 GB microSD storage
* Headless operation via SSH

### Software

* Raspberry Pi OS Lite (64-bit)
* Linux Kernel 6.12
* PREEMPT kernel configuration
* PREEMPT_RT kernel configuration
* GCC
* stress-ng

Linux 6.12 was selected because PREEMPT_RT is integrated into the mainline Linux kernel and can be selected as a kernel preemption model.

## Benchmark Methodology

Each IPC benchmark consists of producer and consumer processes.

For each transmitted message, the producer records a timestamp immediately before the message is sent. The consumer records another timestamp after receiving the message.

Communication latency is calculated as:

```text
Latency = Receive Timestamp - Send Timestamp
```

`CLOCK_MONOTONIC` is used as the timing source.

Each message contains a header together with its payload. The header includes information such as:

* Marker value
* Sequence number
* Send timestamp
* Payload size

A synchronization/handshake mechanism is used between the producer and consumer to ensure controlled message transmission and reliable per-message latency measurement.

## Real-Time Configuration

Several real-time programming practices are applied to reduce measurement noise and improve the reliability of the experiments.

These include:

* `SCHED_FIFO` real-time scheduling
* Static real-time priorities
* Memory locking to prevent page faults
* CPU affinity
* CPU isolation
* Warm-up phase before measurements
* `CLOCK_MONOTONIC` for timestamp generation

Producer and consumer processes are configured with high real-time priorities during the experiments.

## Test Parameters

The benchmarks are executed using different payload sizes:

```text
100 bytes
1000 bytes
5000 bytes
10000 bytes
```

Each configuration processes:

```text
1,000,000 messages
```

Multiple runs are performed for each test configuration in order to reduce the influence of individual measurements.

The benchmark programs follow the general command-line format:

```bash
./program_name <number_of_messages> <payload_size> <consumer_priority> <producer_priority>
```

For example:

```bash
./program_name 1000000 1000 98 98
```

## System Load

The IPC mechanisms are evaluated both without significant background load and under generated system load.

For I/O load experiments, `stress-ng` is used.

Example:

```bash
stress-ng --iomix 4 --timeout 180s --metrics-brief
```

This creates concurrent I/O activity while the IPC benchmark is running.

A typical experiment consists of:

1. Starting the background workload.
2. Waiting for the system to stabilize.
3. Running the IPC benchmark.
4. Recording the benchmark results.
5. Terminating the workload.
6. Waiting before starting the next experiment.

A stabilization period is used before benchmark execution to ensure that measurements are collected while the background workload is active.

## Performance Metrics

The following latency metrics are collected:

* Minimum latency
* Maximum latency / Worst-Case Execution Time (WCET)
* Average latency
* Median latency
* Standard deviation
* Jitter characteristics

All latency measurements are reported in nanoseconds unless otherwise specified.

## PREEMPT vs PREEMPT_RT

The experiments compare two Linux kernel configurations:

### PREEMPT

The standard preemptible Linux kernel provides relatively low average latency while maintaining the behavior of a general-purpose operating system.

### PREEMPT_RT

PREEMPT_RT increases kernel preemptibility and converts many interrupt handling paths into schedulable threads.

The experimental results show an important distinction between **average performance** and **real-time predictability**.

In many test configurations, PREEMPT provides slightly lower average communication latency. However, PREEMPT_RT significantly reduces large latency spikes and worst-case latency, particularly when the system is under load.

Therefore, the main advantage of PREEMPT_RT observed in this study is not necessarily lower average IPC latency, but rather improved **determinism and predictability**.

## Experimental Procedure

For a typical load experiment:

```text
Start stress-ng
      ↓
Wait for stabilization
      ↓
Start IPC producer/consumer benchmark
      ↓
Transmit 1,000,000 messages
      ↓
Collect latency measurements
      ↓
Stop stress-ng
      ↓
Wait before the next run
```

Each test is repeated multiple times for each combination of:

```text
Kernel Configuration
        ×
IPC Mechanism
        ×
Payload Size
        ×
System Load
```

This methodology enables the latency characteristics of PREEMPT and PREEMPT_RT to be compared under equivalent experimental conditions.

## Reproducing the Experiments

First, install the required tools:

```bash
sudo apt update
sudo apt install build-essential stress-ng
```

Compile the corresponding producer and consumer benchmark applications.

Then start the required consumer process followed by the producer process according to the benchmark being evaluated.

For load experiments, start `stress-ng` before running the benchmark:

```bash
stress-ng --iomix 4 --timeout 180s --metrics-brief
```

After the stabilization period, execute the corresponding IPC benchmark.

The exact compilation commands, CPU affinity configuration, kernel configuration, and execution parameters may differ depending on the benchmark and experiment.

## Results

The experimental data included in this repository can be used to compare:

* PREEMPT and PREEMPT_RT
* Different IPC mechanisms
* Different payload sizes
* Idle and loaded system conditions
* Average latency and worst-case latency
* Latency variation and determinism

A general observation from the experiments is that **PREEMPT tends to provide slightly better average latency**, whereas **PREEMPT_RT provides substantially better worst-case behavior and latency predictability**, especially under system load.

This distinction is particularly important for embedded and real-time applications, where bounded response time can be more important than minimizing average latency.

## Research Motivation

Linux is increasingly used in embedded and real-time systems because of its hardware support, open-source ecosystem, and extensive software infrastructure.

However, general-purpose Linux does not inherently guarantee deterministic response times.

PREEMPT_RT aims to improve Linux's suitability for real-time workloads by increasing kernel preemptibility and reducing sections in which high-priority real-time tasks can be delayed.

This project experimentally investigates how these changes affect user-space IPC communication on an ARM-based embedded system.

## Academic Use

This repository was created for academic and experimental purposes as part of a master's thesis.

If you use the benchmark methodology, source code, scripts, or experimental results in an academic study, please provide an appropriate reference to this repository and the related thesis/publication.

## Author

**Berat Yıldızer**
Ankara Yıldırım Beyazıt University
Master's Thesis Project

## License

This repository is intended primarily for academic and research purposes.

Please refer to the repository license, if provided, for detailed usage and redistribution conditions.
