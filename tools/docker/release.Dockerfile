# SPDX-License-Identifier: GPL-3.0-or-later
# Build host.Dockerfile first. This adds the same PS5 release tools used by CI.
ARG HOST_IMAGE=prosperoeden-host
FROM ${HOST_IMAGE}

RUN apt-get update && apt-get install -y --no-install-recommends sudo gnupg python3-pip \
    && rm -rf /var/lib/apt/lists/*
COPY ci/setup-ubuntu-24.04.sh ci/ubuntu-24.04-packages.txt /opt/prosperoeden-ci/
# Keep the translator build within the memory budget of Docker Desktop.
RUN CMAKE_BUILD_PARALLEL_LEVEL=2 bash /opt/prosperoeden-ci/setup-ubuntu-24.04.sh /opt/spirv-translator \
    && rm -rf /var/lib/apt/lists/*
ENV LLVM_CONFIG=/usr/bin/llvm-config-21
ENV PKG_CONFIG_PATH=/opt/spirv-translator/lib/pkgconfig
ENV EDEN_BUILD_JOBS=4 RADV_BUILD_JOBS=4
COPY docker/run-release.sh /usr/local/bin/prosperoeden-release
CMD ["bash", "/usr/local/bin/prosperoeden-release"]
