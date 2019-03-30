#ifndef LOGGER_HPP
#define LOGGER_HPP
#include <glog/logging.h>

#define LOG_SYS_ERR                                                            \
  do {                                                                         \
    LOG(ERROR) << strerror(errno);                                             \
  } while (0)
#define LOG_FATAL_SYS_ERR                                                      \
  do {                                                                         \
    LOG(FATAL) << strerror(errno);                                             \
  } while (0)

#endif