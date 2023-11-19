#include <stdio.h>
int m[6];

int scale(int x) {
  int i;

  if (x == 0)
    return 0;
  for (i = 0; i < 6; i += 1)
    m[i] *= x;
  return 1;
}

int main() {
  int i;
  int x;
  int z = 9;

  i = 0;
  while (i < 6) {
    m[i] = i;
    i = i + 1;
  }
  int j = z;
  int temp = i;

  printf("%d %d %d\n", m[0], m[2], m[5]);

  x = j;
  scale (x);

  printf("%d %d %d\n", m[0], m[2], m[5]);

  return 0;
}
