FROM ubuntu:16.04
WORKDIR /inter-judge
RUN apt-get update \
  && apt-get install -y libzmq3-dev \
  && apt-get autoremove --purge -y \
  && apt-get autoclean -y \
  && rm -rf /var/cache/apt/* /tmp/*