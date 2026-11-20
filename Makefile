CC = gcc

# OS Detection
ifeq ($(OS),Windows_NT)
    TARGET = huffman_server.exe
    TEST_TARGET = test_huffman.exe
    CFLAGS = -Wall -Wextra -O2 -Ibackend/include -Ibackend/lib -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DNO_SSL -DNO_CGI -include limits.h -include ctype.h -D_TIMESPEC_DEFINED
    LDFLAGS = -lws2_32
    CLEAN_CMD = -del /Q /F huffman_server.exe test_huffman.exe *.o 2>nul
else
    UNAME_S := $(shell uname -s)
    TARGET = huffman_server
    TEST_TARGET = test_huffman
    CFLAGS = -Wall -Wextra -O2 -Ibackend/include -Ibackend/lib -DNO_SSL -DNO_CGI
    LDFLAGS = -lpthread
    CLEAN_CMD = rm -f huffman_server test_huffman *.o
endif

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

all: $(TARGET) $(TEST_TARGET)

$(TARGET): $(SERVER_SRCS)
	$(CC) $(CFLAGS) $(SERVER_SRCS) $(LDFLAGS) -o $(TARGET)

$(TEST_TARGET): $(TEST_SRCS)
	$(CC) $(CFLAGS) $(TEST_SRCS) $(LDFLAGS) -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	$(CLEAN_CMD)
