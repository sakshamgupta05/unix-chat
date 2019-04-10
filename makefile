all: bin/multithread bin/eventdriven bin/client

dir:
	mkdir bin
	mkdir build

bin/multithread: build/multithread.o build/read_line.o
	gcc -pthread -o bin/multithread build/multithread.o build/read_line.o

bin/eventdriven: build/eventdriven.o build/read_line.o
	gcc -o bin/eventdriven build/eventdriven.o build/read_line.o

bin/client: build/client.o build/read_line.o
	gcc -o bin/client build/client.o build/read_line.o

build/multithread.o: src/multithread.c
	gcc -c src/multithread.c -o build/multithread.o

build/eventdriven.o: src/eventdriven.c
	gcc -c src/eventdriven.c -o build/eventdriven.o

build/client.o: src/client.c
	gcc -c src/client.c -o build/client.o

build/read_line.o: src/read_line.c
	gcc -c src/read_line.c -o build/read_line.o

clean:
	rm -rf bin
	rm -rf build
