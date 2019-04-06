FROM inter-judge-base
RUN apt-get update \
  && apt-get install -y libseccomp-dev \
  && apt-get autoremove --purge -y \
  && apt-get autoclean -y \
  && rm -rf /var/cache/apt/* /tmp/*
COPY bin/tester bin/