# Build image for MiaUI (github.com/miaui-cfw/MiaUI-GPU-RG35XX) on the RG35XX, used by port/build.sh miaui.
# MiaUI runs a Batocera/Koriki buildroot system (glibc, SDL2, PowerVR GLES2), so it needs this
# SDK instead of the GarlicOS (uClibc) compiler. Same base as MiaUI's own ports
# (miaui-cfw/Shipwright, rg35xx/Dockerfile): Debian 12 and the public Batocera RG35XX SDK
# (buildroot GCC 12.2, armhf, SDL2 and GLES2/EGL in the sysroot), relocated to /opt/rg35xx.
#   podman build -t miaui-toolchain port/miaui/
FROM docker.io/debian:bookworm-slim

ARG SDK_URL=https://github.com/rg35xx-cfw/rg35xx-cfw.github.io/releases/download/rg35xx_batocera_sdk_alpha_0.5/arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y make gcc g++ cmake ninja-build python3 file wget cpio zip unzip \
        rsync bc git autoconf curl pkg-config zlib1g-dev libbz2-dev libpng-dev ca-certificates \
 && apt-get -y autoremove && apt-get -y clean && rm -rf /var/lib/apt/lists/*

RUN curl -sSfL "$SDK_URL" | tar -xzC /opt \
 && mv "/opt/$(ls /opt)" /opt/rg35xx \
 && /opt/rg35xx/relocate-sdk.sh

ENV PATH=/opt/rg35xx/bin:$PATH
WORKDIR /root/workspace
CMD ["/bin/bash"]
