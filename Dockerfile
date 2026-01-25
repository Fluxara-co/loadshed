# Multi-stage build for Device DNP3 Service with Process Manager
FROM gcc:12 AS builder

# Install build dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        pkg-config \
        nlohmann-json3-dev \
        libssl-dev \
        wget \
        git \
    && rm -rf /var/lib/apt/lists/*

# Install cpp-httplib (header-only library)
RUN cd /tmp && \
    git clone https://github.com/yhirose/cpp-httplib.git && \
    cp cpp-httplib/httplib.h /usr/local/include/ && \
    rm -rf cpp-httplib

# Install spdlog (logging library)
RUN cd /tmp && \
    git clone https://github.com/gabime/spdlog.git && \
    cd spdlog && \
    mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf spdlog

# Install Paho MQTT C++ library
RUN cd /tmp && \
    git clone https://github.com/eclipse/paho.mqtt.c.git && \
    cd paho.mqtt.c && \
    mkdir build && cd build && \
    cmake -DPAHO_WITH_SSL=OFF -DPAHO_BUILD_STATIC=OFF .. && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf paho.mqtt.c

RUN cd /tmp && \
    git clone https://github.com/eclipse/paho.mqtt.cpp.git && \
    cd paho.mqtt.cpp && \
    mkdir build && cd build && \
    cmake -DPAHO_BUILD_STATIC=OFF -DPAHO_WITH_SSL=OFF .. && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && \
    rm -rf paho.mqtt.cpp

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Create core directory if it doesn't exist (for Process Manager sources)
RUN mkdir -p /app/src/core

# Build the application
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc) VERBOSE=1

# ======================================
# Runtime stage
# ======================================
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies only
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Copy Paho MQTT runtime libraries from builder
COPY --from=builder /usr/local/lib/libpaho* /usr/local/lib/

# Update library cache
RUN ldconfig

# Set working directory
WORKDIR /app

# Copy built executable (renamed to device-dnp3-service)
COPY --from=builder /app/build/bin/microservice_template /usr/local/bin/device-dnp3-service

# Copy configuration files
COPY --from=builder /app/configs/ /app/configs/

# Create necessary directories
RUN mkdir -p /app/logs /var/log /app/data

# Expose API port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8080/api/v1/health || exit 1

# Run the application
CMD ["/usr/local/bin/device-dnp3-service"]
