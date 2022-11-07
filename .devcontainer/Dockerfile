# See here for image contents: https://github.com/microsoft/vscode-dev-containers/tree/v0.245.2/containers/ubuntu/.devcontainer/base.Dockerfile

# [Choice] Ubuntu version (use ubuntu-22.04 or ubuntu-18.04 on local arm64/Apple Silicon): ubuntu-22.04, ubuntu-20.04, ubuntu-18.04
ARG VARIANT="jammy"
FROM mcr.microsoft.com/vscode/devcontainers/base:0-${VARIANT}

RUN export DEBIAN_FRONTEND=noninteractive \
    && mkdir -p /usr/local/share/keyring/ /build \
    && ln -s /proc/self/mounts /etc/mtab \
    && wget -O /usr/local/share/keyring/devkitpro-pub.gpg https://apt.devkitpro.org/devkitpro-pub.gpg \
    && echo "deb [signed-by=/usr/local/share/keyring/devkitpro-pub.gpg] https://apt.devkitpro.org stable main" > /etc/apt/sources.list.d/devkitpro.list \
    && apt-get update && apt-get upgrade -y \
    && apt-get -y install --no-install-recommends \
      apt-utils \
      bzip2 \
      ca-certificates \
      cmake \
      curl \
      devkitpro-pacman \
      doxygen \
      gnupg \
      git \
      git-restore-mtime \
      lftp \
      locales \
      make \
      pkg-config \
      sudo \
      unzip \
      wget \
      xz-utils \
      zip \
    && sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen \
    && ln -s /opt/devkitpro/portlibs/wiiu/bin/powerpc-eabi-cmake /usr/bin/wiiu-cmake \
    && dpkg-reconfigure --frontend=noninteractive locales \
    && update-locale LANG=en_US.UTF-8 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/* \
    && dkp-pacman -Syyu --noconfirm --needed wiiu-dev ppc-zlib

ENV LANG en_US.UTF-8
ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITPPC=${DEVKITPRO}/devkitPPC

ENV PATH=${DEVKITPRO}/tools/bin:${DEVKITPPC}/bin:$PATH

COPY --from=wiiuenv/libmappedmemory:20220904 /artifacts ${DEVKITPRO}
COPY --from=wiiuenv/wiiumodulesystem:20220904 /artifacts ${DEVKITPRO}