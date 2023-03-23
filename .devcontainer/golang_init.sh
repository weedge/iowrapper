#!/usr/bin/env bash

set -e

apt-get update
apt install -y wget
go version

if [ $? -ne 0 ];then
    wget -c https://go.dev/dl/go1.20.2.linux-amd64.tar.gz
    tar -xzf go1.20.2.linux-amd64.tar.gz  -C /usr/local
    echo "export PATH=$PATH:/usr/local/go/bin" >> ${HOME}/.profile
    source ${HOME}/.profile
    mkdir -p touch ${HOME}/.config/go
    echo "
    GOPROXY=https://goproxy.io,direct
    GOPRIVATE="github.com"
    GOSUMDB=off
    " >> ${HOME}/.config/go/env
    source ${HOME}/.config/go/env
fi