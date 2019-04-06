#!/bin/bash
docker build -t zmq-base -f zmq-base.Dockerfile ..
docker build -t inter-judge-base -f inter-judge-base.Dockerfile ..
docker build -t broker -f broker.Dockerfile ..
docker build -t file_provider -f file_provider.Dockerfile ..
docker build -t tester -f tester.Dockerfile ..
docker build -t compiler_gpp -f compiler_gpp.Dockerfile ..
