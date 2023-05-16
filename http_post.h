#ifndef ___HTTP_POST_H
#define ___HTTP_POST_H






void processPOST(int sock, char *request_line, char *access_secret, char *root_folder, char *default_CWD);

void processPOSTupload(int sock, char *baseFolder);

void sendListResponse(int sock, char *access_secret, char *root_folder, char *cwd);

void replyPostError(int sock, char *error);





#endif

