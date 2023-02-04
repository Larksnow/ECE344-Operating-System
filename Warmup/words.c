#include "common.h"

int
main(int argc, char **argv)
{
    int counter=1;
    for(counter=1;counter<argc;++counter){
        printf(argv[counter]);
        printf("\n");
    }
    return 0;
}
