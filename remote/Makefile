CC=gcc

all: remote_code

remote_code: remote_code.o mock_canvas.o
	$(CC) -g -o $@ $^

%.o: %.c param.h canvas.h common.h
	$(CC) -Wall -g -c -o $@ $<

clean:
	rm -f remote_code.o mock_canvas.o
