#!/bin/bash

set -e

if command -v go &> /dev/null 
then
    echo "Go installed"
    exit
fi

apt-get update
apt install -y wget

echo "installing.."
wget -c https://go.dev/dl/go1.20.2.linux-amd64.tar.gz
tar -xzf go1.20.2.linux-amd64.tar.gz  -C /usr/local
echo "export PATH=$PATH:/usr/local/go/bin" >> ${HOME}/.profile
source ${HOME}/.profile
mkdir -p ${HOME}/.config/go
echo "
GOPROXY=https://goproxy.io,direct
GOPRIVATE=
GOSUMDB=off
" >> ${HOME}/.config/go/env
source ${HOME}/.config/go/env
