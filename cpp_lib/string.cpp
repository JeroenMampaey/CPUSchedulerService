#include "string.h"

void intToDecimalString(int n, char str[]){
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do {
        str[i++] = (n % 10) + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) str[i++] = '-';
    str[i] = '\0';

    reverse(str);
}

void unsignedIntToDecimalString(unsigned int n, char str[]){
    int i = 0;
    do {
        str[i++] = (n % 10) + '0';
    } while ((n /= (unsigned int)10) > 0);

    str[i] = '\0';

    reverse(str);
}

void intToHexadecimalString(unsigned int n, char str[]){
    int i = 0;
    do {
        if((n % 16) >= 10){
            str[i++] = (n % 16) - 10 + 'A';
        }
        else{
            str[i++] = (n % 16) + '0';
        }
    } while ((n /= 16) > 0);

    str[i] = '\0';

    reverse(str);
}

void reverse(char s[]){
    int c, i, j;
    for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

int strlen(char s[]){
    int i = 0;
    while (s[i] != '\0') ++i;
    return i;
}