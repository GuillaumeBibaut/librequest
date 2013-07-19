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

int srq_mfd_readfile(tsrq_file *file, const char *bound, size_t maxfilesize);
int srq_mfd_readfield(tsrq_tuple *tuple, const char *bound);
int srq_multipart(int state, char *bound, tsrq_request *request, tsrq_tuple *tuple);


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
    request->_FILES.maxfilesize = maxfilesize;

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
                    c = *ptr;
                    if (c == '\0') {
                        end = true;
                    }
                    ptr++;
                    break;
            }

            if (method == PUT) {
                *buf = c;
                buf++;

            } else {
                if (end || c == '&') {
                    if (tuple != NULL) {
                        curvalue[index] = '\0';
                        if (srq_tuple_add_value(tuple, curvalue) != 0) {
                            return(4);
                        }
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
enum { MFD_NONE, MFD_NEWFILE, MFD_READFILE, MFD_NEWFIELD, MFD_READFIELD, MFD_INNERBOUND };
static int srq_readmfd(tsrq_request *request, size_t maxfilesize) {
    char *content_type, *bound;
    
    content_type = getenv("CONTENT_TYPE");
    bound = content_type + strlen(MFD_STRING);

    if (srq_multipart(MFD_NONE, bound, request, NULL) != 0) {
        return(1);
    }

    return(0);
}


/*
 *
 */
int srq_multipart(int state, char *bound, tsrq_request *request, tsrq_tuple *tuple) {
    char buffer[MFD_CHUNKSIZE + 1], innerbound[256 + 1];;
    char *tok1, *brm, *ptr, *ptr2;
    int cmp1;
    size_t linesz, boundsz;
    char *eol;
    tsrq_file *file;
    
    file = NULL;
    boundsz = strlen(bound);
    while (!feof(stdin) && fgets(buffer, MFD_CHUNKSIZE, stdin) != NULL) {
        linesz = strlen(buffer);
        if (linesz > 2 && buffer[0] == '-' && buffer[1] == '-') {
            if ((eol = strstr(buffer, MFD_NEWLINE)) != NULL) {
                *eol = '\0';
            }
            /* boundary : check if starting boundary or end */
            ptr = buffer + 2;
            if (strncmp(ptr, bound, boundsz) != 0) {
                return(20);
            }
            ptr += boundsz;
            if (*ptr != '\0' && *ptr == '-' && *(ptr + 1) == '-') {
                break;
            }
        } else if (strcasestr(buffer, "content-disposition:") == buffer) {
            if ((eol = strstr(buffer, MFD_NEWLINE)) != NULL) {
                *eol = '\0';
            }
            /* new field/file */
            ptr = buffer + strlen("content-disposition:");
            while (isspace(*ptr))
                ptr++;
            brm = NULL;
            if ((tok1 = strtok_r(ptr, ";", &brm)) == NULL) {
                /* malformed */
                return(1);
            }
            if ((cmp1 = strcasecmp(tok1, "form-data")) == 0
                || strcasecmp(tok1, "attachment") == 0) {
                if (cmp1 == 0) {
                    if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                        /* malformed */
                        return(2);
                    }
                    while (isspace(*tok1))
                        tok1++;
                    if (strcasestr(tok1, "name=") == NULL) {
                        /* malformed */
                        return(3);
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
                        return(4);
                    }
                    state = MFD_NEWFIELD;
                }
                if ((tok1 = strtok_r(NULL, ";", &brm)) != NULL) {
                    while (isspace(*tok1))
                        tok1++;
                    if (strcasestr(tok1, "filename=") == NULL) {
                        /* malformed */
                        return(5);
                    }
                    tok1 += strlen("filename=");
                    if (*tok1 == '"') {
                        ptr2 = ++tok1;
                        while (*ptr2 != '"')
                            ptr2++;
                        *ptr2 = '\0';
                    }
                    if (*tok1 == '\0') {
                        state = MFD_NONE;
                    } else {
                        if ((file = srq_files_add(&(request->_FILES), tok1)) == NULL) {
                            /* memory exception or already a file with the same name */
                            return(6);
                        }
                        if (srq_tuple_add_value(tuple, tok1) != 0) {
                            /* memory exception */
                            return(7);
                        }
                        state = MFD_NEWFILE;
                    }
                }
            } else {
                /* what ?? malformed */
                return(8);
            }
        } else if (strcasestr(buffer, "content-type:") == buffer) {
            if ((eol = strstr(buffer, MFD_NEWLINE)) != NULL) {
                *eol = '\0';
            }
            /* can be a file, or a new boundary (recursive ?) */
            ptr = buffer + strlen("content-type:");
            while (isspace(*ptr))
                ptr++;
            if (strcasecmp(ptr, "multipart/mixed") == 0) {
                brm = NULL;
                if ((tok1 = strtok_r(ptr, ";", &brm)) == NULL) {
                    /* malformed */
                    return(9);
                }
                if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                    /* malformed */
                    return(10);
                }
                while (isspace(*tok1))
                    tok1++;
                if (strcasestr(tok1, "boundary=") == NULL) {
                    /* malformed */
                    return(11);
                }
                tok1 += strlen("boundary=");
                strcpy(innerbound, tok1);
                state = MFD_INNERBOUND;
            } else if (file) {
                
                file->content_type = strdup(ptr);
            }
        } else if (strcasestr(buffer, "content-transfer-encoding:") == buffer) {
            
            /* TODO another property on a file, same readfile method */
            
        } else if (strstr(buffer, MFD_NEWLINE) == buffer) {
            state = (state == MFD_NEWFIELD ? MFD_READFIELD :
                     (state == MFD_NEWFILE ? MFD_READFILE :
                      (state == MFD_INNERBOUND ? state : MFD_NONE)));
            if (state == MFD_READFILE) {
                if (srq_mfd_readfile(file, bound, request->_FILES.maxfilesize) != 0) {
                    /* couldn't read file, malformed */
                    return(12);
                }
            } else if (state == MFD_READFIELD) {
                if (srq_mfd_readfield(tuple, bound) != 0) {
                    /* couldn't read field, malformed */
                    return(13);
                }
            } else if (state == MFD_INNERBOUND) {
                if (srq_multipart(state, innerbound, request, tuple) != 0) {
                    /* something was malformed */
                    return(14);
                }
            }
        }
    }
    return(0);
}


/*
 *
 */
int srq_mfd_readfile(tsrq_file *file, const char *bound, size_t maxfilesize) {
    size_t capacity;
    size_t size, pos;
    char c, *data;
    char bstart[256 + 1], bend[256 + 1];
    size_t bstartsz, bendsz, nlsz;
    bool endfile, max, found;
    
    snprintf(bstart, 256, "%s--%s", MFD_NEWLINE, bound);
    bstartsz = strlen(bstart);
    snprintf(bend, 256, "%s--%s--", MFD_NEWLINE, bound);
    bendsz = strlen(bend);
    nlsz = strlen(MFD_NEWLINE);
    size = 0;
    capacity = 0;
    endfile = false;
    max = false;
    found = false;
    while (!endfile && !feof(stdin)) {
        c = (char)fgetc(stdin);
        if (size >= capacity) {
            file->data = realloc(file->data, sizeof(char) * (capacity + MFD_CHUNKSIZE));
            if (file->data == NULL) {
                return(1);
            }
            capacity += MFD_CHUNKSIZE;
        }
        file->data[size] = c;
        size++;
        if (size > bstartsz && strncmp(file->data + (size - bstartsz), bstart, bstartsz) == 0) {
            endfile = true;
            file->length = size - bstartsz;
        } else if (size > bendsz && strncmp(file->data + (size - bendsz), bend, bendsz) == 0) {
            endfile = true;
            file->length = size - bendsz;
        }
        if (size > maxfilesize) {
            /* maxfilesize can be set through srq_request_parse function */
            max = true;
            data = file->data;
            file->data = NULL;
            capacity = 0;
            /* Let's trying to keep moving into the stream */
            pos = size - (nlsz + 2);
            while (pos >= (size - MFD_CHUNKSIZE)) {
                if (strnstr(data, MFD_NEWLINE, nlsz) == data
                    && strnstr(data + nlsz, "--", 2) == (data + nlsz)) {
                    /* something looks like a boundary, try to keep that part */
                    found = true;
                    file->data = realloc(file->data, sizeof(char) * MFD_CHUNKSIZE);
                    if (file->data == NULL) {
                        file->data = data;
                        return(2);
                    }
                    memcpy(file->data, (data + pos + nlsz), (size - pos - nlsz));
                    capacity = MFD_CHUNKSIZE;
                    size -= (pos + nlsz);
                    free(data);
                    break;
                }
                pos--;
            }
            if (!found) {
                /* nothing like a boundary, reset */
                capacity = 0;
                size = 0;
                free(data);
            }
            found = false;
        }
    }
    if (max) {
        /* file size was too big */
        free(file->data);
        file->data = NULL;
        file->length = 0;
    }
    if (feof(stdin) && !endfile) {
        /* no end of bound found, malformed */
        return(3);
    } else if (!feof(stdin)) {
        fgets(bstart, 256, stdin);
    }
    return(0);
}


/*
 *
 */
int srq_mfd_readfield(tsrq_tuple *tuple, const char *bound) {
    char buffer[MFD_CHUNKSIZE + 1];
    bool endfield, add;
    char bstart[256 + 1], bend[256 + 1];
    char *eol;
    
    snprintf(bstart, 256, "--%s", bound);
    snprintf(bend, 256, "--%s--", bound);
    endfield = false;
    add = true;
    while (!endfield && !feof(stdin) && fgets(buffer, MFD_CHUNKSIZE, stdin) != NULL) {
        if (strstr(buffer, bstart) == buffer) {
            endfield = true;
        } else if (strstr(buffer, bend) == buffer) {
            endfield = true;
        } else {
            if (add) {
                if (srq_tuple_add_value(tuple, buffer) != 0) {
                    /* memory exception */
                    return(1);
                }
                add = false;
            } else {
                if (srq_tuple_join_value(tuple, buffer) != 0) {
                    /* memory exception */
                    return(2);
                }
            }
        }
    }
    if (feof(stdin) && !endfield) {
        /* no end of bound found, malformed */
        return(3);
    }
    if ((eol = strrchr(tuple->values[tuple->valuescount - 1], '\r')) != NULL) {
        *eol = '\0';
    }
    
    return(0);
}
