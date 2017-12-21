#!/usr/bin/env sh
if [ ! -d "$HOME/protoc/lib" ]; then
    wget https://github.com/google/protobuf/archive/v3.5.1.tar.gz -O protobuf.tar.gz
    tar -xzvf protobuf.tar.gz
    mv protobuf-3.5.1 protobuf
    cd protobuf
    ./autogen.sh
    ./configure --prefix=$HOME/protoc
    make && make install
    export PROTOC="$HOME/instant/protobuf/src/protoc"
    cd ..
else
    echo "Using cached directory."
fi
