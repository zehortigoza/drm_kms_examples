CFLAGS	= -g -Wall -Wextra -s -O3
LIBS	= `pkg-config --cflags --libs libdrm`
COMMON = src/common.o src/debugfs.o

all: frontbuffer_drawing.bin page_flip.bin page_flip2.bin page_flip3.bin page_flip3_psr2.bin cursor.bin page_flip_force_resolution.bin frontbuffer_drawing2.bin frontbuffer_drawing3.bin frontbuffer_drawing3_psr2.bin read_debugfs.bin

frontbuffer_drawing.bin: src/frontbuffer_drawing.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
	
page_flip.bin: src/page_flip.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

page_flip2.bin: src/page_flip2.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

page_flip3.bin: src/page_flip3.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

page_flip3_psr2.bin: src/page_flip3_psr2.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

cursor.bin: src/cursor.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

page_flip_force_resolution.bin: src/page_flip_force_resolution.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

frontbuffer_drawing2.bin: src/frontbuffer_drawing2.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

frontbuffer_drawing3.bin: src/frontbuffer_drawing3.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

frontbuffer_drawing3_psr2.bin: src/frontbuffer_drawing3_psr2.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

read_debugfs.bin: src/read_debugfs.o src/debugfs.o
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) $(LIBS) -c $< -o $@

clean:
	rm src/*.o
	rm *.bin
