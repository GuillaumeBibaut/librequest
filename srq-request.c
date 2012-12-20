#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdbool.h>

#include "srq-request.h"

#define MFD_STRING "multipart/form-data; boundary="
#define MFD_CD_STRING "Content-Disposition: form-data; "
#define MFD_CT_STRING "Content-Type: "
#define MFD_NEWLINE "\r\n"
#define MFD_CHUNKSIZE (32 * 1024)


#define FNAMESZ MFD_CHUNKSIZE
#define FVALUESZ MFD_CHUNKSIZE

#define PAIRS_POOLSZ 4

#define FMAX_COUNT (PAIRS_POOLSZ * 1024)


#define unhex(c) (((c) >= '0' && (c) <= '9') ? (c) - '0' : \
        ((c) >= 'a' && (c) <= 'f') ? (c) - 'a' + 10 : \
        ((c) >= 'A' && (c) <= 'F') ? (c) - 'A' + 10 : 0)

enum request_method {POST, GET, PUT};

static int srq_readform(enum request_method method, void ***_METHOD, size_t *methodcount);

static int srq_readmfd(tsrq_request *request, size_t maxfilesize);

static void srq_pairs_free(tsrq_tuple ***tuples, size_t tuplescount);


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
    char *ptr, *_PUT = NULL;
    int res = 0;
    tsrq_tuple **_GET = NULL, **_POST = NULL;
    size_t getcount = 0, postcount = 0, putcount = 0;
    tsrq_request *request;

    request = (tsrq_request *)malloc(sizeof(tsrq_request));
    if (request == NULL) {
        return((tsrq_request *)NULL);
    }
    memset(request, 0, sizeof(tsrq_request));

    if ((ptr = (char *)getenv("REQUEST_METHOD")) == NULL) {
        return(request);
    }

    if (getenv("QUERY_STRING") != NULL) {
        /* All Methods */
        if ((res = srq_readform(GET, (void ***)&_GET, &getcount)) != 0) {
            return(request);
        }
        request->_GET = _GET;
        request->getcount = getcount;
    }

    if (strcasecmp(ptr, "POST") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
            return(request);
        }
        if ((ptr = getenv("CONTENT_TYPE")) == NULL) {
            return(request);
        }
        if (strcasestr(ptr, MFD_STRING) == NULL) {
            if ((res = srq_readform(POST, (void ***)&_POST, &postcount)) != 0) {
                return(request);
            }
            request->_POST = _POST;
            request->postcount = postcount;
            
        } else {
            if ((res = srq_readmfd(request, maxfilesize)) != 0) {
                return(request);
            }
        }
        
    } else if (strcasecmp(ptr, "PUT") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
            return(request);
        }
        if ((res = srq_readform(PUT, (void ***)&_PUT, &putcount)) != 0) {
            return(request);
        }
        request->_PUT = _PUT;
        request->putcount = putcount;
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

    srq_pairs_free(&(request->_GET), request->getcount);
    srq_pairs_free(&(request->_POST), request->postcount);
    if (request->_FILES) {
        for (index = 0; index < request->filescount; index++) {
            if (request->_FILES[index].filename) {
                free(request->_FILES[index].filename);
            }
            if (request->_FILES[index].content_type) {
                free(request->_FILES[index].content_type);
            }
            if (request->_FILES[index].data) {
                free(request->_FILES[index].data);
            }
        }
        free(request->_FILES);
        request->_FILES = NULL;
    }
    if (request->_PUT) {
        free(request->_PUT);
        request->_PUT = NULL;
    }
    free(request);
    request = NULL;
}


/*
 *
 */
tsrq_tuple * srq_tuple_find(const char *name, tsrq_tuple **tuples, size_t tuplescount) {
    size_t index;

    if (name == NULL || *name == '\0' || tuples == NULL || tuplescount == 0) {
        return((tsrq_tuple *)NULL);
    }
    for (index = 0; index < tuplescount; index++) {
        if (strcasecmp(tuples[index]->name, name) == 0) {
            return(tuples[index]);
        }
    }
    return((tsrq_tuple *)NULL);
}


/*
 *
 */
static void srq_pairs_free(tsrq_tuple ***tuples, size_t tuplescount) {
    size_t index, index2;

    if (*tuples == NULL || tuplescount == 0) {
        return;
    }
    for (index = 0; index < tuplescount; index++) {
        if ((*tuples)[index]->name) {
            free((*tuples)[index]->name);
        }
        for (index2 = 0; index2 < (*tuples)[index]->valuescount; index2++) {
            if ((*tuples)[index]->values[index2]) {
                free((*tuples)[index]->values[index2]);
            }
        }
        free((*tuples)[index]->values);
    }
    free(*tuples);
    *tuples = NULL;
}


/*
 *
 */
static int srq_readform(enum request_method method, void ***_METHOD, size_t *methodcount) {
    enum { FIELDNAME, FIELDVALUE, HEXCHAR };

    char *ptr, c, curvalue[FVALUESZ + 1], curname[FNAMESZ + 1];
    int index = 0, hexcnt = 0, hexc = 0;
    size_t size;
    int current, ostate = 0, pairs_poolsz = 0;
    bool end = false;
    tsrq_tuple *tuple = NULL;
    tsrq_tuple ***_form = NULL;
    char **_put = NULL;

    switch (method) {
        case POST:
        case PUT:
            if ((ptr = (char *)getenv("CONTENT_LENGTH")) == NULL) {
                return(3);
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
            _put = (char **)_METHOD;
            break;
        case GET:
        case POST:
        default:
            _form = (tsrq_tuple ***)_METHOD;
            break;
    }
    
    *methodcount = 0;
    current = FIELDNAME;
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
                if (*_put == NULL) {
                    *_put = (char *)malloc(size + 1);
                    memset(*_put, 0, size + 1);
                }
                *_put[(*methodcount)++] = c;
                if (end) {
                    (*methodcount)--;
                }

            } else {
                if (end || c == '&') {
                    curvalue[index] = '\0';
                    if (tuple != NULL) {
                        if (*methodcount >= FMAX_COUNT) {
                            end = true;
                        }
                        if (tuple->values == NULL) {
                            tuple->values = (char **)malloc(sizeof(char *) * (tuple->valuescount + 1));
                        } else {
                            tuple->values = (char **)realloc(tuple->values, sizeof(char *) * (tuple->valuescount + 1));
                        }
                        tuple->values[tuple->valuescount++] = strdup(curvalue);
                    }

                    index = 0;
                    current = FIELDNAME;

                } else if (c == '=') {
                    curname[index] = '\0';
                    if ((tuple = srq_tuple_find(curname, *_form, *methodcount)) == NULL) {
                        if (*methodcount >= pairs_poolsz) {
                            if (*_form == NULL) {
                                *_form = (tsrq_tuple **)malloc(sizeof(tsrq_tuple *) * PAIRS_POOLSZ);
                            } else {
                                *_form = (tsrq_tuple **)realloc(*_form, sizeof(tsrq_tuple *) * (pairs_poolsz + PAIRS_POOLSZ));
                            }
                            pairs_poolsz += PAIRS_POOLSZ;
                        }
                        tuple = (tsrq_tuple *)malloc(sizeof(tsrq_tuple));
                        memset(tuple, 0, sizeof(tsrq_tuple));
                        tuple->name = strdup(curname);
                        (*_form)[(*methodcount)++] = tuple;
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
    enum { NONE, NEWPAIR, READPAIR, NEWFILE, READFILE };

    char *ptr;
    char buffer[MFD_CHUNKSIZE], *brm, *tok1, *ptr2, *ptr3;
    int state = NONE, oldstate = NONE;
    bool end, endfile;
    int pairs_poolsz = 0;
    char *content_type;
    char *boundary, *bound_start, *bound_startrn, *bound_end;
    char *file_ct;
    size_t file_len, chunk_len;
    size_t end_len, start_len;
    tsrq_tuple *tuple;
    
    content_type = getenv("CONTENT_TYPE");
    boundary = content_type + strlen(MFD_STRING);
    asprintf(&bound_start, "--%s", boundary);
    asprintf(&bound_startrn, "--%s\r\n", boundary);
    asprintf(&bound_end, "--%s--", boundary);
    start_len = strlen(bound_startrn);
    end_len = strlen(bound_end);

    end = false;
    do {
        switch (state) {
            case NONE:
            case NEWPAIR:
            case NEWFILE:
                if (feof(stdin)) {
                    end = true;
                } else {
                    fgets(buffer, sizeof(buffer), stdin);
                    if ((ptr = strcasestr(buffer, MFD_NEWLINE)) == buffer) {
                        oldstate = state;
                        state = (state == NEWPAIR ? READPAIR : (state == NEWFILE ? READFILE : NONE));

                    } else if (ptr == NULL) {
                        free(bound_start);
                        free(bound_end);
                        free(bound_startrn);
                        return(3);

                    } else {
                        *ptr = '\0';
                        if (strcmp(buffer, bound_end) == 0) {
                            end = true;

                        } else if (strcmp(buffer, bound_start) == 0) {
                            oldstate = state;
                            state = NEWPAIR;

                        } else if (strcasestr(buffer, MFD_CD_STRING) == buffer) {
                            ptr = buffer + strlen(MFD_CD_STRING);
                            if ((tok1 = strtok_r(ptr, ";", &brm)) != NULL) {
                                if (strcasestr(tok1, "name=") != tok1) {
                                    free(bound_start);
                                    free(bound_end);
                                    free(bound_startrn);
                                    return(4);
                                }
                                ptr3 = NULL;
                                ptr2 = tok1 + strlen("name=");
                                if (*ptr2 == '"') {
                                    ptr3 = ++ptr2;
                                    while (*ptr3 != '"') {
                                        ptr3++;
                                    }
                                    *ptr3 = '\0';
                                }
                                if ((tuple = srq_tuple_find(ptr2, request->_POST, request->postcount)) == NULL) {
                                    if (request->postcount >= pairs_poolsz) {
                                        if (request->_POST == NULL) {
                                            request->_POST = (tsrq_tuple **)malloc(sizeof(tsrq_tuple *) * PAIRS_POOLSZ);
                                            
                                        } else {
                                            request->_POST = (tsrq_tuple **)realloc(request->_POST, sizeof(tsrq_tuple *) * (pairs_poolsz + PAIRS_POOLSZ));
                                        }
                                        memset((request->_POST + pairs_poolsz), 0, sizeof(tsrq_tuple *) * PAIRS_POOLSZ);
                                        pairs_poolsz += PAIRS_POOLSZ;
                                    }
                                    tuple = (tsrq_tuple *)malloc(sizeof(tsrq_tuple));
                                    memset(tuple, 0, sizeof(tsrq_tuple));
                                    tuple->name = strdup(ptr2);
                                    request->_POST[request->postcount++] = tuple;
                                }

                                if (ptr3) {
                                    *ptr3 = '"';
                                }
                                if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                                    oldstate = state;
                                    state = NEWPAIR;

                                } else {
                                    if (*tok1 == ' ') {
                                        tok1++;
                                    }
                                    if (strcasestr(tok1, "filename=") != tok1) {
                                        free(bound_start);
                                        free(bound_end);
                                        free(bound_startrn);
                                        return(5);
                                    }
                                    ptr3 = NULL;
                                    ptr2 = tok1 + strlen("filename=");
                                    if (*ptr2 == '"') {
                                        ptr3 = ++ptr2;
                                        while (*ptr3 != '"') {
                                            ptr3++;
                                        }
                                        *ptr3 = '\0';
                                    }
                                    if (tuple->values == NULL) {
                                        tuple->values = (char **)malloc(sizeof(char *) * (tuple->valuescount + 1));
                                    } else {
                                        tuple->values = (char **)realloc(tuple->values, sizeof(char *) * (tuple->valuescount + 1));
                                    }
                                    if (*ptr2 == '\0') {
                                        tuple->values[tuple->valuescount] = NULL;
                                    } else {
                                        tuple->values[tuple->valuescount] = strdup(ptr2);
                                    }
                                    if (ptr3) {
                                        *ptr3 = '"';
                                    }
                                    if (tuple->values[tuple->valuescount] != NULL) {
                                        if (request->_FILES == NULL) {
                                            request->_FILES = (tsrq_file *)malloc(sizeof(tsrq_file));

                                        } else {
                                            request->_FILES = (tsrq_file *)realloc(request->_FILES, sizeof(tsrq_file) * (request->filescount + 1));
                                        }
                                        memset((request->_FILES + request->filescount), 0, sizeof(tsrq_file));
                                        request->_FILES[request->filescount].filename = strdup(tuple->values[tuple->valuescount]);
                                    }
                                    oldstate = state;
                                    state = NEWFILE;
                                }
                            }

                        } else if (strcasestr(buffer, MFD_CT_STRING) == buffer) {
                            ptr = buffer + strlen(MFD_CT_STRING);
                            if (tuple->values[tuple->valuescount] != NULL) {
                                request->_FILES[request->filescount].content_type = strdup(ptr);
                            }

                        } else {
                            free(bound_start);
                            free(bound_end);
                            free(bound_startrn);
                            return(6);
                        }
                    }
                }
                break;

            case READPAIR:
                fgets(buffer, sizeof(buffer), stdin);
                if (feof(stdin)) {
                    end = true;
                }
                if ((ptr = strcasestr(buffer, MFD_NEWLINE)) == NULL) {
                    free(bound_start);
                    free(bound_end);
                    free(bound_startrn);
                    return(7);

                } else {
                    *ptr = '\0';
                    if (strcmp(buffer, bound_end) == 0) {
                        tuple->valuescount++;
                        end = true;

                    } else if (strcmp(buffer, bound_start) == 0) {
                        tuple->valuescount++;
                        oldstate = state;
                        state = NEWPAIR;

                    } else {
                        if (tuple) {
                            if (tuple->values == NULL) {
                                tuple->values = (char **)malloc(sizeof(char *) * (tuple->valuescount + 1));
                                tuple->values[tuple->valuescount] = strdup(buffer);
                                
                            } else {
                                if (oldstate == NEWPAIR) {
                                    tuple->values = (char **)realloc(tuple->values, sizeof(char *) * (tuple->valuescount + 1));
                                    tuple->values[tuple->valuescount] = strdup(buffer);
                                } else {
                                    tuple->values[tuple->valuescount] = (char *)realloc(tuple->values[tuple->valuescount],
                                                                                    strlen(tuple->values[tuple->valuescount]) + 1
                                                                                    + strlen(buffer) + 1);
                                    strcat(tuple->values[tuple->valuescount], "\n");
                                    strcat(tuple->values[tuple->valuescount], buffer);
                                }
                            }
                        }
                    }
                }
                break;
                
            case READFILE:
                file_len = 0;
                file_ct = (char *)malloc(MFD_CHUNKSIZE);
                memset(file_ct, 0, MFD_CHUNKSIZE);
                chunk_len = MFD_CHUNKSIZE;
                endfile = false;
                do {
                    if (file_len >= chunk_len) {
                        file_ct = (char *)realloc(file_ct, chunk_len + MFD_CHUNKSIZE);
                        chunk_len += MFD_CHUNKSIZE;
                    }
                    file_ct[file_len] = (char)fgetc(stdin);
                    file_len++;
                    if (feof(stdin)) {
                        endfile = true;
                        end = true;
                        file_len--;
                    }
                    if (file_len >= end_len && strcmp((file_ct + file_len - end_len), bound_end) == 0) {
                        if (tuple->values[tuple->valuescount] == NULL) {
                            free(file_ct);

                        } else {
                            request->_FILES[request->filescount].length = (file_len - end_len);
                            if (request->_FILES[request->filescount].length > 2) {
                                request->_FILES[request->filescount].length -= 2;
                            }
                            if (request->_FILES[request->filescount].length > maxfilesize) {
                                free(request->_FILES[request->filescount].data);
                                request->_FILES[request->filescount].data = NULL;
                                request->_FILES[request->filescount].length = -1;

                            } else {
                                file_ct = (char *)realloc(file_ct, request->_FILES[request->filescount].length);
                                request->_FILES[request->filescount].data = file_ct;
                            }
                            request->filescount++;
                            tuple->valuescount++;
                        }
                        endfile = true;
                        end = true;
                    } else if (file_len >= start_len && strcmp((file_ct + file_len - start_len), bound_startrn) == 0) {
                        if (tuple->values[tuple->valuescount] == NULL) {
                            free(file_ct);

                        } else {
                            request->_FILES[request->filescount].length = (file_len - start_len);
                            if (request->_FILES[request->filescount].length > 2) {
                                request->_FILES[request->filescount].length -= 2;
                            }
                            if (request->_FILES[request->filescount].length > maxfilesize) {
                                free(request->_FILES[request->filescount].data);
                                request->_FILES[request->filescount].data = NULL;
                                request->_FILES[request->filescount].length = -1;

                            } else {
                                file_ct = (char *)realloc(file_ct, request->_FILES[request->filescount].length);
                                request->_FILES[request->filescount].data = file_ct;
                            }
                            request->filescount++;
                            tuple->valuescount++;
                        }
                        endfile = true;
                        oldstate = state;
                        state = NEWPAIR;
                    }
                } while (!endfile);
                break;
        }
    } while (!end);
    free(bound_start);
    free(bound_end);
    free(bound_startrn);

    return(0);
}

