#!/bin/bash

unamestr=`uname`
sudo="sudo "
packagemgr="apt-get"
subcmd=" install "
packages="build-essential cmake cmake-extras extra-cmake-modules libmpich-dev \
mpich python3-matplotlib"


if [[ "$unamestr" == 'Linux' ]]; then
    if command -v "$packagemgr" > /dev/null 2>&1; then
        if eval "$sudo$packagemgr$subcmd$packages"; then
            echo "[SUCCESS] Required packages are installed."
        else
            echo "[FAIL] Required packages are not installed properly."
        fi
    else
        echo "[FAIL] Only Ubuntu > 16.04 is supported"
    fi
elif [[ "$unamestr" == 'FreeBSD' ]]; then
    echo "[FAIL] FreeBSD is not supported"
else
    echo "[FAIL] Unknown platform"
fi
