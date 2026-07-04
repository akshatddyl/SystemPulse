<div align="center">

# 💓 SystemPulse

### High-Throughput Telemetry Streaming & Real-Time Anomaly Detection Pipeline

*A distributed, lock-free, self-healing observability system — built in modern C++ and Python.*

[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white)](#)
[![Python](https://img.shields.io/badge/Python-3.10%2B-3776AB?style=for-the-badge&logo=python&logoColor=white)](#)
[![Docker](https://img.shields.io/badge/Docker-Compose-2496ED?style=for-the-badge&logo=docker&logoColor=white)](#)
[![FastAPI](https://img.shields.io/badge/FastAPI-ML%20Service-009688?style=for-the-badge&logo=fastapi&logoColor=white)](#)
[![Prometheus](https://img.shields.io/badge/Prometheus-Metrics-E6522C?style=for-the-badge&logo=prometheus&logoColor=white)](#)
[![Grafana](https://img.shields.io/badge/Grafana-Dashboards-F46800?style=for-the-badge&logo=grafana&logoColor=white)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](#)

</div>

## 📑 Table of Contents

- [Overview](#overview)
- [Design Goals](#Design-Goals)
- [Key Features](#key-features)
- [System Architecture](#system-architecture)
- [End-to-End Data Flow](#end-to-end-data-flow)
- [Component Deep Dive](#component-deep-dive)
- [Engineering Challenges & Solutions](#engineering-challenges--solutions)
- [Performance Benchmarks](#performance-benchmarks)
- [Chaos Engineering & Fault Injection](#chaos-engineering--fault-injection)
- [Demo & Dashboards](#Demo)
- [Tech Stack](#tech-stack)
- [Getting Started](#getting-started)
- [Project Structure](#project-structure)
- [Testing](#testing)
- [Contributing](#contributing)
- [Author](#author)

---
## 📖 Overview

**SystemPulse** is a distributed telemetry pipeline for collecting infrastructure metrics and detecting anomalies in real time.

The system consists of lightweight C++ agents that continuously collect container-scoped CPU and memory metrics, a high-throughput C++ broker that aggregates telemetry from a fleet of machines, and a Python-based anomaly detection service that performs rolling statistical analysis on each host independently. Every component exports Prometheus metrics, enabling complete end-to-end observability through Grafana.

Unlike traditional monitoring stacks that rely on general-purpose collectors, SystemPulse implements its own ingestion pipeline from the ground up—including a custom lock-free ring buffer, a condition-variable-driven concurrent queue, bounded backpressure handling, and an online rolling Z-score engine. The objective is to explore how a modern telemetry system can remain efficient, fault-tolerant, and horizontally scalable while maintaining minimal runtime overhead.

The project demonstrates concepts commonly found in production distributed systems:

- High-throughput telemetry ingestion
- Lock-free concurrent programming
- Producer–consumer architectures
- Backpressure and load shedding
- Online statistical anomaly detection
- Horizontal scaling with container orchestration
- End-to-end observability using Prometheus and Grafana
---
## 🎯 Design Goals

SystemPulse was built to investigate several engineering problems commonly encountered in distributed observability systems:

- **Low-overhead telemetry collection** using lightweight native C++ agents
- **Efficient concurrent ingestion** without busy-waiting or excessive synchronization
- **Resilience under downstream congestion** through bounded queues and backpressure
- **Online anomaly detection** without requiring historical datasets or offline training
- **Horizontal scalability** where hundreds of agents can be added with minimal operational effort
- **Full observability** of the monitoring pipeline itself using Prometheus and Grafana

---

## ✨ Key Features

- 🔩 **Custom Lock-Free Ring Buffer** — atomic producer-consumer communication inside each agent for near-zero CPU overhead.
- 🌐 **Horizontally Scalable by Design** — agents self-assign identity via `gethostname()`; scale from 1 to 100+ replicas with one command.
- 🧵 **Zero Busy-Waiting** — the broker's queue blocks on a `condition_variable`, idling at 0.0% CPU instead of spinning.
- 🛡️ **Backpressure-Aware** — a strict drop-oldest policy at 10,000 items protects the broker from OOM crashes during downstream congestion.
- 📈 **Online Anomaly Detection** — per-host rolling Z-score scoring with warm-up periods, epsilon safeguards, and absolute-threshold fallbacks.
- 🐳 **Container-Native Metrics** — reads `cgroups`, not `/proc`, so every container reports genuinely isolated CPU/memory usage.
- 🔭 **Full Observability Loop** — native Prometheus `/metrics` endpoints in both C++ and Python, visualized on a Grafana dashboard.
- 🔥 **Battle-Tested with Chaos Engineering** — `stress-ng`-driven fault injection validates the anomaly detector under real load.

---

## 🏗️ System Architecture

SystemPulse is composed of four cooperating layers, each independently scalable and independently observable: a fleet of lightweight C++ agents, a central C++ broker, a Python ML scoring service, and an observability stack that watches the whole pipeline — including itself.

```mermaid
flowchart TB
    subgraph Fleet["🖥️ Server Fleet (Horizontally Scaled)"]
        A1["Agent #1<br/>(C++)"]
        A2["Agent #2<br/>(C++)"]
        AN["Agent #N<br/>(C++)"]
    end

    subgraph Broker["⚙️ Central Broker (C++)"]
        TP["HTTP Thread Pool<br/>cpp-httplib"]
        CQ[("ConcurrentQueue<br/>mutex + condition_variable")]
        TP --> CQ
    end

    subgraph MLProc["🧠 ML Processor (Python / FastAPI)"]
        DEQ[("Per-Agent Deque<br/>Rolling Window")]
        ZS["Z-Score Engine<br/>warm-up + epsilon guard"]
        DEQ --> ZS
    end

    subgraph Obs["📊 Observability Stack"]
        PROM["Prometheus"]
        GRAF["Grafana"]
        PROM --> GRAF
    end

    A1 -->|JSON batch POST| TP
    A2 -->|JSON batch POST| TP
    AN -->|JSON batch POST| TP
    CQ -->|stream batch| DEQ
    TP -.->|expose /metrics| PROM
    ZS -.->|expose /metrics| PROM
    ZS ==>|🚨 anomaly event| GRAF

    classDef cpp fill:#00599C,stroke:#003b66,color:#ffffff
    classDef python fill:#3776AB,stroke:#254a6e,color:#ffffff
    classDef obs fill:#E6522C,stroke:#a83c1f,color:#ffffff
    classDef store fill:#2d333b,stroke:#8b949e,color:#ffffff

    class A1,A2,AN,TP cpp
    class DEQ,ZS python
    class PROM,GRAF obs
    class CQ store
```

- **Agents → Broker**: agents batch samples and ship them over HTTP as JSON; the broker never blocks a producer, it only accepts and enqueues.
- **Broker → ML Processor**: the broker's internal `ConcurrentQueue` is drained by the ML service, which maintains a bounded rolling window per `agent_id`.
- **Every layer → Observability**: both the C++ services and the Python service expose native `/metrics` endpoints, scraped by Prometheus and visualized in Grafana — the pipeline monitors itself with the same rigor it applies to the fleet.

---

## 🔄 End-to-End Data Flow

The sequence below traces a single telemetry sample from the moment it's read off a container's `cgroup` file to the moment it either lands safely in a dashboard or trips an anomaly alert.

```mermaid
sequenceDiagram
    autonumber
    participant Agent as Telemetry Agent
    participant Ring as Ring Buffer
    participant Broker as Central Broker
    participant Queue as ConcurrentQueue
    participant ML as ML Processor
    participant Graf as Grafana

    Agent->>Agent: Read /sys/fs/cgroup (CPU & Memory)
    Agent->>Ring: Producer pushes sample (lock-free, atomic)
    Ring->>Agent: Consumer pops batch
    Agent->>Broker: POST /ingest JSON batch
    Broker->>Queue: enqueue(batch)
    Note over Queue: cond_var wakes worker<br/>0% CPU while idle
    alt queue size > 10,000
        Broker->>Broker: drop-oldest (backpressure)
    end
    Queue->>ML: dequeue & stream batch
    ML->>ML: append to deque[agent_id]
    alt len(deque) < 30
        ML->>ML: warm-up: store only, no scoring
    else len(deque) >= 30
        ML->>ML: compute rolling mean, std-dev, Z-score
        alt abs(Z) > threshold
            ML->>Graf: emit anomaly event
        end
    end
    ML->>Graf: expose /metrics for Prometheus scrape
```

---

## 🧠 Component Deep Dive

### 1. Telemetry Agent — C++

Each agent is a small, self-contained binary designed to be invisible to the workload it's monitoring. Internally it runs a classic **producer-consumer** pattern across two threads:

- A **sampling thread** (producer) polls the container's `cgroup` files for CPU and memory usage at a fixed interval.
- A **network thread** (consumer) drains the samples and ships them to the broker as batched JSON over HTTP.

The two threads never touch a mutex to hand off data. Instead they communicate through a **custom lock-free atomic ring buffer**, so the producer can keep sampling even while the network thread is busy flushing a batch, and vice versa — no blocking, no lock contention, no busy-waiting. On startup, each agent calls POSIX `gethostname()` to derive its own unique `agent_id`, which is what makes horizontal scaling trivial: there's no central registry to coordinate, because Docker already guarantees every scaled replica a unique hostname.

### 2. Central Broker — C++

The broker is the fan-in point for the entire fleet. It exposes an HTTP ingestion endpoint (via `cpp-httplib`) backed by a thread pool, so N agents can POST batches concurrently without serializing on a single accept loop. Once a batch is accepted, it's handed to a custom `ConcurrentQueue` — a `std::mutex` + `std::condition_variable`-backed structure that eliminates busy-waiting entirely (see [Engineering Challenges](#engineering-challenges--solutions)).

Because the broker sits between a fleet that can burst unpredictably and a downstream ML service that processes at its own pace, it also enforces a strict **backpressure policy**: once the queue hits 10,000 items, it starts dropping the *oldest* entries rather than growing without bound. In distributed-systems terms, this is a deliberate load-shedding decision — under sustained congestion, the broker chooses to stay alive and keep serving recent data over faithfully queuing everything and risking an OOM crash that takes the whole pipeline down.

### 3. ML Processor — Python / FastAPI

The ML processor is where raw numbers become a judgment call: *is this normal, or not?* For every `agent_id`, it maintains an isolated `collections.deque` — a fixed-size rolling window of recent samples — and computes a live **Z-score** for each new value against that window's mean and standard deviation.

Three guardrails keep this from being a naive, false-positive-prone implementation:

1. A **30-sample warm-up period** before any score is computed at all (the Cold Start fix).
2. An **epsilon safeguard** that catches near-zero standard deviation — a metric that's been essentially flat — before it can blow up a division.
3. An **absolute-threshold fallback** for exactly that flat-metric case, so a genuinely large jump in an otherwise-quiet metric still gets caught even when the Z-score math alone isn't reliable.

```mermaid
flowchart TD
    Start(["New sample arrives<br/>for agent_id"]) --> Check{"len(deque) >= 30?"}
    Check -- "No" --> Warmup(["Warm-up:<br/>store sample only"])
    Check -- "Yes" --> Stats["Compute rolling mean & std-dev"]
    Stats --> EpsCheck{"std-dev < epsilon?"}
    EpsCheck -- "Yes" --> AbsCheck{"value > absolute threshold?"}
    EpsCheck -- "No" --> ZScore["Z = (value - mean) / std-dev"]
    ZScore --> ZCheck{"abs(Z) > z-threshold?"}
    ZCheck -- "Yes" --> Anomaly(["🚨 Flag anomaly"])
    ZCheck -- "No" --> Nominal(["✅ Nominal"])
    AbsCheck -- "Yes" --> Anomaly
    AbsCheck -- "No" --> Nominal

    classDef decision fill:#bc8cff,stroke:#6e40c9,color:#0d1117
    classDef anomaly fill:#f85149,stroke:#8e1519,color:#ffffff
    classDef nominal fill:#3fb950,stroke:#1a7f37,color:#0d1117
    class Check,EpsCheck,ZCheck,AbsCheck decision
    class Anomaly anomaly
    class Nominal nominal
```

### 4. Observability Stack

Both the C++ services and the Python service expose their own `/metrics` endpoint in Prometheus exposition format — the pipeline is instrumented with the same seriousness as the systems it watches. The Grafana dashboard built on top tracks, at minimum:

- **Ingestion Rate** — batches/sec arriving at the broker
- **Queue Saturation %** — how close the `ConcurrentQueue` is to its 10,000-item backpressure ceiling
- **Dropped Packets** — a direct visualization of load-shedding in action
- **Fleet-wide Anomalies** — every flagged `agent_id`, plotted over time

---

## 🛠️ Engineering Challenges & Solutions

| # | Challenge | One-Line Fix |
|---|---|---|
| 1 | The Shared Kernel Illusion | Read `cgroups`, not `/proc/stat`, for true per-container isolation |
| 2 | The Cold Start Bug | 30-sample warm-up before any Z-score is computed |
| 3 | Busy-Waiting | `condition_variable::wait()` — 0.0% CPU at idle |
| 4 | Auto-Scaling Orchestration | Self-assigned `agent_id` via `gethostname()` + `--scale agent=N` |

### 1. The Shared Kernel Illusion

**The problem:** Inside a container, reading `/proc/stat` doesn't give you what you'd expect. Depending on the runtime and kernel configuration, that file can reflect the *host's* aggregate CPU state rather than a view scoped to the individual container — so ten containers on the same host could all report identical, host-wide numbers instead of their own actual usage. For a project whose entire premise is per-host anomaly detection, that's not a minor bug, it's a silent correctness failure.

**The fix:** The agent reads directly from the container's `cgroup` pseudo-filesystem (`/sys/fs/cgroup/`), which the container runtime *does* scope per-container. This was verified empirically: with 100 containers running simultaneously, each one reports independent, isolated CPU and memory figures.

### 2. The Cold Start Bug

**The problem:** A newly spawned agent has no history. Computing a standard deviation — and therefore a Z-score — from one or two samples isn't just statistically weak, it's actively dangerous: a single early reading can produce an enormous, meaningless Z-score and fire a false anomaly before the system has any real baseline to compare against.

**The fix:** Every `agent_id` gets a mandatory 30-sample warm-up window. Samples are stored in the rolling deque during this period but are never scored — the anomaly detector stays silent until it actually has a statistically meaningful baseline to judge against.

### 3. Busy-Waiting

**The problem:** The most naive way to build a producer-consumer queue is to have the consumer poll in a loop — "is there work yet? is there work yet?" That pattern pins a CPU core near 100% even when the entire pipeline is sitting completely idle, which is a poor look for a system explicitly designed to prove low overhead.

**The fix:** The broker's `ConcurrentQueue` puts consumer threads to sleep on a `std::condition_variable` and only wakes them when a producer calls `notify()`. Measured idle CPU usage: 0.0%.

### 4. Auto-Scaling Orchestration

**The problem:** Simulating a real fleet by hand-declaring 100 separate `agent` blocks in `docker-compose.yml` — or worse, spinning up 100 containers manually — doesn't scale as an engineering practice, and it doesn't reflect how real infrastructure is actually managed.

**The fix:** A single parameterized `agent` service definition, combined with each container self-registering its own `agent_id` via POSIX `gethostname()` (Docker assigns every scaled replica a unique hostname automatically), means the entire fleet can grow or shrink with one command — `docker compose up --scale agent=100` — with zero code or config changes.

---

## 📊 Performance Benchmarks

Measured during a local stress test with 100 concurrently Dockerized agents(servers):

| Metric | Result |
|---|---|
| Concurrent Agents | 100 Dockerized agents |
| Sustained Throughput | ~6,000 metrics/min (99.8 batches/sec) |
| Per-Agent CPU Footprint | ~0.7% CPU |
| Per-Agent Memory Footprint | ~1.7 MB RAM |
| Backpressure Resilience | 7,231 packets safely dropped during a simulated network outage — broker memory stayed capped, zero OOM crashes |
| Broker Idle CPU | 0.0% (condition-variable blocking, zero busy-wait) |

---

## 🔥 Chaos Engineering & Fault Injection

Correctness claims about an anomaly detector are only as good as the chaos you throw at it. SystemPulse ships with a `trigger_anomaly.sh` script that dynamically targets a specific agent replica inside the scaled fleet and injects real CPU load using `stress-ng`:

```bash
# Target agent replica #14 out of a 100-agent fleet and spike its CPU
docker compose exec --index=14 agent stress-ng --cpu 4
```

This isn't a synthetic unit test with mocked inputs — it's a real container, under real load, inside the actual running pipeline, and the ML Processor has to catch the resulting spike using nothing but the live metrics stream.

---

## 📸 Demo

**Grafana — Dashboard Overview**
---
<img width="800" height="450" alt="demo1" src="https://github.com/user-attachments/assets/4b099c1a-4beb-4aee-83e7-0a555d990db8" />
---

**Anomaly Detection in Action**
---
<img width="800" height="435" alt="demo2" src="https://github.com/user-attachments/assets/022e2e33-ebd4-4907-a6a3-ae23466e05b6" />
---

**Chaos Test — Backpressure Under Load**
---
<img width="800" height="435" alt="demo3" src="https://github.com/user-attachments/assets/0eef33f7-e2f3-437f-8a82-80d5d000bbf7" />
---


---

## 🧰 Tech Stack

| Layer | Technology | Purpose |
|---|---|---|
| Agent & Broker | C++20 | Low-level, high-performance metric collection & ingestion |
| Networking | `cpp-httplib` | Lightweight embedded HTTP server/client for the C++ services |
| Concurrency | `std::mutex`, `std::condition_variable`, atomics | Thread-safe concurrency without busy-waiting |
| Testing | GoogleTest (`gtest`) | C++ unit testing |
| ML Service | Python 3 + FastAPI | Async anomaly-detection microservice |
| ML / Stats | NumPy | Rolling statistics & anomaly scoring |
| Containerization | Docker, Docker Compose | Isolation, orchestration, horizontal scaling |
| Metrics | Prometheus | Time-series metrics collection & exposition |
| Visualization | Grafana | Real-time SRE dashboards |
| OS Internals | Linux `cgroups`, POSIX `gethostname()` | Container-scoped resource introspection & self-identification |
| Chaos Testing | `stress-ng` | Synthetic load & fault injection |

---

## 🚀 Getting Started

### Prerequisites
- Docker & Docker Compose (v2+)
- *(Optional, for local dev outside containers)* CMake, a C++17/20-compliant compiler, Python 3.10+

### Clone & Run
```bash
git clone https://github.com/akshatddyl/SystemPulse.git
cd SystemPulse
docker compose up --build
```

### Scale the Fleet
```bash
docker compose up --scale agent=100 -d
```

### Access the Dashboards
| Service | Default URL |
|---|---|
| Grafana | http://localhost:3000 (username & password: `admin`) |
| Prometheus | http://localhost:9090 |
| ML Processor (FastAPI docs) | http://localhost:8000/docs |

### Trigger a Chaos Test
```bash
docker compose exec --index=14 agent stress-ng --cpu 4
```

---

## 📂 Project Structure

```
├── agent/
│   ├── CMakeLists.txt
│   ├── Dockerfile
│   ├── main.cpp
│   ├── metrics_collector.hpp
│   └── ring_buffer.hpp
├── broker/
│   ├── CMakeLists.txt
│   ├── concurrent_queue.hpp
│   ├── Dockerfile
│   └── main.cpp
├── grafana/
│   ├── dashboards/
│   │   └── telemetry.json
│   └── provisioning/
│       ├── dashboards/
│       │   └── dashboards.yml
│       └── datasources/
│           └── datasource.yml
├── ml-processor/
│   ├── app.py
│   ├── Dockerfile
│   └── requirements.txt
├── prometheus/
│   └── prometheus.yml
├── .gitignore
├── docker-compose.yml
├── start.md
└── trigger_anomaly.sh

```

---
## 🧪 Testing

SystemPulse utilizes **Google Test (gtest)** to validate C++ unit logic, specifically targeting the custom lock-free concurrency structures (Ring Buffer) to mathematically prove thread safety and boundary resilience under stress.

Tests are executed using CMake's standard `ctest` utility via a clean, out-of-source build.

```bash
# 1. Navigate to the target directory
cd agent

# 2. Generate the out-of-source build files
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 3. Compile the test executable
cmake --build build

# 4. Run the test suite
ctest --test-dir build --output-on-failure
```
---

## 🤝 Contributing

This started as a solo portfolio build, but issues, ideas, and pull requests are welcome. Feel free to open an issue to discuss a change before submitting a PR.

---

## 👤 Author

Built and maintained by **[@akshatddyl](https://github.com/akshatddyl)**.

Feel free to reach out via GitHub with questions, feedback, or future implementations.

---

<div align="center">

**If SystemPulse's approach to distributed observability interests you, consider ⭐ starring the repo.**

</div>
