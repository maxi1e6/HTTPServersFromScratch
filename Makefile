Servers: SimpleHelper.o SimpleServer.o PersistentHelper.o PersistentServer.o PipelinedHelper.o PipelinedServer.o sha256.o
	gcc -Wall -Werror -o SimpleServer SimpleHelper.o SimpleServer.o -lpthread
	gcc -Wall -Werror -o PersistentServer PersistentHelper.o PersistentServer.o sha256.o -lpthread
	gcc -Wall -Werror -o PipelinedServer PipelinedHelper.o PipelinedServer.o sha256.o -lpthread

%.o: %.c
	gcc -c -g -Wall -Werror -o $@ $<

clean:
	rm -f SimpleServer PersistentServer PipelinedServer *.o