FROM zmq-base
COPY pkg /tmp/
RUN  mv /tmp/protoc3/bin/* /usr/local/bin/ \
  && mv /tmp/protoc3/include/* /usr/local/include/ \
  && apt-get update \
  && apt-get install -y libgflags-dev libgoogle-glog-dev \
  && apt-get autoremove --purge -y \
  && apt-get autoclean -y \
  && rm -rf /var/cache/apt/* /tmp/*