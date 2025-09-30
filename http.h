#ifndef ___HTTP_H
#define ___HTTP_H

#define HTTP_VERSION "HTTP/1.0"

#define HTTP_CONNECTION_CLOSE "Connection: close"
#define HTTP_CONTENT_LENGTH "Content-Length: "
#define HTTP_CONTENT_TYPE "Content-Type: "

#define HTTP_CONTENT_TYPE_FORM_URLENCODED "application/x-www-form-urlencoded"

#define HTTP_CONTENT_TYPE_TEXT_HTML "text/html"



void readLineCRLF(int sock, char *line);
void writeLineCRLF(int sock, char *line);

void sendHttpResponseHeader(int sock, char *status, char *contentType, int contentLength);
int sendHttpResponse(int sock, char *status, char *contentType, char *content, int contentLength);
void sendHttpStringResponse(int sock, char *status, char *contentType, char *content);
//void sendHttpFileResponse(int sock, char *status, char *filename);
void sendHttpFileContent(int sock, FILE *file, char *status, char *contentType);


#endif
