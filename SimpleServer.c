#include "helper.h"

char *root_path;

// entry point
int main(int argc, char *argv[])
{

	// error checking
	if (argc != 3)
	{
		fprintf(stderr, "Usage: %s <TCP port number> <rel. or abs. http path>\n", argv[0]);
		exit(1);
	}

	char *nptr = argv[1];
	char *endptr = NULL;
	errno = 0; // reset errno
	int port_n = strtol(nptr, &endptr, 10);

	if ((errno == 0 && port_n && *endptr != 0) || (nptr == endptr) || ((0 > port_n) || (port_n > 65353)))
	{
		fprintf(stderr, "Invalid TCP port number provided\n");
		exit(1);
	}

	root_path = argv[2];
	checkDir(root_path);

	// set socket attributes, bind server socket to port and listen
	printf("port: %d\nroot_path: %s\n", port_n, root_path);
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serverAddress;

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port_n);
	serverAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
	int listening = listen(serverSocket, BACKLOG);

	if (listening < 0)
	{
		printf("Error: The server is not listening.\n");
		return 1;
	}
	printf("Listening on port %d\n", port_n);

	// loop, accept and process connections/requests using spawned thread
	int clientSocket;
	while (1)
	{
		clientSocket = accept(serverSocket, NULL, NULL);
		printf("\n\naccepted connection\n");
		pthread_t t;
		int *pClient = malloc(sizeof(int));
		*pClient = clientSocket;
		pthread_create(&t, NULL, readRequest, pClient);
	}

	return 0;
}

// REFERENCES
// https://dev-notes.eu/2018/06/http-server-in-c/