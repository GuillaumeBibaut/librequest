/*-
 * Copyright (c) 2013, Guillaume Bibaut.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of Guillaume Bibaut nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL Guillaume Bibaut BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
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


int srq_tuple_join_value(tsrq_tuple *tuple, const char *value) {
    size_t valuesz;
    size_t currentsz;
    
    if (value != NULL) {
        if (tuple->values == NULL) {
            tuple->values = realloc(tuple->values, sizeof(char *));
            if (tuple->values == NULL) {
                return(1);
            }
            tuple->valuescount = 1;
            tuple->values[0] = NULL;
        }
        currentsz = 0;
        if (tuple->values[0] != NULL) {
            currentsz = strlen(tuple->values[0]);
        }
        valuesz = strlen(value);
        tuple->values[0] = realloc(tuple->values[0], currentsz + valuesz + 1);
        if (tuple->values[0] == NULL) {
            return(2);
        }
        memcpy(tuple->values[0] + currentsz, value, valuesz);
        tuple->values[0][currentsz + valuesz] = '\0';
    }
    return(0);
}
