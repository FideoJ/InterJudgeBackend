FROM inter-judge-base
RUN apt-get update \
  && apt-get install -y g++ \
  && apt-get autoremove --purge -y \
  && apt-get autoclean -y \
  && rm -rf /var/cache/apt/* /tmp/*
COPY bin/compiler_gpp bin/