#ifndef __SRQ_TUPLES_H__
#define __SRQ_TUPLES_H__

#include "srq-tuple.h"

typedef struct srq_tuples {
    tsrq_tuple **params;
    size_t count;
    size_t poolsz;
} tsrq_tuples;

void srq_tuples_free(tsrq_tuples *tuples);

tsrq_tuple *srq_tuples_add(tsrq_tuples *tuples, const char *name, const char *value);

tsrq_tuple * srq_tuples_find(tsrq_tuples tuples, const char *name);

#endif /* __SRQ_TUPLES_H__ */
