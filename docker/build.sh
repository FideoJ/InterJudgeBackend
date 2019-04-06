#!/bin/bash
mkdir -p ../pkg/protoc3
cd ../pkg
curl -OL https://github.com/google/protobuf/releases/download/v3.2.0/protoc-3.2.0-linux-x86_64.zip
unzip protoc-3.2.0-linux-x86_64.zip -d protoc3
cd -
docker build -t zmq-base -f zmq-base.Dockerfile ..
docker build -t inter-judge-base -f inter-judge-base.Dockerfile ..
docker build -t broker -f broker.Dockerfile ..
docker build -t file_provider -f file_provider.Dockerfile ..
docker build -t tester -f tester.Dockerfile ..
docker build -t compiler_gpp -f compiler_gpp.Dockerfile ..
