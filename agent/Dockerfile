# Stage 1: Heavy Builder Image
FROM gcc:12 AS builder
WORKDIR /app
RUN apt-get update && apt-get install -y cmake
COPY CMakeLists.txt ./
COPY main.cpp ./
COPY metrics_collector.hpp ./
COPY ring_buffer.hpp ./
# Configure and build
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Stage 2: Lightweight Runtime Image
FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /app/build/agent .
RUN apt-get update && apt-get install -y stress-ng libbrotli1 libssl3 && rm -rf /var/lib/apt/lists/*

CMD ["./agent"]
