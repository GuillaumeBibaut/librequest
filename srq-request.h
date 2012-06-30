#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <stdbool.h>


typedef struct srq_pair {
    char *name;
    char *value;
} tsrq_pair;

typedef struct srq_lookup {
    tsrq_pair *pairs;
    size_t pairscount;
} tsrq_lookup;

typedef struct srq_file {
    char *filename;
    char *content_type;
    char *data;
    size_t length;
} tsrq_file;

typedef struct srq_request {

    tsrq_pair *_GET;
    size_t getcount;

    tsrq_pair *_POST;
    size_t postcount;
    tsrq_file *_FILES;
    size_t filescount;

    char *_PUT;
    size_t putcount;

} tsrq_request;


tsrq_request * srq_request_get(void);
tsrq_request * srq_request_parse(size_t maxfilesize);

void srq_request_free(tsrq_request *request);

bool srq_pair_lookup(const char *name, tsrq_lookup lookup, tsrq_lookup *result);

#endif /* __REQUEST_H__ */
