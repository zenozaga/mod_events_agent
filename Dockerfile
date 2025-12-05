FROM debian:bookworm-slim AS builder

# Install dependencies for building
RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libcurl4-openssl-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Install NATS C Client
WORKDIR /tmp
RUN git clone https://github.com/nats-io/nats.c && \
    cd nats.c && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd /tmp && rm -rf nats.c

# Download FreeSWITCH dev headers (lightweight alternative)
RUN mkdir -p /tmp/freeswitch_headers && \
    cd /tmp/freeswitch_headers && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_types.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_apr.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_platform.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_module_interfaces.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_core.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_loadable_module.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_log.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_channel.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_utils.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_buffer.h && \
    wget -q https://raw.githubusercontent.com/signalwire/freeswitch/master/src/include/switch_event.h

# Set environment
ENV PATH="/usr/local/bin:${PATH}"
ENV PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH}"

WORKDIR /work

CMD ["/bin/bash"]
