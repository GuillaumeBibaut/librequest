#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srq-tuple.h"


tsrq_tuple * srq_tuple_create(const char *name, const char *value) {
    tsrq_tuple *tuple;
    
    if (name == NULL || *name == '\0') {
        return(NULL);
    }
    tuple = calloc(1, sizeof(tsrq_tuple));
    if (tuple == NULL) {
        return(NULL);
    }
    tuple->name = strdup(name);
    if (value != NULL) {
        if (srq_tuple_add_value(tuple, value) != 0) {
            srq_tuple_free(tuple);
            return(NULL);
        }
    }
    return(tuple);
}


void srq_tuple_free(tsrq_tuple *tuple) {
    size_t index;
    
    if (tuple == NULL) {
        return;
    }
    for (index = 0; index < tuple->valuescount; index++) {
        free(tuple->values[index]);
    }
    if (tuple->values != NULL) {
        free(tuple->values);
    }
    free(tuple->name);
    free(tuple);
}


int srq_tuple_add_value(tsrq_tuple *tuple, const char *value) {

    if (value != NULL) {
        tuple->values = realloc(tuple->values, sizeof(char *) * (tuple->valuescount + 1));
        if (tuple->values == NULL) {
            return(1);
        }
        tuple->values[tuple->valuescount] = strdup(value);
        tuple->valuescount++;
    }
    return(0);
}

