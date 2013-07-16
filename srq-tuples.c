#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srq-defs.h"
#include "srq-tuples.h"


/*
 *
 */
tsrq_tuple *srq_tuples_add(tsrq_tuples *tuples, const char *name, const char *value) {
    tsrq_tuple *tuple;

    if ((tuple = srq_tuples_find(*tuples, name)) != NULL) {
        if (srq_tuple_add_value(tuple, value) != 0) {
            return(NULL);
        }
    } else {
        if (tuples->count >= tuples->poolsz) {
            tuples->params = realloc(tuples->params, sizeof(tsrq_tuple *) * (tuples->poolsz + DEFAULT_POOLSZ));
            tuples->poolsz += DEFAULT_POOLSZ;
        }
        if ((tuple = srq_tuple_create(name, value)) == NULL) {
            return(NULL);
        }
        tuples->params[tuples->count] = tuple;
        tuples->count++;
    }
    return(tuple);
}


/*
 *
 */
tsrq_tuple * srq_tuples_find(tsrq_tuples tuples, const char *name) {
    size_t index;

    if (name == NULL || *name == '\0') {
        return((tsrq_tuple *)NULL);
    }
    if (tuples.count == 0 || tuples.params == NULL) {
        return((tsrq_tuple *)NULL);
    }
    for (index = 0; index < tuples.count; index++) {
        if (strcasecmp(tuples.params[index]->name, name) == 0) {
            return(tuples.params[index]);
        }
    }
    return((tsrq_tuple *)NULL);
}


/*
 *
 */
void srq_tuples_free(tsrq_tuples *tuples) {
    size_t index;

    if (tuples == NULL) {
        return;
    }
    for (index = 0; index < tuples->count; index++) {
        srq_tuple_free(tuples->params[index]);
    }
    free(tuples->params);
}
