# SPDX-License-Identifier: GPL-3.0-or-later
# Linux host tests only; the PS5 release toolchain is separate.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ca-certificates clang-18 lld-18 llvm-18 libclang-rt-18-dev \
    git curl wget unzip xz-utils file patch pkg-config ninja-build ccache nasm \
    rsync autoconf automake libtool bison flex glslang-tools spirv-tools python3 python3-venv \
    libssl-dev libcurl4-openssl-dev libzstd-dev nlohmann-json3-dev libudev-dev \
    libicu-dev zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/cmake \
    && /opt/cmake/bin/pip install --no-cache-dir cmake==3.31.10 \
    && ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake \
    && ln -s /opt/cmake/bin/ctest /usr/local/bin/ctest \
    && ln -s /opt/cmake/bin/cpack /usr/local/bin/cpack

ENV PYTHONDONTWRITEBYTECODE=1
ENV XDG_CACHE_HOME=/work/cache
COPY run-host-tests.sh /usr/local/bin/prosperoeden-host-tests
WORKDIR /work/ProsperoEden
CMD ["bash", "/usr/local/bin/prosperoeden-host-tests"]
