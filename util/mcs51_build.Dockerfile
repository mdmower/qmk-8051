# Build environment for util/mcs51_build.py: the official SDCC binary (MCS51 files only), binutils (objcopy: the
# .ihx to ELF conversion of platforms/mcs51/sdcc_cc.py), clang, clang-query and llvm-nm (sdcc_cc.py: weak
# definitions and the check for constructs that SDCC compiles to wrong code) and the QMK CLI.
# SDCC 4.5.0 (the Debian trixie package) stops with an internal error on tmk_core/protocol/ch555/usb_device.c
# (SDCC bug #3894, fixed in 4.6.0). 4.4.0 also works.
# git is not installed: util/mcs51_build.py passes SKIP_GIT=yes, so version.h has QMK_VERSION "NA" (the default
# images do not contain it; VIA images contain its QMK_BUILDDATE).
# util/mcs51_build.py builds this image with the tree's requirements.txt as the only other file in the context,
# and tags it qmk-8051-mcs51:sdcc-<SDCC_VER>-clang.
ARG SDCC_VER=4.6.0

FROM debian:trixie-slim AS sdcc
ARG SDCC_VER
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl bzip2
RUN curl -fsSL -o /tmp/sdcc.tar.bz2 \
        "https://sourceforge.net/projects/sdcc/files/sdcc-linux-amd64/${SDCC_VER}/sdcc-${SDCC_VER}-amd64-unknown-linux2.5.tar.bz2/download" \
    && sha256sum /tmp/sdcc.tar.bz2 \
    && mkdir /tmp/sdcc && tar -xjf /tmp/sdcc.tar.bz2 -C /tmp/sdcc --strip-components=1
# Keep only the files an MCS51 build with --model-large uses.
RUN mkdir -p /opt/sdcc/bin /opt/sdcc/share/sdcc/lib && cd /tmp/sdcc \
    && cp bin/sdcc bin/sdcpp bin/sdas8051 bin/sdld /opt/sdcc/bin/ \
    && cp -r libexec /opt/sdcc/ \
    && cp -r share/sdcc/include /opt/sdcc/share/sdcc/ \
    && cp -r share/sdcc/lib/large /opt/sdcc/share/sdcc/lib/

FROM debian:trixie-slim
RUN apt-get update && apt-get install -y --no-install-recommends binutils libstdc++6 make python3 python3-venv \
        clang clang-tools llvm \
    && rm -rf /var/lib/apt/lists/*
COPY --from=sdcc /opt/sdcc /opt/sdcc
COPY requirements.txt /tmp/req.txt
RUN python3 -m venv /opt/qmk && /opt/qmk/bin/pip install --no-cache-dir qmk -r /tmp/req.txt && rm /tmp/req.txt
ENV PATH=/opt/sdcc/bin:/opt/qmk/bin:$PATH QMK_HOME=/qmk
