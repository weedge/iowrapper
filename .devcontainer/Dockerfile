# See here for image contents: https://github.com/microsoft/vscode-dev-containers
FROM mcr.microsoft.com/devcontainers/go:0-1.19-bullseye

# [Optional] Install CMake version different from what base image has already installed. 
# CMake reinstall choices: none, 3.21.5, 3.22.2, or versions from https://cmake.org/download/
ARG REINSTALL_CMAKE_VERSION_FROM_SOURCE="none"

COPY ./ubuntu_init.sh /tmp/
COPY ./reinstall-cmake.sh /tmp/
RUN chmod +x /tmp/ubuntu_init.sh && /tmp/ubuntu_init.sh && rm -f /tmp/ubuntu_init.sh \
    && if [ "${REINSTALL_CMAKE_VERSION_FROM_SOURCE}" != "none" ]; then \
    chmod +x /tmp/reinstall-cmake.sh && /tmp/reinstall-cmake.sh ${REINSTALL_CMAKE_VERSION_FROM_SOURCE}; \
    fi \
    && rm -f /tmp/reinstall-cmake.sh
