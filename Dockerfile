FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake libcurl4-openssl-dev nlohmann-json3-dev git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)


FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    libcurl4 \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

RUN useradd -m -s /bin/bash mcpp

COPY --from=builder /build/build/mcpp_example /usr/local/bin/mcpp_example
COPY --from=builder /build/mcpp_launcher.sh /usr/local/bin/mcpp_launcher.sh

RUN chmod +x /usr/local/bin/mcpp_example /usr/local/bin/mcpp_launcher.sh

USER mcpp

ENTRYPOINT ["/usr/local/bin/mcpp_example"]
