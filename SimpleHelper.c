#include "helper.h"

// Check if file filename exists
int checkFileExists(char *filename)
{
	if (access(filename, F_OK) == 0)
	{
		return 0;
	}
	else
	{
		return -1;
	}
}

// check if directory rootpath is valid
void checkDir(char *root_path)
{
	// edge case condition:
	if (strcmp(root_path, "./") == 0)
	{
		fprintf(stderr, "Invalid rel. or abs. http path provided\n");
		exit(1);
	}
	int root_path_fd = open(root_path, O_RDONLY);
	DIR *dir_stream = fdopendir(root_path_fd);
	if (dir_stream == NULL)
	{
		fprintf(stderr, "Invalid rel. or abs. http path provided\n");
		exit(1);
	}
	if (closedir(dir_stream) != 0)
	{
		fprintf(stderr, "closedir failed\n");
		exit(1);
	}
}

// read in a single client request and then process it
void *readRequest(void *pClient)
{
	int clientSocket = *((int *)pClient);
	free(pClient);
	int n;
	char buff[SIZE];
	memset(buff, 0, SIZE);
	int read_in = 0;
	int cr = 0;
	int lf = 0;

	// read request into buffer
	while ((n = read(clientSocket, &buff[read_in], 1)) > 0)
	{
		if (buff[read_in] == '\r')
		{
			cr++;
		}
		else if (buff[read_in] == '\n')
		{
			lf++;
		}
		else if (cr > 0 && lf > 0)
		{
			cr--;
			lf--;
		}
		read_in++;

		// read in full req, proceed
		if (cr == 2 && lf == 2)
		{
			break;
		}
		else if (cr > 2 || lf > 2 || read_in > SIZE - 1)
		{
			createSimpleCustomResponse("400 bad request");
			printf("400 bad request\n");
			return NULL;
		}
	}
	// parse the request
	parseRequest(buff, clientSocket);
	return NULL;
}

// parse a single request and create response based on header
void parseRequest(char buff[], int clientSocket)
{
	char buffcpy[SIZE]; // save a copy of buff
	memset(buffcpy, 0, SIZE);
	strncpy(buffcpy, buff, SIZE);
	char *first_line = getRequest1stLine(buff);
	if (strcmp(first_line, "") == 0)
	{
		fprintf(stderr, "getRequest1stLine failed\n");
		return;
	}

	char *filename = parseRequests1stLine(first_line, clientSocket);
	if (strcmp(filename, "") == 0)
	{
		fprintf(stderr, "parseRequests1stLine failed\n");
		return;
	}
	int resp_type = 2;

	char *if_mod_since_line = getRequestifModSinceLine(buffcpy);
	if (strcmp(if_mod_since_line, "") == 0)
	{
		fprintf(stderr, "getRequestifModSinceLine failed or did not find a If-Modified-Since header\n");
		resp_type = 1; // handles the later case for the line above
		if_mod_since_line = NULL;
	}
	char *if_mod_since_line_time = NULL;
	if (if_mod_since_line != NULL)
	{
		if_mod_since_line_time = getDateFromifModSinceHeaderLine(if_mod_since_line);
	}
	createResponse(getFilePath(filename), clientSocket, resp_type, if_mod_since_line_time);
	free(if_mod_since_line_time);
}

// get first line of a request based on delimeter
char *getRequest1stLine(char buff[])
{
	char *delim_req = "\r\n";
	char *token = strtok(buff, delim_req);
	return token;
}

// get line from header corresponding to if modified since
char *getRequestifModSinceLine(char buff[])
{
	char *delim_req = "\r\n";
	char *end_str;

	// skip the 1st line:
	char *token = strtok_r(buff, delim_req, &end_str);
	token = strtok_r(NULL, delim_req, &end_str);

	// find the line w/ If-Modified-Since header:
	char *current_attr = NULL;
	char *tokencpy = (char *)malloc(SIZE);
	memset(tokencpy, 0, SIZE);
	while (token != NULL)
	{
		// printf("> current token   : %s\n", token); // TODO: remove
		strncpy(tokencpy, token, SIZE);
		current_attr = getRequestLineAttr(tokencpy);
		if (strcmp(current_attr, "If-Modified-Since:") == 0)
		{
			printf("HTTP 1.0 conditional GET's attribute's line: %s\n", token);
			free(tokencpy);
			return token;
		}
		else
		{
			token = strtok_r(NULL, delim_req, &end_str);
		}
	}
	free(tokencpy);
	return "";
}

// the following function is meant for parsing any of a request's lines but the 1st line:
char *getRequestLineAttr(char *line)
{
	char *delim_line = " ";
	char *end_token;
	char *tokenn = strtok_r(line, delim_line, &end_token);
	return tokenn;
}

// get data corresponding to 'If-Modified-Since: '
char *getDateFromifModSinceHeaderLine(char *if_mod_since_line)
{
	char *if_mod_since_line_time = (char *)malloc(SIZE);
	int starting_index = 19;
	strncpy(if_mod_since_line_time, if_mod_since_line + starting_index, SIZE - starting_index); // skipping 'If-Modified-Since: '
	return if_mod_since_line_time;
}

// parse the first line of a client request
char *parseRequests1stLine(char *first_line, int clientSocket)
{
	// split first line of request
	char *token;
	char *delim_line = " ";
	char *filename = NULL;
	token = strtok(first_line, delim_line);
	int counter = 0;
	while (token != NULL)
	{
		switch (counter)
		{
		case 0:
			if (strcmp(token, "GET") != 0)
			{
				fprintf(stderr, "400 bad request - not GET\n");
				sendResponse(createSimpleCustomResponse("400 Bad Request"), strlen("400 Bad Request"), clientSocket);
				return "";
			}
			break;
		case 1:
			filename = token;
			break;
		case 2:

			if (strcmp(token, "HTTP/1.1") != 0 && strcmp(token, "HTTP/1.0") != 0)
			{
				fprintf(stderr, "400 bad request - wrong protocol\n");
				sendResponse(createSimpleCustomResponse("400 Bad Request"), strlen("400 Bad Request"), clientSocket);
				return "";
			}
			break;
		default:
			sendResponse(createSimpleCustomResponse("400 Bad Request"), strlen("400 Bad Request"), clientSocket);
			return "";
		}
		counter++;
		token = strtok(NULL, delim_line);
	}
	return filename;
}

// finds the path of the file we need to respond with
char *getFilePath(char *filename)
{
	char *rp = malloc(strlen(root_path) + strlen(filename));
	strcpy(rp, root_path);
	return strcat(rp, filename);
}

// get the MIME file type that the client is requesting
char *getFileType(char *filepath)
{
	char *ext = strrchr(filepath, '.');
	if (ext == NULL)
	{
		printf("filetype for response: error\n");
		return "error";
	}
	ext++;
	if (strcmp("html", ext) == 0)
	{
		printf("filetype for response: text/html\n");
		return "text/html";
	}
	else if (strcmp("txt", ext) == 0)
	{
		printf("filetype for response: text/plain\n");
		return "text/plain";
	}
	else if (strcmp("css", ext) == 0)
	{
		printf("filetype for response: text/css\n");
		return "text/css";
	}
	else if (strcmp("js", ext) == 0)
	{
		printf("filetype for response: text/javascript\n");
		return "text/javascript";
	}
	else if (strcmp("jpg", ext) == 0 || strcmp("jpeg", ext) == 0)
	{
		printf("filetype for response: image/jpeg\n");
		return "image/jpeg";
	}
	else
	{
		printf("filetype for response: error\n");
		return "error";
	}
}

// handle normal get requests (i.e. when resp_type = 1)
// and handle conditional get requests (i.e. when resp_type = 2)
// NOTE: later is when 'If-Modified-Since:' header is found':
void createResponse(char *filepath, int clientSocket, int resp_type, char *resp_d1)
{
	printf("filepath for response: %s\n", filepath); // TODO: remove
	if (checkFileExists(filepath) == -1)
	{
		fprintf(stderr, "filename provided via client's request not found\n");
		sendResponse(createSimpleCustomResponse("404 Not Found"), strlen("404 Not Found"), clientSocket);
		return;
	}
	if (strcmp(getFileType(filepath), "error") == 0)
	{
		fprintf(stderr, "filename provided via client's request leads to a file w/ a file type that's not accepted\n");
		sendResponse(createSimpleCustomResponse("400 Bad Request"), strlen("400 Bad Request"), clientSocket);
		return;
	}
	if (resp_type == 1 && resp_d1 == NULL)
	{
		// handle normal get requests:
		// continue;
	}
	else if (resp_type == 2 && resp_d1 != NULL)
	{
		// handle conditional get requests:
		char *last_mod_date_of_req_file = getLastModifiedDate(filepath);
		printf("> last_mod_date_of_req_file (in createResponse)                           : %s\n", last_mod_date_of_req_file); // TODO: remove
		struct tm last_mod_date_of_req_file_tm = strTime2tm(last_mod_date_of_req_file);
		printf(">> if_mod_since_line of client's request header's time (in createResponse): %s\n", resp_d1); // TODO: remove
		struct tm resp_d1_tm = strTime2tm(resp_d1);
		time_t time1 = mktime(&last_mod_date_of_req_file_tm);
		time_t time2 = mktime(&resp_d1_tm);
		if (time2 == -1)
		{ // conditional get case 3/3 (i.e. invalid conditional get attribute from user's request)
			// respond normally w/o content:
			char *resp = "200 OK";
			sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
			return;
		}
		double time_dif = difftime(time1, time2);
		free(last_mod_date_of_req_file);
		printf("determine conditional get case:\n"); // TODO: remove
		if (time_dif <= 0)
		{ // conditional get case 1/3 (i.e. last_mod_date_of_req_file_tm <= resp_d1_tm) (i.e. get condition not met)
			// respond informatively:
			char *resp = "304 Not Modified";
			sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
			return;
		}
		else
		{ // conditional get case 2/3 (i.e. last_mod_date_of_req_file_tm > resp_d1_tm) (i.e. get condition met)
		  // respond normally w/ content:
		  // printf("business as usual\n");
		  // continue;
		}
	}
	else
	{
		// handle requests that aren't valid:
		char *resp = "400 Bad Request";
		sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
		return;
	}

	int size = 0;
	char *body = getBody(filepath, &size);
	if (strcmp(body, "") == 0)
	{
		fprintf(stderr, "getBody failed\n");
		return;
	}

	char *header = getHeader(size, filepath);

	char temp[size + strlen(header)];
	strcpy(temp, header);
	memcpy(&temp[strlen(header)], body, size);

	sendResponse(temp, size + strlen(header), clientSocket);
	free(body);
	free(header);
	free(filepath);
}

// get the last modified date of file pointed to by filepath
char *getLastModifiedDate(char *filepath)
{
	struct stat attr;
	if (stat(filepath, &attr) == -1)
	{
		fprintf(stderr, "stat failed\n");
		exit(1);
	}
	char *t = (char *)malloc(29);
	time_t time = attr.st_mtime;
	strftime(t, 29, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&time));
	return t;
}

// convert a str of the form "%a, %d %b %Y %H:%M:%S GMT" to a struct tm:
struct tm strTime2tm(char *s)
{
	struct tm t = {0};
	// get day:
	int day = 10 * (s[5] - '0') + (s[6] - '0'); // for %d (2 chars)
	printf("1. day: %d\n", day);				// TODO: remove
	t.tm_mday = day;
	// get month:
	int month = strMonthAbbr2int(s); // for %b (3 chars)
	printf("2. month: %d\n", month); // TODO: remove
	t.tm_mon = month;
	// get year:
	int year = 1000 * (s[12] - '0') + 100 * (s[13] - '0') + 10 * (s[14] - '0') + (s[15] - '0'); // for %Y (4 chars)
	printf("3. year: %d\n", year);																// TODO: remove
	t.tm_year = year;
	// get hours:
	int hours = 10 * (s[17] - '0') + 1 * (s[18] - '0'); // for %H (2 chars)
	printf("4. hours: %d\n", hours);					// TODO: remove
	t.tm_hour = hours;
	// get minutes:
	int minutes = 10 * (s[20] - '0') + 1 * (s[21] - '0'); // for %M (2 chars)
	printf("5. minutes: %d\n", minutes);				  // TODO: remove
	t.tm_min = minutes;
	// get seconds:
	int seconds = 10 * (s[23] - '0') + 1 * (s[24] - '0'); // for %S (2 chars)
	printf("6. seconds: %d\n", seconds);				  // TODO: remove
	t.tm_sec = seconds;
	return t;
}

// convert string representation of month to integer
int strMonthAbbr2int(char *s)
{
	char months[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
	for (int j = 0; j < 12; j++)
	{
		if (s[8] == months[j][0] && s[9] == months[j][1] && s[10] == months[j][2])
		{
			return j + 1;
		}
	}
	return -1;
}

// WARNING: the following function breaks if passed in str is longer than 100 - 32 chars:
// creates the simplest response message using str
char *createSimpleCustomResponse(char *str)
{
	char *resp = (char *)malloc(100);
	snprintf(resp, 100, "HTTP/1.0 %s\r\nContent-Length: 0\r\n\r\n", str);
	return resp;
}

// sends response to client
void sendResponse(char *response, int size, int clientSocket)
{
	write(clientSocket, response, size + 32);
	close(clientSocket);
}

// created the header of a response corresponding to an ok request
char *getHeader(int length, char *filepath)
{
	printf("getting header\n");
	int header_length = 200;
	char *header = (char *)malloc(header_length);
	char *type = getFileType(filepath);
	char *lmd = getLastModifiedDate(filepath);
	char *cache_control = "60"; // seconds
	// TODO: get the file's last modified time/date here
	// snprintf(header, 100, "HTTP/1.0 200 ok\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", type, length);
	snprintf(header, header_length, "HTTP/1.0 200 ok\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: max-age=%s\r\nLast-Modified: %s\r\n\r\n", type, length, cache_control, lmd);
	return header;
}

// gets the body of file requested
char *getBody(char *filepath, int *size)
{
	printf("getting body\n");
	int n;
	int read = 0;
	FILE *fileData;
	if ((fileData = fopen(filepath, "rb")) == NULL)
	{
		fprintf(stderr, "fopen failed\n");
		return "";
	}

	char *responseData = (char *)malloc(SIZE);
	int curr_size = SIZE;

	memset(responseData, 0, SIZE);
	int temp = 50000;
	while ((n = fread(&responseData[read], 1, temp, fileData)) != 0)
	{
		read += n;
		if (curr_size - read < temp)
		{
			curr_size = curr_size + temp;
			responseData = (char *)realloc(responseData, curr_size);
			memset(&responseData[curr_size - temp], 0, temp);
		}
	}
	*size = read;
	return responseData;
}
