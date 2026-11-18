CC = gcc
CFLAGS = -Wall -Wextra -O2 -Ibackend/include -Ibackend/lib -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DNO_SSL -DNO_CGI -include limits.h -include ctype.h -D_TIMESPEC_DEFINED
LDFLAGS = -lws2_32

COMMON_SRCS = backend/src/frequency.c \
              backend/src/minheap.c \
              backend/src/tree.c \
              backend/src/compress.c \
              backend/src/decompress.c \
              backend/src/file_handler.c \
              backend/lib/cJSON.c

SERVER_SRCS = $(COMMON_SRCS) \
              backend/src/server.c \
              backend/src/main.c \
              backend/lib/civetweb.c

TEST_SRCS = $(COMMON_SRCS) \
            backend/tests/test_huffman.c

all: huffman_server.exe test_huffman.exe

huffman_server.exe: $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(LDFLAGS) -o huffman_server.exe

test_huffman.exe: $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) -o test_huffman.exe

test: test_huffman.exe
	./test_huffman.exe

clean:
	-del /Q huffman_server.exe test_huffman.exe *.o 2>nul
