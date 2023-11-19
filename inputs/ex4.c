#include <stdio.h>
int main(){
    int i = 1;
    int j = 2;
    int a = 6;
    int b = 7;
    int c = 10;
    int d = 11;
    for(i = 0; i <= 10; i++){
        int temp = i + 19;
        j = 8;
    }
    a = i;
    d = j;
    int temp2 = c;

    printf("i: %d\n j: %d\n a: %d\n b: %d\n c: %d\n d: %d\n", i, j, a, b, c, d);

    return 0;
}
