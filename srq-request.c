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
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>

#include "srq-request.h"

#define MFD_STRING "multipart/form-data; boundary="
#define MFD_CD_STRING "Content-Disposition: form-data; "
#define MFD_CT_STRING "Content-Type: "
#define MFD_NEWLINE "\r\n"
#define MFD_CHUNKSIZE (32 * 1024)

#define FNAMESZ MFD_CHUNKSIZE
#define FVALUESZ MFD_CHUNKSIZE

#define FMAX_COUNT (DEFAULT_POOLSZ * 1024)

#define unhex(c) (((c) >= '0' && (c) <= '9') ? (c) - '0' : \
        ((c) >= 'a' && (c) <= 'f') ? (c) - 'a' + 10 : \
        ((c) >= 'A' && (c) <= 'F') ? (c) - 'A' + 10 : 0)


enum request_method {POST, GET, PUT};

static int srq_readform(enum request_method method, void *_METHOD);
static int srq_readmfd(tsrq_request *request, size_t maxfilesize);


/*
 *
 */
tsrq_request * srq_request_get(void) {
    return(srq_request_parse(SRQ_MAXFILESIZE));
}


/*
 *
 */
tsrq_request * srq_request_parse(size_t maxfilesize) {
    tsrq_request *request;
    char *ptr;
    int res;

    request = (tsrq_request *)calloc(1, sizeof(tsrq_request));
    if (request == NULL) {
#if defined(DEBUG)
        fprintf(stderr, "%s: %s, at line %d\n", __func__, strerror(errno), __LINE__);
#endif
        return(NULL);
    }

    if ((ptr = (char *)getenv("REQUEST_METHOD")) == NULL) {
#if defined(DEBUG)
        fprintf(stderr, "%s: env. REQUEST_METHOD not set, at line %d\n", __func__, __LINE__);
#endif
        srq_request_free(request);
        return(NULL);
    }

    if (getenv("QUERY_STRING") != NULL) {
        /* All Methods */
        if ((res = srq_readform(GET, &(request->_GET))) != 0) {
            srq_request_free(request);
            return(NULL);
        }
    }

    if (strcasecmp(ptr, "POST") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
#if defined(DEBUG)
            fprintf(stderr, "%s: env. CONTENT_LENGTH not set, at line %d\n", __func__, __LINE__);
#endif
            srq_request_free(request);
            return(NULL);
        }
        if ((ptr = getenv("CONTENT_TYPE")) == NULL) {
#if defined(DEBUG)
            fprintf(stderr, "%s: env. CONTENT_TYPE not set, at line %d\n", __func__, __LINE__);
#endif
            srq_request_free(request);
            return(NULL);
        }
        if (strcasestr(ptr, MFD_STRING) == NULL) {
            if ((res = srq_readform(POST, &(request->_POST))) != 0) {
                srq_request_free(request);
                return(NULL);
            }
        } else {
            if ((res = srq_readmfd(request, maxfilesize)) != 0) {
                srq_request_free(request);
                return(NULL);
            }
        }
        
    } else if (strcasecmp(ptr, "PUT") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
#if defined(DEBUG)
            fprintf(stderr, "%s: env. CONTENT_LENGTH not set, at line %d\n", __func__, __LINE__);
#endif
            srq_request_free(request);
            return(NULL);
        }
        if ((res = srq_readform(PUT, &(request->_PUT))) != 0) {
            srq_request_free(request);
            return(NULL);
        }
    }
    return(request);
}


/*
 *
 */
void srq_request_free(tsrq_request *request) {
    int index;

    if (request == NULL) {
        return;
    }

    srq_tuples_free(&(request->_GET));
    srq_tuples_free(&(request->_POST));
    if (request->_FILES.files) {
        for (index = 0; index < request->_FILES.count; index++) {
            srq_file_free(request->_FILES.files[index]);
        }
    }
    if (request->_PUT.buffer) {
        free(request->_PUT.buffer);
    }

    free(request);
}


/*
 *
 */
static int srq_readform(enum request_method method, void *_METHOD) {
    enum { FIELDNAME, FIELDVALUE, HEXCHAR };

    char *ptr, c, curvalue[FVALUESZ + 1], curname[FNAMESZ + 1], *buf;
    int index = 0, hexcnt = 0, hexc = 0;
    size_t size;
    int current, ostate = 0;
    bool end = false;
    tsrq_tuple *tuple;
    tsrq_tuples *tuples;
    tsrq_put *putbuf;

    switch (method) {
        case POST:
        case PUT:
            if ((ptr = (char *)getenv("CONTENT_LENGTH")) == NULL) {
                return(1);
            }
            size = atoi(ptr);
            break;
        case GET:
        default:
            ptr = (char *)getenv("QUERY_STRING");
            size = strlen(ptr);
            break;
    }

    switch (method) {
        case PUT:
            putbuf = (tsrq_put *)_METHOD;
            putbuf->buffer = calloc(size + 1, sizeof(char));
            if (putbuf->buffer == NULL) {
                return(2);
            }
            putbuf->length = size;
            buf = putbuf->buffer;
            break;
        case GET:
        case POST:
        default:
            tuples = (tsrq_tuples *)_METHOD;
            break;
    }
    
    current = FIELDNAME;
    tuple = NULL;
    c = '\0';
    do {
        if (hexcnt == 2) {
            hexcnt = 0;
            switch (current) {
                case FIELDNAME:
                    if (index < FNAMESZ) {
                        curname[index++] = c;
                    }
                    break;
                case FIELDVALUE:
                    if (index < FVALUESZ) {
                        curvalue[index++] = c;
                    }
                    break;
            }
            
        } else {
            switch (method) {
                case PUT:
                case POST:
                    c = getc(stdin);
                    if (feof(stdin)) {
                        end = true;
                        c = '\0';
                    }
                    break;
                case GET:
                    c = *(ptr++);
                    if (c == '\0') {
                        end = true;
                    }
                    break;
            }

            if (method == PUT) {
                *buf = c;
                buf++;

            } else {
                if (end || c == '&') {
                    curvalue[index] = '\0';
                    if (srq_tuple_add_value(tuple, curvalue) != 0) {
                        return(4);
                    }

                    index = 0;
                    current = FIELDNAME;
                    tuple = NULL;

                } else if (c == '=') {
                    curname[index] = '\0';
                    if ((tuple = srq_tuples_add(tuples, curname, NULL)) == NULL) {
                        return(3);
                    }

                    index = 0;
                    current = FIELDVALUE;

                } else if (c == '%') {
                    ostate = current;
                    hexcnt = 0;
                    current = HEXCHAR;

                } else {
                    if (c == '+') {
                        c = ' ';
                    }
                    if (hexcnt == 2) {
                        hexcnt = 0;
                    }
                    switch (current) {
                        case FIELDNAME:
                            if (index < FNAMESZ) {
                                curname[index++] = c;
                            }
                            break;
                        case FIELDVALUE:
                            if (index < FVALUESZ) {
                                curvalue[index++] = c;
                            }
                            break;
                        case HEXCHAR:
                            hexcnt++;
                            if (hexcnt == 1) {
                                hexc = unhex(c) << 4;
                            } else if (hexcnt == 2) {
                                hexc |= unhex(c);
                                c = (char)hexc;
                                current = ostate;
                            }
                            break;
                    }
                }
            }
        }
    } while (!end);

    return(0);
}


/*
 *
 */
static int srq_readmfd(tsrq_request *request, size_t maxfilesize) {
    enum { NONE, NEWFILE, READFILE, NEWFIELD, READFIELD };

    char *ptr;
    char buffer[MFD_CHUNKSIZE + 1], *brm, *tok1, *ptr2, *ptr3;
    int state = NONE, oldstate = NONE;
    bool end, endfile;
    char *content_type;
    char *boundary, *bound_start, *bound_startrn, *bound_end;
    char *file_ct;
    size_t file_len, chunk_len;
    size_t end_len, start_len;
    tsrq_tuple *tuple;
    tsrq_file *file;

    char *bound, *innerbound;
    size_t boundsz, linesz;
    char *eol;
    int cmp1, cmp2;
    
    content_type = getenv("CONTENT_TYPE");
    bound = content_type + strlen(MFD_STRING);
    boundsz = strlen(bound);
    
    state = NONE;
    while (fgets(buffer, MFD_CHUNKSIZE, stdin) != NULL) {
        if ((eol = strstr(buffer, MFD_NEWLINE)) != NULL) {
            *eol = '\0';
        }
        linesz = strlen(buffer);
        if (linesz > 2 && buffer[0] == '-' && buffer[1] == '-') {
            /* boundary : check if starting boundary or end */
            ptr = buffer;
            ptr += 2 + boundsz;
            if (*ptr != '\0' && *ptr == '-' && *(ptr + 1) == '-') {
                break;
            }
        } else if (strcasestr(buffer, "content-disposition:") == buffer) {
            /* new field/file */
            ptr = buffer + strlen("content-disposition:");
            while (isspace(*ptr))
                ptr++;
            brm = NULL;
            if ((tok1 = strtok_r(ptr, ";", &brm)) == NULL) {
                /* malformed */
            }
            cmp1 = -1;
            cmp2 = -1;
            if ((cmp1 = strcasecmp(tok1, "form-data")) == 0
                || (cmp2 = strcasecmp(tok1, "attachment")) == 0) {
                if (cmp1 == 0) {
                    if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                        /* malformed */
                    }
                    while (isspace(*tok1))
                        tok1++;
                    if (strcasestr(tok1, "name=") == NULL) {
                        /* malformed */
                    }
                    tok1 += strlen("name=");
                    if (*tok1 == '"') {
                        ptr2 = ++tok1;
                        while (*ptr2 != '"')
                            ptr2++;
                        *ptr2 = '\0';
                    }
                    if ((tuple = srq_tuples_add(&(request->_POST), tok1, NULL)) == NULL) {
                        /* memory exception */
                    }
                    state = NEWFIELD;
                }
                if ((tok1 = strtok_r(NULL, ";", &brm)) != NULL) {
                    while (isspace(*tok1))
                        tok1++;
                    if (strcasestr(tok1, "filename=") == NULL) {
                        /* malformed */
                    }
                    if (*tok1 == '"') {
                        ptr2 = ++tok1;
                        while (*ptr2 != '"')
                            ptr2++;
                        *ptr2 = '\0';
                    }
                    if ((file = srq_files_add(&(request->_FILES), tok1)) == NULL) {
                        /* memory exception or already a file with the same name */
                    }
                    if (srq_tuple_add_value(tuple, tok1) != 0) {
                        /* memory exception */
                    }
                    state = NEWFILE;
                }
            } else {
                /* what ?? malformed */
            }
        } else if (strcasestr(buffer, "content-type:") == buffer) {
            /* can be a file, or a new boundary (recursive ?) */
            ptr = buffer + strlen("content-type:");
            while (isspace(*ptr))
                ptr++;
            if (strcasecmp(ptr, "multipart/mixed") == 0) {
                brm = NULL;
                if ((tok1 = strtok_r(ptr, ";", &brm)) == NULL) {
                    /* malformed */
                }
                if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                    /* malformed */
                }
                while (isspace(*tok1))
                    tok1++;
                if (strcasestr(tok1, "boundary=") == NULL) {
                    /* malformed */
                }
                tok1 += strlen("boundary=");
            }
        } else if (strcasestr(buffer, "content-transfer-encoding:") == buffer) {
            /* another property on a file, useful ? same readfile method */
        } else if (strcasestr(buffer, MFD_NEWLINE) == buffer) {
            state = (state == NEWFIELD ? READFIELD : (state == NEWFILE ? READFILE : NONE));
        } else {
            if (state == READFIELD) {
                if (srq_readfile()) {
                }
            }
        }
    }
    
    
    
    
//    if (asprintf(&bound_start, "--%s", boundary) == -1) {
//        return(1);
//    }
//    if (asprintf(&bound_startrn, "--%s\r\n", boundary) == -1) {
//        free(bound_start);
//        return(2);
//    }
//    if (asprintf(&bound_end, "--%s--", boundary) == -1) {
//        free(bound_start);
//        free(bound_startrn);
//        return(3);
//    }
//    start_len = strlen(bound_startrn);
//    end_len = strlen(bound_end);
//
//    end = false;
//    do {
//        switch (state) {
//            case NONE:
//            case NEWPAIR:
//            case NEWFILE:
//                if (feof(stdin)) {
//                    end = true;
//                } else {
//                    fgets(buffer, sizeof(buffer), stdin);
//                    if ((ptr = strcasestr(buffer, MFD_NEWLINE)) == buffer) {
//                        oldstate = state;
//                        state = (state == NEWPAIR ? READPAIR : (state == NEWFILE ? READFILE : NONE));
//
//                    } else if (ptr == NULL) {
//                        free(bound_start);
//                        free(bound_end);
//                        free(bound_startrn);
//                        return(3);
//
//                    } else {
//                        *ptr = '\0';
//                        if (strcmp(buffer, bound_end) == 0) {
//                            end = true;
//
//                        } else if (strcmp(buffer, bound_start) == 0) {
//                            oldstate = state;
//                            state = NEWPAIR;
//
//                        } else if (strcasestr(buffer, MFD_CD_STRING) == buffer) {
//                            ptr = buffer + strlen(MFD_CD_STRING);
//                            if ((tok1 = strtok_r(ptr, ";", &brm)) != NULL) {
//                                if (strcasestr(tok1, "name=") != tok1) {
//                                    free(bound_start);
//                                    free(bound_end);
//                                    free(bound_startrn);
//                                    return(4);
//                                }
//                                ptr3 = NULL;
//                                ptr2 = tok1 + strlen("name=");
//                                if (*ptr2 == '"') {
//                                    ptr3 = ++ptr2;
//                                    while (*ptr3 != '"') {
//                                        ptr3++;
//                                    }
//                                    *ptr3 = '\0';
//                                }
//                                if ((tuple = srq_tuples_add(&(request->_POST), ptr2, NULL)) == NULL) {
//                                    free(bound_start);
//                                    free(bound_end);
//                                    free(bound_startrn);
//                                    return(42);
//                                }
//
//                                if (ptr3) {
//                                    *ptr3 = '"';
//                                }
//                                if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
//                                    oldstate = state;
//                                    state = NEWPAIR;
//
//                                } else {
//                                    if (*tok1 == ' ') {
//                                        tok1++;
//                                    }
//                                    if (strcasestr(tok1, "filename=") != tok1) {
//                                        free(bound_start);
//                                        free(bound_end);
//                                        free(bound_startrn);
//                                        return(5);
//                                    }
//                                    ptr3 = NULL;
//                                    ptr2 = tok1 + strlen("filename=");
//                                    if (*ptr2 == '"') {
//                                        ptr3 = ++ptr2;
//                                        while (*ptr3 != '"') {
//                                            ptr3++;
//                                        }
//                                        *ptr3 = '\0';
//                                    }
//                                    if (srq_tuple_add_value(tuple, ptr2) != 0) {
//                                        free(bound_start);
//                                        free(bound_end);
//                                        free(bound_startrn);
//                                        return(43);
//                                    }
//                                    if (ptr3) {
//                                        *ptr3 = '"';
//                                    }
//                                    if (*ptr2 != '\0') {
//                                        request->_FILES.files = (tsrq_file *)realloc(request->_FILES.files, sizeof(tsrq_file) * (request->_FILES.count + 1));
//                                        memset((request->_FILES.files + request->_FILES.count), 0, sizeof(tsrq_file));
//                                        request->_FILES.files[request->_FILES.count].filename = strdup(ptr2);
//                                    }
//                                    oldstate = state;
//                                    state = NEWFILE;
//                                }
//                            }
//
//                        } else if (strcasestr(buffer, MFD_CT_STRING) == buffer) {
//                            ptr = buffer + strlen(MFD_CT_STRING);
//                            if (*(tuple->values[tuple->valuescount - 1]) != '\0') {
//                                request->_FILES.files[request->_FILES.count].content_type = strdup(ptr);
//                            }
//
//                        } else {
//                            free(bound_start);
//                            free(bound_end);
//                            free(bound_startrn);
//                            return(6);
//                        }
//                    }
//                }
//                break;
//
//            case READPAIR:
//                fgets(buffer, sizeof(buffer), stdin);
//                if (feof(stdin)) {
//                    end = true;
//                }
//                if ((ptr = strcasestr(buffer, MFD_NEWLINE)) == NULL) {
//                    free(bound_start);
//                    free(bound_end);
//                    free(bound_startrn);
//                    return(7);
//
//                } else {
//                    *ptr = '\0';
//                    if (strcmp(buffer, bound_end) == 0) {
//                        end = true;
//
//                    } else if (strcmp(buffer, bound_start) == 0) {
//                        oldstate = state;
//                        state = NEWPAIR;
//
//                    } else {
//                        if (tuple) {
//                            if (tuple->valuescount == 0) {
//                                if (srq_tuple_add_value(tuple, buffer) != 0) {
//                                    free(bound_start);
//                                    free(bound_end);
//                                    free(bound_startrn);
//                                    return(44);
//                                }
//                            } else {
//                                if (oldstate == NEWPAIR) {
//                                    if (srq_tuple_add_value(tuple, buffer) != 0) {
//                                        free(bound_start);
//                                        free(bound_end);
//                                        free(bound_startrn);
//                                        return(45);
//                                    }
//                                } else {
//                                    tuple->values[tuple->valuescount - 1] = realloc(tuple->values[tuple->valuescount - 1],
//                                            strlen(tuple->values[tuple->valuescount - 1]) + 1 + strlen(buffer) + 1);
//                                    if (tuple->values[tuple->valuescount - 1] == NULL) {
//                                        free(bound_start);
//                                        free(bound_end);
//                                        free(bound_startrn);
//                                        return(46);
//                                    }
//                                    strcat(tuple->values[tuple->valuescount - 1], "\n");
//                                    strcat(tuple->values[tuple->valuescount - 1], buffer);
//                                }
//                            }
//                        }
//                    }
//                }
//                break;
//                
//            case READFILE:
//                file_len = 0;
//                file_ct = (char *)malloc(MFD_CHUNKSIZE);
//                memset(file_ct, 0, MFD_CHUNKSIZE);
//                chunk_len = MFD_CHUNKSIZE;
//                endfile = false;
//                do {
//                    if (file_len >= chunk_len) {
//                        file_ct = (char *)realloc(file_ct, chunk_len + MFD_CHUNKSIZE);
//                        chunk_len += MFD_CHUNKSIZE;
//                    }
//                    file_ct[file_len] = (char)fgetc(stdin);
//                    file_len++;
//                    if (feof(stdin)) {
//                        endfile = true;
//                        end = true;
//                        file_len--;
//                    }
//                    if (file_len >= end_len && strcmp((file_ct + file_len - end_len), bound_end) == 0) {
//                        if (tuple->values[tuple->valuescount - 1] == '\0') {
//                            free(file_ct);
//
//                        } else {
//                            request->_FILES.files[request->_FILES.count].length = (file_len - end_len);
//                            if (request->_FILES.files[request->_FILES.count].length > 2) {
//                                request->_FILES.files[request->_FILES.count].length -= 2;
//                            }
//                            if (request->_FILES.files[request->_FILES.count].length > maxfilesize) {
//                                free(request->_FILES.files[request->_FILES.count].data);
//                                request->_FILES.files[request->_FILES.count].data = NULL;
//                                request->_FILES.files[request->_FILES.count].length = -1;
//
//                            } else {
//                                file_ct = (char *)realloc(file_ct, request->_FILES.files[request->_FILES.count].length);
//                                request->_FILES.files[request->_FILES.count].data = file_ct;
//                            }
//                            request->_FILES.count++;
//                        }
//                        endfile = true;
//                        end = true;
//                    } else if (file_len >= start_len && strcmp((file_ct + file_len - start_len), bound_startrn) == 0) {
//                        if (tuple->values[tuple->valuescount - 1] == '\0') {
//                            free(file_ct);
//
//                        } else {
//                            request->_FILES.files[request->_FILES.count].length = (file_len - start_len);
//                            if (request->_FILES.files[request->_FILES.count].length > 2) {
//                                request->_FILES.files[request->_FILES.count].length -= 2;
//                            }
//                            if (request->_FILES.files[request->_FILES.count].length > maxfilesize) {
//                                free(request->_FILES.files[request->_FILES.count].data);
//                                request->_FILES.files[request->_FILES.count].data = NULL;
//                                request->_FILES.files[request->_FILES.count].length = -1;
//
//                            } else {
//                                file_ct = (char *)realloc(file_ct, request->_FILES.files[request->_FILES.count].length);
//                                request->_FILES.files[request->_FILES.count].data = file_ct;
//                            }
//                            request->_FILES.count++;
//                        }
//                        endfile = true;
//                        oldstate = state;
//                        state = NEWPAIR;
//                    }
//                } while (!endfile);
//                break;
//        }
//    } while (!end);
//    free(bound_start);
//    free(bound_end);
//    free(bound_startrn);

    return(0);
}

