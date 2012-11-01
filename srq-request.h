#ifndef __REQUEST_H__
#define __REQUEST_H__

#include <stdbool.h>


typedef struct srq_tuple {
    char *name;
    char **values;
    size_t valuescount;
} tsrq_tuple;

typedef struct srq_lookup {
    tsrq_tuple **tuples;
    size_t tuplescount;
} tsrq_lookup;

typedef struct srq_file {
    char *filename;
    char *content_type;
    char *data;
    size_t length;
} tsrq_file;

typedef struct srq_request {

    tsrq_tuple **_GET;
    size_t getcount;

    tsrq_tuple **_POST;
    size_t postcount;
    tsrq_file *_FILES;
    size_t filescount;

    char *_PUT;
    size_t putcount;

} tsrq_request;


tsrq_request * srq_request_get(void);
tsrq_request * srq_request_parse(size_t maxfilesize);

void srq_request_free(tsrq_request *request);

tsrq_tuple * srq_tuple_find(const char *name, tsrq_tuple **tuples, size_t tuplescount);

#endif /* __REQUEST_H__ */

