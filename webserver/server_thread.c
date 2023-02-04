#include "common.h"
#include "request.h"
#include "server_thread.h"
#define max_table_size 1000
typedef enum {false, true} bool;
pthread_mutex_t cache_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct lru_node{
    char* file_name;
    struct lru_node* next;
}lru_node;

typedef struct lru_list{
    struct lru_node* head;
}lru_list;

typedef struct cache_node{
    struct file_data* data;
    bool inuse;
    struct cache_node* next;
}cache_node;

typedef struct rq_cache {
    int tablesize;
    int has_used_size;
    lru_list* lrulist;
    cache_node** cache_table;
}rq_cache;

struct server {
	int nr_threads;
	int max_requests;
	int max_cache_size;
	int exiting;
	/* add any other parameters you need */
	int *conn_buf;
	pthread_t *threads;
	int request_head;
	int request_tail;
	pthread_mutex_t mutex;
	pthread_cond_t prod_cond;
	pthread_cond_t cons_cond;
        rq_cache* cache;
};

struct request {
	int fd;		 /* descriptor for client connection */
	struct file_data *data;
};

/* static functions */

static struct file_data *file_data_init(void);
static void file_data_free(struct file_data *data);

void push_lru(char* filename, lru_list* list){//add a thread node at the  tail of lru list
    lru_node* newnode = (lru_node*)malloc(sizeof(lru_node));
    assert(newnode);
    newnode->file_name = filename;
    newnode->next = NULL;
    if(list->head == NULL){//empty list
        list->head = newnode;
    }
    else{
        lru_node* cur = list->head;
        while(cur->next != NULL){//find the end of list
            cur = cur->next;
        }
        cur->next = newnode;
    }
}

void pop_lru_first(lru_list *list){// pop the first node from lru list   
    lru_node* cur = list->head;
    list->head = cur->next;
    free(cur->file_name);
    free(cur);
}

void pop_lru_want(char* filename, lru_list *list){//pop particular node by pointer form lru_list
    assert(list->head != NULL);//quit if it is an empty list
    lru_node* cur = list->head;
    lru_node* pre = NULL;
    while(cur != NULL){
        if(strcmp(cur->file_name, filename) == 0){
            if(pre == NULL){
                list->head = cur->next;        
                free(cur->file_name);
                free(cur);
                break;
            }
            else{
                pre->next = cur->next;
                free(cur->file_name);
                free(cur); 
                break;
            }         
        }
        else{
            pre = cur;
            cur = cur->next;
        }
    }
}

void destroy_lru(lru_list* list){
    lru_node* cur = list->head;
    lru_node* next = NULL;
    while(cur!= NULL){
        next = cur->next;
        free(cur->file_name);
        free(cur);
        cur = next;
    }
    free(list);
}

/*void print_list(lru_list* list){
    lru_node* cur = list->head;
    int counter = 0;
    while(cur!= NULL){
        cur = cur->next;
        counter++;
    }
}*/

int algorithm(char* key, rq_cache* cache){
    int length = strlen(key);
    int result = 2*length+1;
    for(int i=0;i<length;++i){
        result = (result * 33) + (int)key[i]; //polynomial rolling hash function
    }
    result = abs(result%(cache->tablesize));
    return result;
}

rq_cache* cache_init(){
    rq_cache* cache = (rq_cache*)malloc(sizeof(rq_cache));
    assert(cache);
    cache->has_used_size = 0;
    cache->tablesize = max_table_size;
    cache->lrulist = (lru_list*)malloc(sizeof(lru_list));
    assert(cache->lrulist);
    cache->lrulist->head = NULL;
    cache->cache_table = (cache_node**)malloc(sizeof(cache_node*)* max_table_size);
    assert(cache->cache_table);
    for(int i=0; i<max_table_size; ++i){
        cache->cache_table[i] = NULL;
    }
    return cache;
}

cache_node* cache_node_search(char* key, rq_cache* cache){
    int index = algorithm(key, cache);
    cache_node* cur = cache->cache_table[index];
    while(cur != NULL){
        if(strcmp(cur->data->file_name, key) == 0){
            return cur;
        }
        else{
            cur = cur->next;
        }
    }
    return NULL;
}

bool cache_evict(struct server* sv, int new_size){
    int pridict_size = sv->cache->has_used_size + new_size;
    while(pridict_size>sv->max_cache_size){
        assert(sv->cache->lrulist->head != NULL);
        char* name = sv->cache->lrulist->head->file_name;//find the first node in lru               
        cache_node* want_node = cache_node_search(name, sv->cache);//find this filename in table
        if(want_node->inuse == true){
            return false;
        }
        int filesize = want_node->data->file_size;
        int index = algorithm(name, sv->cache);
        cache_node* cur = sv->cache->cache_table[index];
        cache_node* pre = NULL;
        while(cur != NULL){
            if(cur == want_node){
                if(pre == NULL){//first one in the sublist
                    sv->cache->cache_table[index] = cur->next;
                    break;
                }
                else{
                    pre->next = cur->next;
                    break;
                }
            }
            else{
                pre = cur;
                cur = cur->next;
            }
        }
        sv->cache->has_used_size -= filesize;
        pridict_size = sv->cache->has_used_size + new_size;
        file_data_free(want_node->data);
        free(want_node);
        pop_lru_first(sv->cache->lrulist);       
    }
    return true;
}

cache_node* insert_cache(struct file_data* data, struct server* sv){
    int datasize = data->file_size;
    char* key = data->file_name;
    int index = algorithm(key, sv->cache);
    if(datasize >= sv->max_cache_size){//if file size greater than limit
        return NULL;
    }
    int pridict_size = sv->cache->has_used_size + datasize;  
    if(pridict_size > sv->max_cache_size){
        if(cache_evict(sv, datasize) == false){
            return NULL;
        }
    }
    cache_node* new_node = (cache_node*)malloc(sizeof(cache_node));
    assert(new_node);
    new_node->inuse = false;
    new_node->data = file_data_init();
    new_node->next = sv->cache->cache_table[index];
    new_node->data->file_size = datasize;
    new_node->data->file_name = strdup(key);
    new_node->data->file_buf = strdup(data->file_buf);
    sv->cache->cache_table[index] = new_node;
    sv->cache->has_used_size += datasize;
    push_lru(key, sv->cache->lrulist);
    //print_list(sv->cache->lrulist);
    return new_node;   
}

void rq_cache_destroy(rq_cache *cache){
    for (int i = 0; i < cache->tablesize; ++i){
        cache_node* cur = cache->cache_table[i];
        cache_node* next = NULL;
        while(cur != NULL){
            next = cur->next;
            free(cur->data->file_buf);
            free(cur->data->file_name);
            free(cur->data);
            free(cur);
            cur = next;
        }
    }
    destroy_lru(cache->lrulist);
    free(cache);
}

/* initialize file data */
static struct file_data *
file_data_init(void)
{
	struct file_data *data;

	data = Malloc(sizeof(struct file_data));
	data->file_name = NULL;
	data->file_buf = NULL;
	data->file_size = 0;
	return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data)
{
	free(data->file_name);
	free(data->file_buf);
	free(data);
}

static void
do_server_request(struct server *sv, int connfd)
{
	int ret;
	struct request *rq;
	struct file_data *data;

	data = file_data_init();

	/* fill data->file_name with name of the file being requested */
	rq = request_init(connfd, data);
	if (!rq) {
		file_data_free(data);
		return;
	}//now we have know the file name
        //check if in cache first
        char* filename = strdup(data->file_name);
        if(sv->max_cache_size == 0){//no cache needed
            ret = request_readfile(rq);
            if(ret == 0){
                goto out;
            }
            request_sendfile(rq);
        }
        else{
            pthread_mutex_lock(&cache_lock);
            cache_node* want_node = cache_node_search(filename, sv->cache);
            if(want_node != NULL){//we find one in the cache
                data->file_size= want_node->data->file_size;
                data->file_name = strdup(want_node->data->file_name);
                data->file_buf = strdup(want_node->data->file_buf);
                want_node->inuse = true;
                request_set_data(rq, data);
                pop_lru_want(filename,sv->cache->lrulist);
                push_lru(filename, sv->cache->lrulist);
                //print_list(sv->cache->lrulist);
            }else{//if not in the cache,  then read from disk
                pthread_mutex_unlock(&cache_lock);
                ret = request_readfile(rq);
                if (ret == 0) { 
                    goto out;
                }
                pthread_mutex_lock(&cache_lock);
                want_node = insert_cache(data, sv);
                if(want_node != NULL){
                    want_node->inuse = true;
                }
            } 
            pthread_mutex_unlock(&cache_lock);
            request_sendfile(rq);
            pthread_mutex_lock(&cache_lock);
            if(want_node != NULL){
                want_node->inuse = false;
            }
            pthread_mutex_unlock(&cache_lock);
        }

out:
	request_destroy(rq);
	//file_data_free(data);
}

static void *
do_server_thread(void *arg)
{
	struct server *sv = (struct server *)arg;
	int connfd;

	while (1) {
		pthread_mutex_lock(&sv->mutex);
		while (sv->request_head == sv->request_tail) {
			/* buffer is empty */
			if (sv->exiting) {
				pthread_mutex_unlock(&sv->mutex);
				goto out;
			}
			pthread_cond_wait(&sv->cons_cond, &sv->mutex);
		}
		/* get request from tail */
		connfd = sv->conn_buf[sv->request_tail];
		/* consume request */
		sv->conn_buf[sv->request_tail] = -1;
		sv->request_tail = (sv->request_tail + 1) % sv->max_requests;
		
		pthread_cond_signal(&sv->prod_cond);
		pthread_mutex_unlock(&sv->mutex);
		/* now serve request */
		do_server_request(sv, connfd);
	}
out:
	return NULL;
}

/* entry point functions */

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size)
{
	struct server *sv;
        int i;

	sv = Malloc(sizeof(struct server));
	sv->nr_threads = nr_threads;
	/* we add 1 because we queue at most max_request - 1 requests */
	sv->max_requests = max_requests + 1;
	sv->max_cache_size = max_cache_size;
	sv->exiting = 0;

	/* Lab 4: create queue of max_request size when max_requests > 0 */
	sv->conn_buf = Malloc(sizeof(*sv->conn_buf) * sv->max_requests);
	for (i = 0; i < sv->max_requests; i++) {
		sv->conn_buf[i] = -1;
	}
	sv->request_head = 0;
	sv->request_tail = 0;

	/* Lab 5: init server cache and limit its size to max_cache_size */
        sv->cache = cache_init();;
	/* Lab 4: create worker threads when nr_threads > 0 */
	pthread_mutex_init(&sv->mutex, NULL);
	pthread_cond_init(&sv->prod_cond, NULL);
	pthread_cond_init(&sv->cons_cond, NULL);	
	sv->threads = Malloc(sizeof(pthread_t) * nr_threads);
	for (i = 0; i < nr_threads; i++) {
		SYS(pthread_create(&(sv->threads[i]), NULL, do_server_thread,
				   (void *)sv));
	}
	return sv;
}

void
server_request(struct server *sv, int connfd)
{
	if (sv->nr_threads == 0) { /* no worker threads */
		do_server_request(sv, connfd);
	} else {
		/*  Save the relevant info in a buffer and have one of the
		 *  worker threads do the work. */

		pthread_mutex_lock(&sv->mutex);
		while (((sv->request_head - sv->request_tail + sv->max_requests)
			% sv->max_requests) == (sv->max_requests - 1)) {
			/* buffer is full */
			pthread_cond_wait(&sv->prod_cond, &sv->mutex);
		}
		/* fill conn_buf with this request */
		assert(sv->conn_buf[sv->request_head] == -1);
		sv->conn_buf[sv->request_head] = connfd;
		sv->request_head = (sv->request_head + 1) % sv->max_requests;
		pthread_cond_signal(&sv->cons_cond);
		pthread_mutex_unlock(&sv->mutex);
	}
}

void
server_exit(struct server *sv)
{
	int i;
	/* when using one or more worker threads, use sv->exiting to indicate to
	 * these threads that the server is exiting. make sure to call
	 * pthread_join in this function so that the main server thread waits
	 * for all the worker threads to exit before exiting. */
	pthread_mutex_lock(&sv->mutex);
	sv->exiting = 1;
	pthread_cond_broadcast(&sv->cons_cond);
	pthread_mutex_unlock(&sv->mutex);
	for (i = 0; i < sv->nr_threads; i++) {
		pthread_join(sv->threads[i], NULL);
	}

	/* make sure to free any allocated resources */
	free(sv->conn_buf);
	free(sv->threads);
        if(sv->cache!=NULL){
            rq_cache_destroy(sv->cache);
        }
	free(sv);
}
