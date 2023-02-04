//Fan Weite #1006761933
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "common.h"
#include "wc.h"

typedef struct sublist{
    char* key;
    int counter;
    struct sublist* next;
}sub;

struct wc {
    int listsize;
    sub **list;
};

sub* new_sub(char* key){
	sub* newsub = (sub*)malloc(sizeof(sub));
	if(newsub == NULL){//check if memory allocate is available
            return NULL;
	}
	newsub -> key = strdup(key);
        if(newsub -> key == NULL){//check if there is an empty input
            return NULL;
	}
	newsub -> counter = 1;
	newsub -> next = NULL;
	return newsub;
}

int algorithm(char* key, struct wc *ht){
    int result=0;
    int base = 21;
    int length = strlen(key);
    for(int i=0;i<length;++i){
        result = (result * base) + (int)key[i]; //polynomial rolling hash function
    }
    result = abs(result%(ht->listsize));
    return result;
}

void inserttail(char *key, struct wc *ht){
    int index = algorithm(key,ht);
    sub *tptr = ht->list[index];
    sub *nptr = NULL;
    while (tptr != NULL && (strcmp(key,tptr->key) != 0)){//list traversal
        nptr = tptr;
        tptr = tptr ->next;
    }
    if (tptr != NULL && (strcmp(key,tptr->key) == 0)){//if find the same word add counter
        ++(tptr->counter);
    }
    else{
        sub *newsub = new_sub(key);
        assert(newsub);
        if(nptr != NULL) {
            nptr -> next = newsub;
        }
        else ht -> list[index] = newsub;
    }
}

struct wc *
wc_init(char *word_array, long size)
{
    int listsize=2*size;
    struct wc *wc;
    wc = (struct wc *)malloc(sizeof(struct wc));
    assert(wc);
    wc-> list = malloc((sizeof(sub**)) * listsize);//allocate memory for pointer array
    assert(wc->list);
    wc-> listsize = listsize;
    for (int i = 0; i < listsize; ++i){
        wc->list[i] = NULL;
    }
    char word[100];
    for (int i = 0; i <size; ++i){
        int wordlen = 0;
        while(!isspace(word_array[i])){
            ++i;
            ++wordlen;
        }
        if (wordlen != 0){
            memset(word,0,100);//reset word
            for(int j = 0; j<wordlen; ++j){
                word[j] = word_array[i-wordlen+j];//copy word from word_array
            }
            inserttail(word,wc);
        }
    }
    return wc;
}

void
wc_output(struct wc *wc)
{
    sub *word;
    for (int i = 0; i < wc->listsize; ++i){
        word = wc->list[i];
        while(word != NULL){
            printf("%s:%d\n",word->key,word->counter);
            word = word->next;
        }
    }
}

void
wc_destroy(struct wc *wc)
{
    for (int i = 0; i < wc->listsize; ++i){
        sub *tptr = wc->list[i];
        sub *nptr;
        while(tptr != NULL){
            nptr = tptr->next;
            free(tptr);
            tptr = nptr;
        }
    }
    free(wc);
}
