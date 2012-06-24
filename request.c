#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "request.h"

#define FNAMESZ 256
#define FVALUESZ 8192

#define PAIRS_POOLSZ 4

#define unhex(c) (((c) >= '0' && (c) <= '9') ? (c) - '0' : \
        ((c) >= 'a' && (c) <= 'f') ? (c) - 'a' + 10 : \
        ((c) >= 'A' && (c) <= 'F') ? (c) - 'A' + 10 : 0)

#define MFD_STRING "multipart/form-data; boundary="
#define MFD_CD_STRING "Content-Disposition: form-data; "
#define MFD_CT_STRING "Content-Type: "
#define MFD_NEWLINE "\r\n"
#define MFD_CHUNKSIZE (32 * 1024)

#define SRQ_MAXFILESIZE (8 * 1024 * 1024)

enum request_method {POST, GET, PUT};

static int srq_readform(enum request_method method, void **_METHOD, size_t *methodcount);
static int srq_readmfd(tsrq_request *request, size_t maxfilesize);
static void srq_pairs_free(tsrq_pair *pairs, int pairscount);

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
    tsrq_pair *_GET = NULL, *_POST = NULL;
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
        if ((res = srq_readform(GET, (void **)&_GET, &getcount)) != 0) {
            return(request);
        }
        request->_GET = _GET;
        request->getcount = getcount;
    }

    if (strcmp(ptr, "POST") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
            return(request);
        }
        if ((ptr = getenv("CONTENT_TYPE")) == NULL) {
            return(request);
        }
        if (strstr(ptr, MFD_STRING) == NULL) {
            if ((res = srq_readform(POST, (void **)&_POST, &postcount)) != 0) {
                return(request);
            }
            request->_POST = _POST;
            request->postcount = postcount;
        } else {
            if ((res = srq_readmfd(request, maxfilesize)) != 0) {
                return(request);
            }
        }
    } else if (strcmp(ptr, "PUT") == 0) {
        if (getenv("CONTENT_LENGTH") == NULL) {
            return(request);
        }
        if ((res = srq_readform(PUT, (void **)&_PUT, &putcount)) != 0) {
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

    srq_pairs_free(request->_GET, request->getcount);
    srq_pairs_free(request->_POST, request->postcount);
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
bool srq_pair_lookup(const char *name, tsrq_lookup lookup, tsrq_lookup *result) {
    bool res = false;
    int index;
    int pairs_poolsz = 0;
     
    if (name == NULL || *name == '\0'
        || lookup.pairs == NULL || lookup.pairscount == 0) {
        return(result);
    }
    if (result != NULL) {
        memset(result, 0, sizeof(tsrq_lookup));
    }
    for (index = 0; index < lookup.pairscount; index++) {
        if (strcmp(lookup.pairs[index].name, name) == 0) {
            res = true;
            if (result == NULL) {
                break;
            } else {
                if (result->pairscount >= pairs_poolsz) {
                    if (result->pairs == NULL) {
                        result->pairs = (tsrq_pair *)malloc(sizeof(tsrq_pair) * PAIRS_POOLSZ);
                    } else {
                        result->pairs = (tsrq_pair *)realloc(result->pairs, sizeof(tsrq_pair) * (pairs_poolsz + PAIRS_POOLSZ));
                    }
                    memset((result->pairs + pairs_poolsz), 0, sizeof(tsrq_pair) * PAIRS_POOLSZ);
                    pairs_poolsz += PAIRS_POOLSZ;
                }
                result->pairs[result->pairscount] = lookup.pairs[index];
                result->pairscount++;
            }
        }
    }
    return(res);
}


/*
 *
 */
static void srq_pairs_free(tsrq_pair *pairs, int pairscount) {
    int index;

    if (pairs == NULL || pairscount == 0) {
        return;
    }
    for (index = 0; index < pairscount; index++) {
        if (pairs[index].name) {
            free(pairs[index].name);
        }
        if (pairs[index].value) {
            free(pairs[index].value);
        }
    }
    free(pairs);
    pairs = NULL;
}


/*
 *
 */
static int srq_readform(enum request_method method, void **_METHOD, size_t *methodcount) {
    enum { FIELDNAME, FIELDVALUE, HEXCHAR };

    char *ptr, c, curvalue[FVALUESZ + 1], curname[FNAMESZ + 1];
    int size, index = 0, hexcnt = 0, hexc = 0;
    int current, ostate = 0, pairs_poolsz = 0;
    bool end = false;

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
                if (*_METHOD == NULL) {
                    *_METHOD = (char *)malloc(size + 1);
                    memset(*_METHOD, 0, size + 1);
                }
                ((char *)(*_METHOD))[(*methodcount)++] = c;
                if (end) {
                    (*methodcount)--;
                }

            } else {
                if (end || c == '&') {
                    curvalue[index] = '\0';
                    if (*_METHOD != NULL) {
                        ((tsrq_pair *)(*_METHOD))[*methodcount].value = strdup(curvalue);
                        (*methodcount)++;
                    }

                    index = 0;
                    current = FIELDNAME;

                } else if (c == '=') {
                    curname[index] = '\0';
                    if (*methodcount >= pairs_poolsz) {
                        if (*_METHOD == NULL) {
                            *_METHOD = (tsrq_pair *)malloc(sizeof(tsrq_pair) * PAIRS_POOLSZ);
                        } else {
                            *_METHOD = (tsrq_pair *)realloc(*_METHOD, sizeof(tsrq_pair) * (pairs_poolsz + PAIRS_POOLSZ));
                        }
                        memset((*_METHOD + pairs_poolsz), 0, sizeof(tsrq_pair) * PAIRS_POOLSZ);
                        pairs_poolsz += PAIRS_POOLSZ;
                    }
                    ((tsrq_pair *)(*_METHOD))[*methodcount].name = strdup(curname);

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
    char buffer[1024], *brm, *tok1, *ptr2, *ptr3;
    int state = NONE;
    bool end, endfile;
    int pairs_poolsz = 0;
    char *content_type;
    char *boundary, *bound_start, *bound_startrn, *bound_end;
    char *file_ct;
    size_t file_len, chunk_len;
    size_t end_len, start_len;
    
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
                    if ((ptr = strstr(buffer, MFD_NEWLINE)) == buffer) {
                        state = (state == NEWPAIR ? READPAIR : (state == NEWFILE ? READFILE : NONE));
                        if (state == READPAIR) {
                            fgets(buffer, sizeof(buffer), stdin);
                        }
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
                            state = NEWPAIR;
                        } else if (strstr(buffer, MFD_CD_STRING) == buffer) {
                            ptr = buffer + strlen(MFD_CD_STRING);
                            if ((tok1 = strtok_r(ptr, ";", &brm)) != NULL) {
                                if (strstr(tok1, "name=") != tok1) {
                                    free(bound_start);
                                    free(bound_end);
                                    free(bound_startrn);
                                    return(3);
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
                                if (request->postcount >= pairs_poolsz) {
                                    if (request->_POST == NULL) {
                                        request->_POST = (tsrq_pair *)malloc(sizeof(tsrq_pair) * PAIRS_POOLSZ);
                                    } else {
                                        request->_POST = (tsrq_pair *)realloc(request->_POST, sizeof(tsrq_pair) * (pairs_poolsz + PAIRS_POOLSZ));
                                    }
                                    memset((request->_POST + pairs_poolsz), 0, sizeof(tsrq_pair) * PAIRS_POOLSZ);
                                    pairs_poolsz += PAIRS_POOLSZ;
                                }
                                request->_POST[request->postcount].name = strdup(ptr2);

                                if (ptr3) {
                                    *ptr3 = '"';
                                }
                                if ((tok1 = strtok_r(NULL, ";", &brm)) == NULL) {
                                    state = NEWPAIR;
                                } else {
                                    if (*tok1 == ' ') {
                                        tok1++;
                                    }
                                    if (strstr(tok1, "filename=") != tok1) {
                                        free(bound_start);
                                        free(bound_end);
                                        free(bound_startrn);
                                        return(3);
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
                                    request->_POST[request->postcount].value = strdup(ptr2);
                                    if (ptr3) {
                                        *ptr3 = '"';
                                    }
                                    if (request->_POST[request->postcount].value[0] != '\0') {
                                        if (request->_FILES == NULL) {
                                            request->_FILES = (tsrq_file *)malloc(sizeof(tsrq_file));
                                        } else {
                                            request->_FILES = (tsrq_file *)realloc(request->_FILES, sizeof(tsrq_file) * (request->filescount + 1));
                                        }
                                        memset((request->_FILES + request->filescount), 0, sizeof(tsrq_file));
                                        request->_FILES[request->filescount].filename = strdup(request->_POST[request->postcount].value);
                                    }
                                    state = NEWFILE;
                                }
                            }
                        } else if (strstr(buffer, MFD_CT_STRING) == buffer) {
                            ptr = buffer + strlen(MFD_CT_STRING);
                            if (request->_POST[request->postcount].value[0] != '\0') {
                                request->_FILES[request->filescount].content_type = strdup(ptr);
                            }
                        } else {
                            free(bound_start);
                            free(bound_end);
                            free(bound_startrn);
                            return(3);
                        }
                    }
                }
                break;

            case READPAIR:
                if ((ptr = strstr(buffer, MFD_NEWLINE)) == NULL) {
                    free(bound_start);
                    free(bound_end);
                    free(bound_startrn);
                    return(3);
                } else {
                    *ptr = '\0';
                    if (strcmp(buffer, bound_end) == 0) {
                        request->postcount++;
                        end = true;
                    } else if (strcmp(buffer, bound_start) == 0) {
                        request->postcount++;
                        state = NEWPAIR;
                    } else {
                        if (request->_POST[request->postcount].value == NULL) {
                            request->_POST[request->postcount].value = strdup(buffer);
                        } else {
                            request->_POST[request->postcount].value = (char *)realloc(request->_POST[request->postcount].value,
                                    strlen(request->_POST[request->postcount].value) + 1 + strlen(buffer) + 1);
                            strcat(request->_POST[request->postcount].value, "\n");
                            strcat(request->_POST[request->postcount].value, buffer);
                        }
                        if (feof(stdin)) {
                            end = true;
                        } else {
                            fgets(buffer, sizeof(buffer), stdin);
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
                        if (request->_POST[request->postcount].value[0] == '\0') {
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
                            request->postcount++;
                        }
                        endfile = true;
                        end = true;
                    } else if (file_len >= start_len && strcmp((file_ct + file_len - start_len), bound_startrn) == 0) {
                        if (request->_POST[request->postcount].value[0] == '\0') {
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
                            request->postcount++;
                        }
                        endfile = true;
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

