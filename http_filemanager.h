#ifndef ___HTTP_FILEMANAGER_H
#define ___HTTP_FILEMANAGER_H



void processGETfilemanager(int sock, char *request_line);
void processPOSTfilemanager(int sock, char *request_line);

void processPOSTupload(int sock, char *baseFolder);

void sendListResponse(int sock, char *cwd);

void sendDetailsResponse(int sock, char *cwd, char *obj);

void sendHttpFileDownloadResponse(int sock, char *cwd, char *obj);

void processMultipartPost(int sock, long content_len, char *boundary);



#endif

