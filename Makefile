all:
	gcc -o server server.c -pthread
	gcc -o client client.c -pthread

clean:
	rm server
	rm client

