#define SIZE 100000 // maximum limit on the size of a file that can be requested (see computeSHA256Sum)
// #define ERROR_SIZE 65
#define BACKLOG 100 // Passed to listen()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h> // for getnameinfo()
#include <sys/stat.h>
#include <dirent.h> // for O_RDONLY
#include <fcntl.h>  // for DIR type
#include <pthread.h>
#include <time.h> // see usr/include
#include <poll.h>

// Usual socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

extern char *root_path;

void parseRequest(char *buff, int clientSocket);
void *readRequest(void *pClient);
void *processPipelined(void *pClient);
void checkDir(char *root_path);
int checkFile(char *filename);
char *getFilePath(char *filename);
char *getBody(char *filename, int *size);
char *getHeader(int length, char *filename);
void sendResponse(char *response, int size, int clientSocket);
void sendErrorResponse(char *response, int clientSocket);
void createResponse(char *filepath, int clientSocket, int resp_type, char *resp_d1);
void createResponsePersistent(char *filepath, int clientSocket, int if_mod_since_resp_type, char *resp_d1, int if_unmod_since_resp_type, char *resp_d2, int if_match_resp_type, char *resp_d3, int if_none_match_resp_type, char *resp_d4);
char *createSimpleCustomResponse(char *code);
char *getRequest1stLine(char buff[]);
char *getRequestifUnModSinceLine(char buff[]);
char *getRequestifMatchLine(char buff[]);
char *getRequestifNoneMatchLine(char buff[]);
char *getDateFromifModSinceHeaderLine(char *if_mod_since_line);
char *getDateFromifUnModSinceHeaderLine(char *if_mod_since_line);
char *getSHA256SumFromifMatchHeaderLine(char *if_match_since_line);
char *parseRequests1stLine(char *first_line, int clientSocket);
char *getRequestifModSinceLine(char buff[]);
char *getRequestLineAttr(char *line);
int createResponseForifUnModSince(char *filepath, int clientSocket, int if_unmod_since_resp_type, char *resp_d2);
int createResponseForifMatch(char *filepath, int clientSocket, int if_match_resp_type, char *resp_d3);
int createResponseForifNoneMatch(char *filepath, int clientSocket, int if_none_match_resp_type, char *resp_d4);
char *getLastModifiedDate(char *filepath);
struct tm strTime2tm(char *s);
int strMonthAbbr2int(char *s);
char *createSimpleCustomResponse(char *str);
