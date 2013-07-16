#ifndef __SRQ_TUPLE_H__
#define __SRQ_TUPLE_H__

#include <stddef.h>

typedef struct srq_tuple {
    char *name;
    char **values;
    size_t valuescount;
} tsrq_tuple;

tsrq_tuple * srq_tuple_create(const char *name, const char *value);
void srq_tuple_free(tsrq_tuple *tuple);

int srq_tuple_add_value(tsrq_tuple *tuple, const char *value);

#endif /* __SRQ_TUPLE_H__ */
