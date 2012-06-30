LIB=		request

SRCS+=		request.c

WITHOUT_PROFILE=    1
WITHOUT_INFO=       1
WITHOUT_DOC=        1
NO_OBJ=             1
NO_MAN=             1
#DEBUG_FLAGS=        -g -ggdb
WARNS=              2

SHLIB_MAJOR=0.1.1

.include <bsd.lib.mk>
