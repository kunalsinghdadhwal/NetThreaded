#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CLIENTS 15
#define MAX_BYTES 4096
#define MAX_ELEMENT_SIZE 10 * (1 << 10)
#define MAX_SIZE 200 * (1 << 20)

typedef struct cache_element
{
    char *data;
    int len;
    char *url;
    time_t lru_time_track;
    struct cache_element *next;
} cache_element;

int PORT = 8080;
int proxy_socketId;
int cache_size;

pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;

cache_element *head;

int sendErrorMessage(int socket, int status_code)
{
    char str[1024];
    char current_time[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(current_time, sizeof(current_time), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code)
    {
    case 400:
        snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", current_time);
        printf("400 Bad Request\n");
        send(socket, str, strlen(str), 0);
        break;

    case 403:
        snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", current_time);
        printf("403 Forbidden\n");
        send(socket, str, strlen(str), 0);
        break;

    case 404:
        snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", current_time);
        printf("404 Not Found\n");
        send(socket, str, strlen(str), 0);
        break;

    case 500:
        snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", current_time);
        // printf("500 Internal Server Error\n");
        send(socket, str, strlen(str), 0);
        break;

    case 501:
        snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", current_time);
        printf("501 Not Implemented\n");
        send(socket, str, strlen(str), 0);
        break;

    case 505:
        snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", current_time);
        printf("505 HTTP Version Not Supported\n");
        send(socket, str, strlen(str), 0);
        break;

    default:
        return -1;
    }

    return 1;
}

int checkHTTPversion(char *msg)
{
    int version = -1;

    if (strncmp(msg, "HTTP/1.1", 8) == 0)
    {
        version = 1;
    }
    else if (strncmp(msg, "HTTP/1.0", 8) == 0)
    {
        version = 1;
    }
    else
    {
        version = -1;
    }
    return version;
}

int connnectRemoteServer(char *host_addr, int port_num)
{
    int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (remoteSocket < 0)
    {
        printf("Error in creating your Socket\n");
        return -1;
    }

    struct hostent *host = gethostbyname(host_addr);
    if (host == NULL)
    {
        fprintf(stderr, "No such hosts exist\n");
        return -1;
    }
    struct sockaddr_in server_addr;
    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);

    bcopy((char *)host->h_addr, (char *)&server_addr.sin_addr.s_addr, host->h_length);

    if (connect(remoteSocket, (struct sockaddr *)&server_addr, (size_t)sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error in Connecting\n");
        return -1;
    }
    return remoteSocket;
}

cache_element *find_cache_element(char *url)
{
    cache_element *site = NULL;
    int tmp_lock_val = pthread_mutex_lock(&lock);
    printf("Remove Cache Lock Acquired:- %d\n", tmp_lock_val);
    if (head != NULL)
    {
        site = head;
        while (site != NULL)
        {
            if (!strcmp(site->url, url))
            {
                printf("LRU time track before: %ld\n", site->lru_time_track);
                printf("URL found\n");
                site->lru_time_track = time(NULL);
                printf("LRU time track After: %ld\n", site->lru_time_track);
                break;
            }
            site = site->next;
        }
    }
    else
    {
        printf("Url not found\n");
    }
    tmp_lock_val = pthread_mutex_unlock(&lock);
    printf("Lock is removed\n");
    return site;
}

void remove_cache_element()
{
    cache_element *p;
    cache_element *q;
    cache_element *tmp;

    pthread_mutex_lock(&lock);
    printf("Remove element lock is acquired\n");
    if (head != NULL)
    {
        for (q = head, p = head, tmp = head; q->next != NULL; q = q->next)
        {
            if ((q->next)->lru_time_track < (tmp->lru_time_track))
            {
                tmp = q->next;
                p = q;
            }
        }
        if (tmp == head)
        {
            head = head->next;
        }
        else
        {
            p->next = tmp->next;
        }

        cache_size = cache_size - (tmp->len) - sizeof(cache_element) - strlen(tmp->url) - 1;
        free(tmp->data);
        free(tmp->url);
        free(tmp);
    }
    pthread_mutex_unlock(&lock);
    printf("Remove Element Lock removed\n");
    return;
}

int add_cache_element(char *data, int size, char *url)
{
    int tmp_lock_val = pthread_mutex_lock(&lock);
    printf("Add cache Lock Accquired: %d\n", tmp_lock_val);
    int element_size = size + 1 + strlen(url) + sizeof(cache_element);
    if (element_size > MAX_ELEMENT_SIZE)
    {
        tmp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache Lock is unlocked\n");
        return 0;
    }
    else
    {
        while (cache_size + element_size > MAX_SIZE)
        {
            remove_cache_element();
        }
        cache_element *element = (cache_element *)calloc(1, sizeof(cache_element));
        // Fix: Allocate memory for data before copying
        element->data = (char *)calloc(size + 1, sizeof(char));
        strcpy(element->data, data);
        element->url = (char *)calloc(strlen(url) + 1, sizeof(char));
        strcpy(element->url, url);
        element->lru_time_track = time(NULL);
        element->next = head;
        element->len = size;
        head = element;
        cache_size += element_size;
        tmp_lock_val = pthread_mutex_unlock(&lock);
        printf("Add cache lock unlocked\n");
        return 1;
    }
    return 0;
}

int handle_request(int clientSocketId, ParsedRequest *request, char *tempReq)
{
    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    // Fix: Use strcpy then strcat properly
    strcpy(buffer, "GET ");
    strcat(buffer, request->path);
    strcat(buffer, " ");
    strcat(buffer, request->version);
    strcat(buffer, "\r\n");
    size_t len = strlen(buffer);

    if (ParsedHeader_set(request, "Connection", "close") < 0)
    {
        printf("Set Connection header key not working\n");
    }

    if (ParsedHeader_get(request, "Host") == NULL)
    {
        if (ParsedHeader_set(request, "Host", request->host) < 0)
        {
            printf("Set Host header key not working\n");
        }
    }
    if (ParsedRequest_unparse_headers(request, buffer + len, (size_t)MAX_BYTES - len) < 0)
    {
        printf("Unparse headers failed\n");
    }

    int server_port = 80;
    if (request->port != NULL)
    {
        server_port = atoi(request->port);
    }
    int remoteSocketId = connnectRemoteServer(request->host, server_port);
    if (remoteSocketId < 0)
    {
        return -1;
    }
    int bytes_send = send(remoteSocketId, buffer, strlen(buffer), 0);
    bzero(buffer, MAX_BYTES);
    bytes_send = recv(remoteSocketId, buffer, MAX_BYTES - 1, 0);
    char *temp_buffer = (char *)malloc(sizeof(char) * MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while (bytes_send > 0)
    {
        bytes_send = send(clientSocketId, buffer, bytes_send, 0);
        for (size_t i = 0; i < bytes_send / sizeof(char); i++)
        {
            temp_buffer[temp_buffer_index] = buffer[i];
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char *)realloc(temp_buffer, temp_buffer_size);
        if (bytes_send < 0)
        {
            perror("Error in sending data to client\n");
            break;
        }
        bzero(buffer, MAX_BYTES);
        bytes_send = recv(remoteSocketId, buffer, MAX_BYTES - 1, 0);
    }
    temp_buffer[temp_buffer_index] = '\0';
    free(buffer);
    add_cache_element(temp_buffer, strlen(temp_buffer), tempReq);
    free(temp_buffer);
    close(remoteSocketId);
    return 0;
}

void *thread_fn(void *socketNew)
{
    sem_wait(&semaphore);
    int semaphore_value;
    sem_getvalue(&semaphore, &semaphore_value);
    printf("Value of Semaphore:- %d\n", semaphore_value);

    // Fix: Get socket value and free the allocated pointer
    int *socketPtr = (int *)socketNew;
    int socket = *socketPtr;
    free(socketPtr);

    int bytes_send_client, len;

    char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));
    bzero(buffer, MAX_BYTES);
    bytes_send_client = recv(socket, buffer, MAX_BYTES - 1, 0);

    while (bytes_send_client > 0)
    {
        len = strlen(buffer);
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {
            bytes_send_client = recv(socket, buffer + len, MAX_BYTES - len - 1, 0);
        }
        else
        {
            break;
        }
    }

    printf("Received request:\n%s\n", buffer);

    char *tempReq = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
    strcpy(tempReq, buffer);

    // Check for CONNECT method (HTTPS - not supported)
    if (strncmp(buffer, "CONNECT", 7) == 0)
    {
        printf("CONNECT method not supported (HTTPS)\n");
        const char *response = "HTTP/1.1 501 Not Implemented\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: close\r\n\r\n"
                               "<html><body><h1>501 Not Implemented</h1>"
                               "<p>HTTPS is not supported. Use http:// URLs only.</p></body></html>";
        send(socket, response, strlen(response), 0);
    }
    // Handle URL-in-path format: GET /http://example.com
    else if (strstr(buffer, "GET /http://") != NULL)
    {
        char *newBuffer = (char *)calloc(MAX_BYTES, sizeof(char));
        char *urlStart = strstr(buffer, "/http://") + 1; // Skip leading /
        char *urlEnd = strstr(urlStart, " ");

        if (urlEnd != NULL)
        {
            int urlLen = urlEnd - urlStart;
            char *versionStart = urlEnd + 1;
            char *versionEnd = strstr(versionStart, "\r\n");

            if (versionEnd != NULL)
            {
                int versionLen = versionEnd - versionStart;

                // Copy URL to check if it needs a trailing slash
                char *urlCopy = (char *)calloc(urlLen + 2, sizeof(char));
                strncpy(urlCopy, urlStart, urlLen);
                urlCopy[urlLen] = '\0';

                // Check if URL has a path (look for / after http://host)
                char *hostStart = urlCopy + 7; // Skip "http://"
                char *pathStart = strchr(hostStart, '/');

                strcpy(newBuffer, "GET ");
                strncat(newBuffer, urlStart, urlLen);

                // Add trailing slash if no path exists
                if (pathStart == NULL)
                {
                    strcat(newBuffer, "/");
                }

                strcat(newBuffer, " ");
                strncat(newBuffer, versionStart, versionLen);
                strcat(newBuffer, "\r\n");

                // Extract host from URL for Host header
                char *hostEnd = strchr(hostStart, '/');
                char *portStart = strchr(hostStart, ':');
                char targetHost[256] = {0};

                if (portStart != NULL && (hostEnd == NULL || portStart < hostEnd))
                {
                    // Host has port
                    int hostPortLen = (hostEnd != NULL) ? (hostEnd - hostStart) : strlen(hostStart);
                    strncpy(targetHost, hostStart, hostPortLen);
                }
                else if (hostEnd != NULL)
                {
                    strncpy(targetHost, hostStart, hostEnd - hostStart);
                }
                else
                {
                    strcpy(targetHost, hostStart);
                }

                // Add proper Host header
                strcat(newBuffer, "Host: ");
                strcat(newBuffer, targetHost);
                strcat(newBuffer, "\r\n");

                // Copy remaining headers, skipping the original Host header
                char *headerStart = versionEnd + 2; // Skip \r\n after version
                while (headerStart && *headerStart && !(headerStart[0] == '\r' && headerStart[1] == '\n'))
                {
                    char *headerEnd = strstr(headerStart, "\r\n");
                    if (headerEnd == NULL)
                        break;

                    // Skip Host header (we already added it)
                    if (strncasecmp(headerStart, "Host:", 5) != 0)
                    {
                        strncat(newBuffer, headerStart, headerEnd - headerStart + 2);
                    }
                    headerStart = headerEnd + 2;
                }
                strcat(newBuffer, "\r\n");

                free(urlCopy);

                bzero(buffer, MAX_BYTES);
                strcpy(buffer, newBuffer);

                free(tempReq);
                tempReq = (char *)malloc(strlen(buffer) + 1);
                strcpy(tempReq, buffer);
            }
        }
        free(newBuffer);

        // Process the transformed request
        goto process_request;
    }
    // Reject /https:// URLs
    else if (strstr(buffer, "GET /https://") != NULL)
    {
        printf("HTTPS URLs not supported\n");
        const char *response = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: close\r\n\r\n"
                               "<html><body><h1>HTTPS Not Supported</h1>"
                               "<p>Use http:// URLs only. Example: http://localhost:8080/http://example.com</p></body></html>";
        send(socket, response, strlen(response), 0);
    }
    // Check for valid proxy request
    else if (bytes_send_client > 0 && strstr(buffer, "http://") == NULL)
    {
        printf("Not a valid proxy request\n");
        const char *response = "HTTP/1.1 400 Bad Request\r\n"
                               "Content-Type: text/html\r\n"
                               "Connection: close\r\n\r\n"
                               "<html><body><h1>Proxy Server</h1>"
                               "<p>Usage: http://localhost:8080/http://example.com</p>"
                               "<p>Note: Only HTTP (not HTTPS) is supported.</p></body></html>";
        send(socket, response, strlen(response), 0);
    }
    else
    {
    process_request:;
        cache_element *temp = find_cache_element(tempReq);
        if (temp != NULL)
        {
            // ...existing cache hit code...
            int size = temp->len;
            int pos = 0;
            char response[MAX_BYTES];
            while (pos < size)
            {
                bzero(response, MAX_BYTES);
                int chunk = (size - pos < MAX_BYTES) ? (size - pos) : MAX_BYTES;
                memcpy(response, temp->data + pos, chunk);
                send(socket, response, chunk, 0);
                pos += chunk;
                printf("Data retrieved from cache\n");
            }
        }
        else if (bytes_send_client > 0)
        {
            len = strlen(buffer);
            ParsedRequest *request = ParsedRequest_create();
            if (ParsedRequest_parse(request, buffer, len) < 0)
            {
                printf("Parsing Failed for request of length %d\n", len);
                sendErrorMessage(socket, 400);
            }
            else
            {
                bzero(buffer, MAX_BYTES);
                if (!strcmp(request->method, "GET"))
                {
                    if (request->host && request->path && checkHTTPversion(request->version) == 1)
                    {
                        bytes_send_client = handle_request(socket, request, tempReq);
                        if (bytes_send_client == -1)
                        {
                            sendErrorMessage(socket, 500);
                        }
                    }
                    else
                    {
                        sendErrorMessage(socket, 500);
                    }
                }
                else
                {
                    printf("The Server can only handle GET method\n");
                }
            }
            ParsedRequest_destroy(request);
        }
        else if (bytes_send_client < 0)
        {
            printf("Error in receiving from Client\n");
            perror("recv");
        }
        else if (bytes_send_client == 0)
        {
            printf("Client Disconnected\n");
        }
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);

    sem_getvalue(&semaphore, &semaphore_value);
    printf("Value of Semaphore:- %d\n", semaphore_value);
    free(tempReq);
    return NULL;
}

int main(int argc, char const *argv[])
{
    // ...existing code until the while loop...
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;
    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if (argc == 2)
    {
        PORT = atoi(argv[1]);
    }
    printf("Starting Proxy Server at port: %d\n", PORT);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
    if (proxy_socketId < 0)
    {
        perror("Failed to create a Socket");
        exit(1);
    }
    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
    {
        perror("setSockOpt FAILED!!");
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Port is not available");
        exit(1);
    }
    printf("Binding on Port %d\n", PORT);
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);
    if (listen_status < 0)
    {
        perror("Error Listening");
        exit(1);
    }
    int socketId_count = 0;

    while (1)
    {
        bzero((char *)&client_addr, sizeof(client_addr));
        client_len = sizeof(client_addr);
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len);
        if (client_socketId < 0)
        {
            perror("Not able to Connect to Client");
            exit(1);
        }

        struct sockaddr_in *client_pt = (struct sockaddr_in *)&client_addr;
        struct in_addr ip_addr = client_pt->sin_addr;
        char str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip_addr, str, INET_ADDRSTRLEN);
        printf("Client Successfully connected with port number %d and ip address %s\n", ntohs(client_addr.sin_port), str);

        // Fix: Allocate memory for socket to avoid race condition
        int *socketPtr = (int *)malloc(sizeof(int));
        *socketPtr = client_socketId;

        pthread_create(&tid[socketId_count], NULL, thread_fn, (void *)socketPtr);
        socketId_count++;

        if (socketId_count >= MAX_CLIENTS)
        {
            socketId_count = 0;
        }
    }
    close(proxy_socketId);
    return 0;
}
