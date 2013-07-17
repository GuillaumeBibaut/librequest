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

#include "srq-files.h"


/*
 *
 */
tsrq_file *srq_files_add(tsrq_files *files, const char *filename) {
    tsrq_file *file;
    
    if ((file = srq_files_find(*files, filename)) != NULL) {
        return(NULL);
    } else {
        files->files = realloc(files->files, sizeof(tsrq_file *) * (files->count + 1));
        if (files->files == NULL) {
            return(NULL);
        }
        if ((file = srq_file_create(filename)) == NULL) {
            return(NULL);
        }
        files->files[files->count] = file;
        files->count++;
    }
    return(file);
}

tsrq_file *srq_files_find(tsrq_files files, const char *filename) {
    size_t index;
    
    if (filename == NULL || *filename == '\0') {
        return(NULL);
    }
    if (files.count == 0 || files.files == NULL) {
        return(NULL);
    }
    for (index = 0; index < files.count; index++) {
        if (strcasecmp(files.files[index]->filename, filename) == 0) {
            return(files.files[index]);
        }
    }
    return(NULL);
}

