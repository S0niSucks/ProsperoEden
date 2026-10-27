# SPDX-License-Identifier: GPL-3.0-or-later
# Optional offscreen renderer checks; no display server or host toolchain is needed.
ARG HOST_IMAGE=prosperoeden-host
FROM ${HOST_IMAGE}
RUN apt-get update && apt-get install -y --no-install-recommends \
    libvulkan-dev mesa-vulkan-drivers vulkan-validationlayers \
    && rm -rf /var/lib/apt/lists/*
