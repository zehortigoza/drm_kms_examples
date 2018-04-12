CFLAGS	= -g -Wall -Wextra
LIBS	= `pkg-config --cflags --libs libdrm`
COMMON = src/common.o

all: frontbuffer_drawing.bin page_flip.bin

frontbuffer_drawing.bin: src/frontbuffer_drawing.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^
	
page_flip.bin: src/page_flip.o $(COMMON)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

%.o : %.c
	$(CC) $(CFLAGS) $(LIBS) -c $< -o $@

clean:
	rm src/*.o
	rm *.bin
