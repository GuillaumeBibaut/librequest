LIB=		request

SRCS+=		srq-request.c
SRCS+=		srq-tuple.c
SRCS+=		srq-tuples.c
SRCS+=		srq-file.c
SRCS+=		srq-files.c

WITHOUT_PROFILE=    1
NO_PROFILE=         1
WITHOUT_INFO=       1
NO_INFO=            1
WITHOUT_DOC=        1
NO_OBJ=             1
NO_MAN=             1

WARNS=              2
#DEBUG_FLAGS+=		-g -ggdb

#SHLIB_MAJOR=0.1.1

.include <bsd.lib.mk>
