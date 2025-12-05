FROM drachtio/drachtio-freeswitch-mrf:latest AS freeswitch-base

FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    git \
    build-essential \
    cmake \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    libcurl4-openssl-dev \
    libcjson-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp
RUN git clone https://github.com/nats-io/nats.c && \
    cd nats.c && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd /tmp && rm -rf nats.c

COPY --from=freeswitch-base /usr/local/freeswitch/include/freeswitch/ /tmp/freeswitch_headers/

WORKDIR /workspace
COPY Makefile .
COPY src/ src/
COPY include/ include/
COPY lib/ lib/

RUN make DRIVER=nats clean all

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libssl3 \
    libcurl4 \
    zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /usr/local/lib/libnats.so* /usr/local/lib/
COPY --from=builder /workspace/mod_event_agent.so /usr/local/lib/

RUN ldconfig

CMD ["/bin/bash"]
