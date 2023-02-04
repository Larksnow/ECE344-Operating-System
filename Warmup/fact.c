#include "common.h"

int
main(int argc, char **argv)
{
    int integer;
    double flo;
    int fact=1;
    flo=atof(argv[1]);
    integer=atol(argv[1]);
    if(flo-integer!=0){
        printf("Huh?\n");
    }
    else if(integer<=0){
        printf("Huh?\n");
    }
    else if(integer>12){
        printf("Overflow\n");
    }
    else{
        while(integer>1){
            fact=fact*integer;
            integer--;
        }
        printf("%d\n",fact);
    }
    return 0;
}
