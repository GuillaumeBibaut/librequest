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
