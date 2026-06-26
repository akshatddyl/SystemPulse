# Stage 1: Heavy Builder Image
FROM gcc:12 AS builder
WORKDIR /app
RUN apt-get update && apt-get install -y cmake
COPY CMakeLists.txt ./
COPY main.cpp ./
COPY concurrent_queue.hpp ./
RUN mkdir build && cd build && cmake .. && make -j$(nproc)

# Stage 2: Lightweight Runtime Image
FROM debian:bookworm-slim
WORKDIR /app
RUN apt-get update && apt-get install -y libbrotli1 libssl3 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/broker .
EXPOSE 8080
CMD ["./broker"]
