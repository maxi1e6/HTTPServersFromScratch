#include "sha256.h"
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

// processes block of requests in a pipelined fashion
void *processPipelined(void *pClient)
{
    int clientSocket = *((int *)pClient);
    free(pClient);
    char buff[SIZE];
    memset(buff, 0, SIZE);
    char *request_end;
    char *request_start = buff;
    char *new_request;
    char *delim = "\r\n\r\n";
    int req_size;
    while (read(clientSocket, buff, SIZE) > 0)
    {
        while ((request_end = strstr(request_start, delim)) != NULL)
        {
            req_size = request_end - request_start + 1;
            new_request = (char *)malloc(sizeof(char) * req_size);
            memset(new_request, 0, req_size);
            strncpy(new_request, request_start, req_size);
            parseRequest(new_request, clientSocket);
            request_start = request_end + strlen(delim);
            free(new_request);
        }
    }
    return NULL;
}

// parse a single request and create response based on header
void parseRequest(char buff[], int clientSocket)
{
    char buffcpy[SIZE];
    memset(buffcpy, 0, SIZE);
    strncpy(buffcpy, buff, SIZE); // save a copy of buff

    char *first_line = getRequest1stLine(buffcpy);
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

    char *filenamecpy = malloc(strlen(filename) + 1);
    strcpy(filenamecpy, filename);

    memset(buffcpy, 0, SIZE);
    strncpy(buffcpy, buff, SIZE); // save a copy of buff
    int if_mod_since_resp_type = 2;

    char *if_mod_since_line = getRequestifModSinceLine(buffcpy);
    if (strcmp(if_mod_since_line, "") == 0)
    {
        fprintf(stderr, "getRequestifModSinceLine failed or did not find a If-Modified-Since header\n");
        if_mod_since_resp_type = 1; // handles the later case for the line above
        if_mod_since_line = NULL;
    }

    char *if_mod_since_line_time = NULL;
    if (if_mod_since_line != NULL)
    {
        if_mod_since_line_time = getDateFromifModSinceHeaderLine(if_mod_since_line);
    }

    memset(buffcpy, 0, SIZE);
    strncpy(buffcpy, buff, SIZE); // save a copy of buff
    int if_unmod_since_resp_type = 2;

    char *if_unmod_since_line = getRequestifUnModSinceLine(buffcpy);
    if (strcmp(if_unmod_since_line, "") == 0)
    {
        fprintf(stderr, "getRequestifUnModSinceLine failed or did not find a If-Unmodified-Since header\n");
        if_unmod_since_resp_type = 1; // handles the later case for the line above
        if_unmod_since_line = NULL;
    }
    char *if_unmod_since_line_time = NULL;
    if (if_unmod_since_line != NULL)
    {
        if_unmod_since_line_time = getDateFromifUnModSinceHeaderLine(if_unmod_since_line);
    }

    memset(buffcpy, 0, SIZE);
    strncpy(buffcpy, buff, SIZE); // save a copy of buff
    int if_match_resp_type = 2;

    char *if_match_line = getRequestifMatchLine(buffcpy);
    if (strcmp(if_match_line, "") == 0)
    {
        fprintf(stderr, "getRequestifMatchLine failed or did not find a If-Match header\n");
        if_match_resp_type = 1; // handles the later case for the line above
        if_match_line = NULL;
    }

    char *if_match_line_sum = NULL;
    char *if_match_line_sumcpy = NULL;
    if (if_match_line != NULL)
    {
        if_match_line_sum = getSHA256SumFromifMatchHeaderLine(if_match_line);
        if_match_line_sumcpy = malloc(strlen(if_match_line_sum) + 1);
        strcpy(if_match_line_sumcpy, if_match_line_sum);
    }

    memset(buffcpy, 0, SIZE);
    strncpy(buffcpy, buff, SIZE); // save a copy of buff
    int if_none_match_resp_type = 2;
    char *if_none_match_line = getRequestifNoneMatchLine(buffcpy);
    if (strcmp(if_none_match_line, "") == 0)
    {
        fprintf(stderr, "getRequestifNoneMatchLine failed or did not find a If-None-Match header\n");
        if_none_match_resp_type = 1; // handles the later case for the line above
        if_none_match_line = NULL;
    }
    char *if_none_match_line_sum = NULL;
    char *if_none_match_line_sumcpy = NULL;
    if (if_none_match_line != NULL)
    {
        if_none_match_line_sum = getSHA256SumFromifMatchHeaderLine(if_none_match_line);
        if_none_match_line_sumcpy = malloc(strlen(if_none_match_line_sum) + 1);
        strcpy(if_none_match_line_sumcpy, if_none_match_line_sum);
    }
    printf("if_none_match_resp_type: %d\n", if_none_match_resp_type);

    createResponsePersistent(getFilePath(filenamecpy), clientSocket, if_mod_since_resp_type, if_mod_since_line_time, if_unmod_since_resp_type, if_unmod_since_line_time, if_match_resp_type, if_match_line_sumcpy, if_none_match_resp_type, if_none_match_line_sumcpy); // hope it works!
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

// get line from header corresponding to if unmodified since
char *getRequestifUnModSinceLine(char buff[])
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
        strncpy(tokencpy, token, SIZE);
        current_attr = getRequestLineAttr(tokencpy);

        if (strcmp(current_attr, "If-Unmodified-Since:") == 0)
        {
            printf("HTTP 1.1 conditional GET's attribute's line: %s\n", token);
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

// get line from header corresponding to if match
char *getRequestifMatchLine(char buff[])
{
    char *delim_req = "\r\n";
    char *end_str;

    // skip the 1st line:
    char *token = strtok_r(buff, delim_req, &end_str);
    token = strtok_r(NULL, delim_req, &end_str);

    char *current_attr = NULL;
    char *tokencpy = (char *)malloc(SIZE);
    memset(tokencpy, 0, SIZE);
    while (token != NULL)
    {
        strncpy(tokencpy, token, SIZE);
        current_attr = getRequestLineAttr(tokencpy);
        if (strcmp(current_attr, "If-Match:") == 0)
        {
            printf("HTTP 1.1 conditional GET's attribute's line: %s\n", token);
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

// get line from header corresponding to if none match
char *getRequestifNoneMatchLine(char buff[])
{
    char *delim_req = "\r\n";
    char *end_str;

    char *token = strtok_r(buff, delim_req, &end_str);
    token = strtok_r(NULL, delim_req, &end_str);

    char *current_attr = NULL;
    char *tokencpy = (char *)malloc(SIZE);
    memset(tokencpy, 0, SIZE);
    while (token != NULL)
    {
        strncpy(tokencpy, token, SIZE);
        current_attr = getRequestLineAttr(tokencpy);

        if (strcmp(current_attr, "If-None-Match:") == 0)
        {
            printf("HTTP 1.1 conditional GET's attribute's line: %s\n", token);
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

// get the date corresponding to the if modified since header
char *getDateFromifModSinceHeaderLine(char *if_mod_since_line)
{
    char *if_mod_since_line_time = (char *)malloc(SIZE);
    int starting_index = 19;
    strncpy(if_mod_since_line_time, if_mod_since_line + starting_index, SIZE - starting_index); // skipping 'If-Modified-Since: '
    return if_mod_since_line_time;
}

// get the date corresponding to the if unmodified since header
char *getDateFromifUnModSinceHeaderLine(char *if_unmod_since_line)
{
    char *if_unmod_since_line_time = (char *)malloc(SIZE);
    int starting_index = 21;
    strncpy(if_unmod_since_line_time, if_unmod_since_line + starting_index, SIZE - starting_index); // skipping 'If-Unmodified-Since: '
    return if_unmod_since_line_time;
}

// get the SHA256Sum corresponding to the if match header
char *getSHA256SumFromifMatchHeaderLine(char *if_match_since_line)
{
    char *delim_req = "\"";
    char *token = strtok(if_match_since_line, delim_req);
    token = strtok(NULL, delim_req);
    return token;
}

// parse the first line of a client request
char *parseRequests1stLine(char *first_line, int clientSocket)
{
    // split first line of request
    char *token;
    char *delim_line = " ";
    char *filename = NULL;
    token = strtok(first_line, delim_line); // GET / HTTP/1.1
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

// 1.
// handle normal get requests (i.e. when if_mod_since_resp_type = 1)
// and handle conditional get requests (i.e. when if_mod_since_resp_type = 2)
// NOTE: later is when 'If-Modified-Since:' header is found':
// 2.
// handle get requests w/o 'If-Unmodified-Since:' header (i.e. when if_unmod_since_resp_type = 1)
// and handle get requests w/ 'If-Unmodified-Since:' header (i.e. when if_unmod_since_resp_type = 2)
// 3.
// handle get requests w/o 'If-Match:' header (i.e. when if_unmod_since_resp_type = 1)
// and handle get requests w/ 'If-Match:' header (i.e. when if_unmod_since_resp_type = 2)
// NOTE: this function assumes 'If-Modified-Since:''s value is a date w/ a specific format, and nothing else
void createResponsePersistent(char *filepath, int clientSocket, int if_mod_since_resp_type, char *resp_d1, int if_unmod_since_resp_type, char *resp_d2, int if_match_resp_type, char *resp_d3, int if_none_match_resp_type, char *resp_d4)
{
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

    int if_none_match_status;
    int if_none_match_flag = 0;
    if (if_none_match_resp_type == 2 && resp_d4 != NULL)
    {
        if_none_match_status = createResponseForifNoneMatch(filepath, clientSocket, if_none_match_resp_type, resp_d4);
        if (if_none_match_status == 0)
        {
            if_mod_since_resp_type = 1;
            resp_d1 = NULL;
        }
        else if (if_none_match_status == 400)
        {
            char *resp = "400 Bad Request";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else if (if_none_match_status == 412)
        {
            if_none_match_flag = 1;
        }
        else
        {
            return;
        }
    }

    int if_unmod_since_status;
    int if_match_status;
    if (if_mod_since_resp_type == 1 && resp_d1 == NULL)
    {
        if_unmod_since_status = createResponseForifUnModSince(filepath, clientSocket, if_unmod_since_resp_type, resp_d2);
        if (if_unmod_since_status == 0)
        {
        }
        else if (if_unmod_since_status == 200)
        {
            if_match_status = createResponseForifMatch(filepath, clientSocket, if_match_resp_type, resp_d3);
            if (if_match_status == 0)
            {
            }
            else if (if_match_status == 400)
            {
                char *resp = "400 Bad Request";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else if (if_match_status == 412)
            {
                char *resp = "412 Precondition Failed";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else
            {
                return;
            }
            char *resp = "200 OK";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else if (if_unmod_since_status == 400)
        {
            char *resp = "400 Bad Request";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else if (if_unmod_since_status == 412)
        {
            if ((if_match_resp_type == 2) && (strcmp(resp_d3, "*") == 0))
            {
            }
            else
            {
                char *resp = "412 Precondition Failed";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
        }
        else
        {
            return;
        }
        if_match_status = createResponseForifMatch(filepath, clientSocket, if_match_resp_type, resp_d3);
        if (if_match_status == 0)
        {
        }
        else if (if_match_status == 400)
        {
            char *resp = "400 Bad Request";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else if (if_match_status == 412)
        {
            char *resp = "412 Precondition Failed";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else
        {
            return;
        }
    }
    else if (if_mod_since_resp_type == 2 && resp_d1 != NULL)
    {
        char *last_mod_date_of_req_file = getLastModifiedDate(filepath);
        struct tm last_mod_date_of_req_file_tm = strTime2tm(last_mod_date_of_req_file);
        struct tm resp_d1_tm = strTime2tm(resp_d1);
        time_t time1 = mktime(&last_mod_date_of_req_file_tm);
        time_t time2 = mktime(&resp_d1_tm);
        if (time2 == -1)
        {
            char *resp = "200 OK";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        double time_dif = difftime(time1, time2);
        free(last_mod_date_of_req_file);
        if (time_dif <= 0)
        {
            char *resp = "304 Not Modified";
            sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
            return;
        }
        else
        {
            if (if_none_match_flag == 1)
            {
                char *resp = "412 Precondition Failed";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            if_unmod_since_status = createResponseForifUnModSince(filepath, clientSocket, if_unmod_since_resp_type, resp_d2);
            if (if_unmod_since_status == 0)
            {
            }
            else if (if_unmod_since_status == 200)
            {
                if_match_status = createResponseForifMatch(filepath, clientSocket, if_match_resp_type, resp_d3);
                if (if_match_status == 0)
                {
                }
                else if (if_match_status == 400)
                {
                    char *resp = "400 Bad Request";
                    sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                    return;
                }
                else if (if_match_status == 412)
                {
                    char *resp = "412 Precondition Failed";
                    sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                    return;
                }
                else
                {
                    return;
                }
                char *resp = "200 OK";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else if (if_unmod_since_status == 400)
            {
                char *resp = "400 Bad Request";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else if (if_unmod_since_status == 412)
            {
                if ((if_match_resp_type == 2) && (strcmp(resp_d3, "*") == 0))
                {
                }
                else
                {
                    char *resp = "412 Precondition Failed";
                    sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                    return;
                }
            }
            else
            {
                return;
            }
            if_match_status = createResponseForifMatch(filepath, clientSocket, if_match_resp_type, resp_d3);
            if (if_match_status == 0)
            {
            }
            else if (if_match_status == 400)
            {
                char *resp = "400 Bad Request";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else if (if_match_status == 412)
            {
                char *resp = "412 Precondition Failed";
                sendResponse(createSimpleCustomResponse(resp), strlen(resp), clientSocket);
                return;
            }
            else
            {
                return;
            }
        }
    }
    else
    {
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

// create a response based a clients' request's 'If-Unmodified-Since: ' header field, where:
// the following function returns the status code of the response it would send
// if it were alone and 0 otherwise,
// and assumes 'If-Unmodified-Since:''s value is a date w/ a specific format, and nothing else:
int createResponseForifUnModSince(char *filepath, int clientSocket, int if_unmod_since_resp_type, char *resp_d2)
{
    if (if_unmod_since_resp_type == 1 && resp_d2 == NULL)
    {
        return 0;
    }
    else if (if_unmod_since_resp_type == 2 && resp_d2 != NULL)
    {
        char *last_mod_date_of_req_file = getLastModifiedDate(filepath);
        struct tm last_mod_date_of_req_file_tm = strTime2tm(last_mod_date_of_req_file);
        struct tm resp_d2_tm = strTime2tm(resp_d2);
        time_t time1 = mktime(&last_mod_date_of_req_file_tm);
        time_t time2 = mktime(&resp_d2_tm);
        if (time2 == -1)
        {
            return 200;
        }
        double time_dif = difftime(time1, time2);
        free(last_mod_date_of_req_file);
        if (time_dif < 0)
        {
            return 0;
        }
        else
        {
            return 412;
        }
    }
    else
    {
        return 400;
    }
}

// create a response based a clients' request's 'If-Match: ' header field, where:
// the following function returns the status code of the response it would send
// if it were alone and 0 otherwise,
// and assumes 'If-Match:''s value is a '*' or sha256sum, and nothing else:
int createResponseForifMatch(char *filepath, int clientSocket, int if_match_resp_type, char *resp_d3)
{
    if (if_match_resp_type == 1 && resp_d3 == NULL)
    {
        return 0;
    }
    else if (if_match_resp_type == 2 && resp_d3 != NULL)
    {
        printf("createResponseForifMatch's resp_d3: %s\n", resp_d3);
        if (strcmp(resp_d3, "*") != 0)
        {
            char *sha256sum = computeSHA256Sum(filepath);
            printf("> > computed sha256sum for requested file at server: %s\n", sha256sum);
            printf("> > sha256sum found in client's request            : %s\n", resp_d3);
            if (strcmp(sha256sum, resp_d3) != 0)
            {
                return 412;
            }
            else
            {
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 400;
    }
}

// create a response based a clients' request's 'If-None-Match: ' header field, where:
// the following function returns the status code of the response it would send
// if it were alone and 0 otherwise,
// and assumes 'If-None-Match:''s value is a '*' or sha256sum, and nothing else:
int createResponseForifNoneMatch(char *filepath, int clientSocket, int if_none_match_resp_type, char *resp_d4)
{
    if (if_none_match_resp_type == 2 && resp_d4 != NULL)
    {
        printf("createResponseForifNoneMatch's resp_d4: %s\n", resp_d4);
        if (strcmp(resp_d4, "*") != 0)
        {
            char *sha256sum = computeSHA256Sum(filepath);
            printf("> > computed sha256sum for requested file at server: %s\n", sha256sum);
            printf("> > sha256sum found in client's request            : %s\n", resp_d4);
            if (strcmp(sha256sum, resp_d4) != 0)
            {
                return 0;
            }
            else
            {
                return 412;
            }
        }
        else
        {
            return 412;
        }
    }
    else
    {
        return 400;
    }
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
    t.tm_mday = day;
    // get month:
    int month = strMonthAbbr2int(s); // for %b (3 chars)
    t.tm_mon = month;
    // get year:
    int year = 1000 * (s[12] - '0') + 100 * (s[13] - '0') + 10 * (s[14] - '0') + (s[15] - '0'); // for %Y (4 chars)
    t.tm_year = year;
    // get hours:
    int hours = 10 * (s[17] - '0') + 1 * (s[18] - '0'); // for %H (2 chars)
    t.tm_hour = hours;
    // get minutes:
    int minutes = 10 * (s[20] - '0') + 1 * (s[21] - '0'); // for %M (2 chars)
    t.tm_min = minutes;
    // get seconds:
    int seconds = 10 * (s[23] - '0') + 1 * (s[24] - '0'); // for %S (2 chars)
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
    // close(clientSocket);
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
    int n;
    int read = 0;
    FILE *fileData;
    if ((fileData = fopen(filepath, "rb")) == NULL)
    {
        fprintf(stderr, "fopen failed\n");
        // exit(1); should not exit
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