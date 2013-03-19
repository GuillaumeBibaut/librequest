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
#ifndef __REQUEST_H__
#define __REQUEST_H__


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

/* Default max file size set to 8MB */
#define SRQ_MAXFILESIZE (8 * 1024 * 1024)
tsrq_request * srq_request_parse(size_t maxfilesize);

void srq_request_free(tsrq_request *request);

tsrq_tuple * srq_tuple_find(const char *name, tsrq_tuple **tuples, size_t tuplescount);

#endif /* __REQUEST_H__ */

