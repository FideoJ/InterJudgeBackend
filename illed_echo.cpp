#include <stdio.h>

int main() {
  for (int i = 0; i < argc - 1; ++i) {
    printf("%s ", argv[i]);
  }
  printf("%s", argv[argc - 1]);
  return 0;
}