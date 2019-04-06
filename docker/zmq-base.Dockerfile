FROM ubuntu:16.04
COPY docker/sources.list /etc/apt/
WORKDIR /inter-judge
RUN apt-get update \
  && apt-get install -y libzmq3-dev \
  && apt-get autoremove --purge -y \
  && apt-get autoclean -y \
  && rm -rf /var/cache/apt/* /tmp/*