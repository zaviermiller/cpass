#include <stdio.h>
int main(){
    int i = 1;
    int j = 2;
    int a = 6;
    int b = 7;
    int c = 8;
    int d = 9;
    while(i != 100){
        if(d % 2){
            a += 14;
            if(!a % 2){
                int temp = j + 1;
                j = a;
                if(temp < j){
                    c = 11;
                    if(c == 11){
                        b = 18;
                    }
                }
            }
            d = a;
        }
        i++;
    }

    i = c;
    j = b;
    printf("i: %d\n j: %d\n a: %d\n b: %d\n c: %d\n d: %d\n", i, j, a, b, c, d);
    return 0;
}
