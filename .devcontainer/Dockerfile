FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Re-enable documentation installation for manpages
RUN rm -f /etc/dpkg/dpkg.cfg.d/excludes || true

# Install base tools
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y \
    build-essential \
    gcc-multilib \
    g++-multilib \
    libc6-dev-i386 \
    libc6:i386 \
    libc6-dbg:i386 \
    libstdc++6:i386 \
    valgrind \
    valgrind-dbg \
    gdb \
    nasm \
    hexedit \
    man-db \
    manpages \
    manpages-dev \
    sudo \
    && rm -rf /var/lib/apt/lists/*
# Create vscode user
RUN useradd -ms /bin/bash vscode \
    && echo "vscode ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

USER root
WORKDIR /workspace