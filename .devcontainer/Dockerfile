# Use Ubuntu 24.04 as base image
FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update and install essential packages
RUN apt-get update && apt-get install -y \
    # Build tools
    build-essential \
    gcc \
    g++ \
    make \
    cmake \
    git \
    # Libraries
    libssl-dev \
    libmbedtls-dev \
    # Debugging and tools
    gdb \
    valgrind \
    strace \
    # Network tools
    chrony \
    net-tools \
    iputils-ping \
    tcpdump \
    # Additional utilities
    vim \
    nano \
    wget \
    curl \
    tree \
    htop \
    sudo \
    # Python for scripts
    python3 \
    python3-pip \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a non-root user (vscode)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

# Create directories for FreeRTOS
RUN mkdir -p /workspace/FreeRTOS-Kernel \
    && mkdir -p /workspace/FreeRTOS-Plus-TCP \
    && chown -R $USERNAME:$USERNAME /workspace

# Copy chrony configuration
COPY chrony.conf /etc/chrony/chrony.conf

# Copy startup script
COPY start-chrony.sh /usr/local/bin/start-chrony.sh
RUN chmod +x /usr/local/bin/start-chrony.sh

# Expose NTP port
EXPOSE 123/udp

# Set working directory
WORKDIR /workspaces/freertos-ntp

# Switch to non-root user
USER $USERNAME

# Set environment variables
ENV FREERTOS_PATH=/workspace/FreeRTOS-Kernel
ENV FREERTOS_TCP_PATH=/workspace/FreeRTOS-Plus-TCP
ENV FREERTOS_PORT=GCC/Posix
