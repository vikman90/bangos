FROM --platform=linux/amd64 ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    gnu-efi \
    nasm \
    musl-tools \
    tar \
    qemu-system-x86 \
    ovmf \
    python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /bangos

CMD ["make", "all"]
