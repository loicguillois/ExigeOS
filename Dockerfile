# ExigeOS build environment
#
# This image contains all tools needed to compile ExigeOS for both
# supported platforms (x86 and Raspberry Pi 3B) without installing
# anything on your local machine.
#
# Usage:
#   docker pull ghcr.io/loicguillois/exigeos:latest
#
#   # Build for x86
#   docker run --rm -v "$PWD":/src ghcr.io/loicguillois/exigeos make PLATFORM=x86
#
#   # Build for RPi3
#   docker run --rm -v "$PWD":/src ghcr.io/loicguillois/exigeos make PLATFORM=rpi3

FROM ubuntu:22.04

LABEL org.opencontainers.image.title="ExigeOS build environment"
LABEL org.opencontainers.image.description="Cross-compilation toolchain for ExigeOS (x86 + Raspberry Pi 3B)"
LABEL org.opencontainers.image.source="https://github.com/loicguillois/ExigeOS"
LABEL org.opencontainers.image.licenses="MIT"

RUN apt-get update -qq && \
    apt-get install -y --no-install-recommends \
        gcc \
        libc6-dev-i386 \
        nasm \
        binutils \
        gcc-aarch64-linux-gnu \
        binutils-aarch64-linux-gnu \
        make \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
