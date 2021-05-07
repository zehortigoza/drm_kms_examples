CFLAGS  = -g -Wall -Wextra -s -O3
CFLAGS  += `pkg-config --cflags libdrm libdrm_intel`
LDFLAGS += `pkg-config --libs libdrm libdrm_intel`
COMMON = src/common.o src/debugfs.o

all: frontbuffer_drawing.bin page_flip.bin page_flip2.bin page_flip3.bin page_flip3_psr2.bin cursor.bin page_flip_force_resolution.bin frontbuffer_drawing2.bin frontbuffer_drawing3.bin frontbuffer_drawing3_psr2.bin read_debugfs.bin submission.bin

frontbuffer_drawing.bin: src/frontbuffer_drawing.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

page_flip.bin: src/page_flip.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

page_flip2.bin: src/page_flip2.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

page_flip3.bin: src/page_flip3.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

page_flip3_psr2.bin: src/page_flip3_psr2.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

cursor.bin: src/cursor.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

page_flip_force_resolution.bin: src/page_flip_force_resolution.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

frontbuffer_drawing2.bin: src/frontbuffer_drawing2.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

frontbuffer_drawing3.bin: src/frontbuffer_drawing3.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

frontbuffer_drawing3_psr2.bin: src/frontbuffer_drawing3_psr2.o $(COMMON)
	$(CC) -o $@ $^ $(LDFLAGS)

read_debugfs.bin: src/read_debugfs.o src/debugfs.o
	$(CC) -o $@ $^ $(LDFLAGS)

submission.bin: src/gem_submission/submission.o src/gem_submission/lib.o
	$(CC) -o $@ $^ $(LDFLAGS)

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf src/*.o
	rm -rf src/gem_submission/*.o
	rm -rf *.bin
