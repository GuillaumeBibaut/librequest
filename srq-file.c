#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srq-file.h"


tsrq_file *srq_file_create(const char *filename) {
    tsrq_file *file;
    
    if (filename == NULL || *filename == '\0') {
        return(NULL);
    }
    file = calloc(1, sizeof(tsrq_file));
    if (file == NULL) {
        return(NULL);
    }
    file->filename = strdup(filename);
    if (file->filename == NULL) {
        srq_file_free(file);
        return(NULL);
    }
    
    return(file);
}


void srq_file_free(tsrq_file *file) {
    
    if (file == NULL) {
        return;
    }
    if (file->filename) {
        free(file->filename);
    }
    if (file->content_type) {
        free(file->content_type);
    }
    if (file->data) {
        free(file->data);
    }
    free(file);
}
