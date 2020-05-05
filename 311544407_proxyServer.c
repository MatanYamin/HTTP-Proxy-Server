//Matan Yamin, ID: 311544407
#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#define BUFLEN 10
#define BUF 10

char **filter = NULL;
char err[1024];
int filterRows = 0;
void freeMe(char **something);
int response(void *arg);
void checkArgs(int argc, char *argv[]);
void errorMess(int num, char err[], int sock);
void getFIlter(char *path);
int findPort(char* input);
char* findHost(char* input);
int inputsplit(char *input, int tempSoc);

int main(int argc, char *argv[]){
    checkArgs(argc, argv);       //check if parametnncers is valid
    getFIlter(argv[4]);          //read from filter file
    int port = atoi(argv[1]);    //will be the port
    int poolSize = atoi(argv[2]);  //num of threads
    int maxNumOfReq = atoi(argv[3]); //num of jobs
    threadpool *tp = create_threadpool(poolSize);  //creates tp
    if (tp == NULL){
        freeMe(filter);
        return 0; 
    }
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) //creating our socket
    {
        freeMe(filter);
        destroy_threadpool(tp);
        perror("error: socket() failed"); //check failure
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
    {
        destroy_threadpool(tp);
        freeMe(filter);
        close(sockfd);
        perror("error: bind() failed"); //check if it fails
        exit(EXIT_FAILURE);
    }
    if (listen(sockfd, 5) == -1)
    {
        destroy_threadpool(tp);
        freeMe(filter);
        close(sockfd);
        perror("error: listen() failed");
        exit(EXIT_FAILURE);
    }
    int *requestFd = (int *)malloc(sizeof(int) * maxNumOfReq); //creating an fd array to use for the accept
    if (requestFd == NULL)
    { //check if it fails
        destroy_threadpool(tp);
        freeMe(filter);
        close(sockfd);
        printf("Error. Allocation was unsuccessful.\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i<maxNumOfReq; i++)
    { //for every thread we will send a func to handle with
        requestFd[i] = accept(sockfd, NULL, NULL);
        if (requestFd[i] == -1)
        { //check if it fails
            continue;
        }
        dispatch(tp, response, (void *)&requestFd[i]); //doing dispath for every fd
    }
    free(requestFd);
    destroy_threadpool(tp);
    freeMe(filter);
    close(sockfd);
    return 0;
}

int response(void *arg){
    char buffer[BUFLEN];
    int rc;
    char *newBuf;
    int tempSoc = *((int *)arg);
    newBuf = (char *)malloc(sizeof(char));//will cpy to later
    if (newBuf == NULL)
    {
        freeMe(filter);
        printf("Error. Allocation was unsuccessful.\n");
        return -1;
    }
    newBuf[0] = '\0';
    while (1)
    { //read
        if ((rc = read(tempSoc, buffer, BUFLEN)) == 0)//read from
        {
            break;
        }
        else if (rc > 0)
        {
            newBuf = realloc(newBuf, strlen(newBuf) + rc +1);
            if (newBuf == NULL)
            {
                freeMe(filter);
                printf("Error. Reallocation was unsuccessful.\n");
                return -1;
            }
            strncat(newBuf, buffer, rc); //add to newBuf the new buffer
        }
        else
        {
            freeMe(filter);
            free(newBuf);
            perror("error: read() failed"); //check failure
            return -1;
        }
        if (strstr(newBuf, "\r\n\r\n") != NULL)
        { //help us to know when we arr done
            break;
        }
    }
    char *tempNewBuf = (char *)malloc(sizeof(char) * strlen(newBuf) + 1);
    if (tempNewBuf == NULL)
    {
        freeMe(filter);
        free(newBuf);
        return -1;
    }
    strncpy(tempNewBuf, newBuf, strlen(newBuf) + 1);
    inputsplit(tempNewBuf, tempSoc);
    free(newBuf);
    free(tempNewBuf);
    return 0;
}

int inputsplit(char *input, int tempSoc)//will help us to locate problems and send to write
{
    char *newInput = (char *)malloc(sizeof(char) * strlen(input) + 1);
    if (newInput == NULL)
    {
        freeMe(filter);
        printf("Error. Allocation was unsuccessful.\n");
        return -1;
    }
    strncpy(newInput, input, strlen(input) + 1);
    char *firstLine; //will be the first line
    firstLine = strtok(newInput, "\r\n");
    char *request = (char *)malloc(sizeof(char) * (strlen(firstLine)) + 1);
    if (request == NULL)
    {
        freeMe(filter);
        free(newInput);
        printf("Error. Allocation was unsuccessful.\n");
        return -1;
    }
    strncpy(request, firstLine, strlen(firstLine) + 1);//creating 'request'
    char *firstTok = strtok(firstLine, " ");//frist char
    char *secondTok = strtok(NULL, " ");//sec char
    char *thirdTok = strtok(NULL, "\r\n");//third char
    if (firstTok == NULL || secondTok == NULL || thirdTok == NULL)//if some1 null so we have more or less than 3
    {
        errorMess(400, err, tempSoc);
        free(request);
        free(newInput);
        return 400;
    }
    if (strcmp(firstTok, "GET") != 0)
    {
        errorMess(501, err, tempSoc);
        free(request);
        free(newInput);
        return 501;
    }
    if (strcmp(thirdTok, "HTTP/1.0") != 0 && strcmp(thirdTok, "HTTP/1.1") != 0)//if not equal to 0 so meands we dont have right 1
    {
        errorMess(501, err, tempSoc);
        free(request);
        free(newInput);
        return 501;
    }
    strncpy(newInput, input, strlen(input) + 1);
    char *hostHeader = strtok(newInput, "\r\n");
    while (hostHeader != NULL)
    {
        if (strstr(hostHeader, "Host: ") != NULL)
        { // host header exists
            hostHeader += 6;
            break;
        }
        hostHeader = strtok(NULL, "\r\n");
    }
    if (hostHeader == NULL)
    {
        free(request);
        free(newInput);
        errorMess(400, err, tempSoc);
        return 400;
    }
    int port = 80;
    if (strstr(hostHeader, ":") != NULL)
    {                                   //means we have :
        if (findPort(hostHeader) != -1) //findPort returns -1 if port not exists
        {
            port = findPort(hostHeader);
            hostHeader = strtok(hostHeader, ":");
        }
    }
    if (gethostbyname(hostHeader) == NULL)//check if have ip
    {
        free(request);
        free(newInput);
        errorMess(404, err, tempSoc);
        return 404;
    }
    for (int i = 0; i < filterRows; i++)
    {
        if (strcmp(filter[i], hostHeader) == 0)
        {
            free(request);
            free(newInput);
            errorMess(403, err, tempSoc);
            return 403;
        }
    }
    request = (char *)realloc(request, strlen(request) + strlen(hostHeader) + 32);//add 1
    if(request == NULL){
        free(newInput);
        return -1;
    }
    strcat(request, "\r\nHost: ");
    strcat(request, hostHeader);
    strcat(request, "\r\nConnection: close\r\n\r\n");
    int sd, rd = 1, wr = 0, sum = 0;
         struct sockaddr_in serv_addr;
         struct hostent *server;
         char rbuf[BUF];
         sd = socket(AF_INET, SOCK_STREAM, 0);
         if (sd < 0)
         {
             perror("socket() failed"); //if creating socket failed
             freeMe(filter);
             free(newInput);
             free(request);
             exit(EXIT_FAILURE);
         }
         server = gethostbyname(hostHeader);
         if (server == NULL)
         {
            errorMess(404,err,tempSoc);
            perror("gethostbyname() failed"); //if gethost failed
            close(sd);
            freeMe(filter);
            free(request);
            free(newInput);
            exit(EXIT_FAILURE);
             //return -1;
         }//creating
         serv_addr.sin_family = AF_INET;
         bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr, server->h_length);
         serv_addr.sin_port = htons(port);
         if (connect(sd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
         {
             errorMess(500,err,sd);
             perror("connect() failed");
             freeMe(filter);
             free(request);
             free(newInput);
             exit(EXIT_FAILURE);
         }
         wr = write(sd, request, strlen(request) + 1);
         if (wr < 0)
         {
             perror("write failed()"); //if write failed
             freeMe(filter);
             free(request);
             free(newInput);
             exit(EXIT_FAILURE);
         }
         while (rd > 0)
         { //while loop for read func
             rd = read(sd, rbuf, BUF);
             /*if(strstr(rbuf, "\r\n\r\n") != NULL){//help us to know when we r done
             break;
         }*/
             if (rd < 0){
                 perror("read() failed"); //if read failed
                 freeMe(filter);
                 free(newInput);
                 free(request);
                 errorMess(500,err,sd);
             }
             sum += rd;
             if (rd != 0){
                 rbuf[rd] = '\0';
                 if (write(tempSoc, rbuf, rd) == -1)
                 {
                     perror("write() failed"); //if read failed
                     freeMe(filter);
                     free(newInput);
                     free(request);
                     errorMess(500,err,sd);
                 }
                 //printf("%s", rbuf);
             }
             if (rd == 0){
                 break;
             }
        }
        free(request);
        free(newInput);
        close(tempSoc);
        return 0;//success
}

void freeMe(char **something){//will free filter
    if (*something == NULL || something == NULL)
    {
        return;
    }
    for (int i = 0; i <filterRows; i++)
    {
        free(something[i]);
    }
    free(something);
}

void checkArgs(int argc, char *argv[]){//check first params
    if (argc < 5 || argc > 5)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n"); //EXIT?
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc - 1; i++)
    {
        if (atoi(argv[i]) <= 0)
        {                                                                                       //check if there is a negative number or not a number!
            printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n"); //EXIT?
            exit(EXIT_FAILURE);
        }
    }
}

void errorMess(int num, char err[], int sock){ //will be the errorMessage
    //will be the err message by ex rules
    if (num == 400)
    {
        strcpy(err, "HTTP/1.0 400 Bad Request\r\n");
    }
    else if (num == 403)
    {
        strcpy(err, "HTTP/1.0 403 Forbidden\r\n");
    }
    else if (num == 404)
    {
        strcpy(err, "HTTP/1.0 404 Not Found\r\n");
    }
    else if (num == 500)
    {
        strcpy(err, "HTTP/1.0 500 Internal Server Error\r\n");
    }
    else if (num == 501)
    {
        strcpy(err, "HTTP/1.0 501 Not supported\r\n");
    }
    strcat(err, "Server: webserver/1.0\r\n");
    strcat(err, "Content-Type: text/html\r\n");
    if (num == 400)
    {
        strcat(err, "Content-Length: 113\r\n");
        strcat(err, "Connection: close\r\n\r\n");
        strcat(err, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n");
        strcat(err, "<BODY><H4>400 Bad request</H4>\r\n");
        strcat(err, "Bad Request.\r\n");
    }
    else if (num == 403)
    {
        strcat(err, "Content-Length: 111\r\n");
        strcat(err, "Connection: close\r\n\r\n");
        strcat(err, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n");
        strcat(err, "<BODY><H4>403 Forbidden</H4>\r\n");
        strcat(err, "Access denied.\r\n");
    }
    else if (num == 404)
    {
        strcat(err, "Content-Length: 112\r\n");
        strcat(err, "Connection: close\r\n\r\n");
        strcat(err, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n");
        strcat(err, "<BODY><H4>404 Not Found</H4>\r\n");
        strcat(err, "File not found.\r\n");
    }
    else if (num == 500)
    {
        strcat(err, "Content-Length: 144\r\n");
        strcat(err, "Connection: close\r\n\r\n");
        strcat(err, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n");
        strcat(err, "<BODY><H4>500 Internal Server Error</H4>\r\n");
        strcat(err, "Some server side error.\r\n");
    }
    else if (num == 501)
    {
        strcat(err, "Content-Length: 129\r\n");
        strcat(err, "Connection: close\r\n\r\n");
        strcat(err, "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n");
        strcat(err, "<BODY><H4>501 Not supported</H4>\r\n");
        strcat(err, "Method is not supported.\r\n");
    }
    strcat(err, "<BODY></HTML/>\r\n\r\n");
    if(write(sock,err,strlen(err)) == -1){
        perror("write() failed");
    }
}

void getFIlter(char *path){ //will give n\r\nth"2000","6","6","filter.txt

    FILE *fp;
    fp = fopen(path, "r");
    if (fp == NULL)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n"); //EXIT?
        exit(EXIT_FAILURE);
    }
    size_t size = 0;
    char *temp = NULL;
    int i = 0;
    while (1)
    {
        filter = (char **)realloc(filter, sizeof(char *) * (i + 1));
        if (filter == NULL)
        {
            printf("Error. Reallocation was unsuccessful.\n");
            exit(EXIT_FAILURE);
        }
        if (getline(&temp, &size, fp) == -1)
        {
            break;
        }
        filter[i] = malloc(strlen(temp) + 1);
        if(filter[i] == NULL){
            printf("Error. Allocation was unsuccessful.\n");
            exit(EXIT_FAILURE);
        }
        strncpy(filter[i], temp, strlen(temp) + 1);
        filter[i] = strtok(filter[i], "\r\n");
        i++;
        filterRows++;
    }
    free(temp);
    fclose(fp);
}

int findPort(char* input){//find me a port, return -1 we dont have 1
    char *port1;
    char port2[256];
    //port[0] = '\0';
    if(input == NULL){
        return -1;
    }
      port1 = strstr(input,":")+1;
        strcpy(port2,strtok(port1,"\r\n"));
        if(atoi(port2) > 0){
            return atoi(port2);
        }
        return -1;
}