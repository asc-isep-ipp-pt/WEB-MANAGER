#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

#include "http.h"
#include "http_filemanager.c"
#include "web-manager.h"
#include "killall.c"



// read the secret from a file

void usage_message(void) {printf("Optional command line options are:\n --secret-file FILE-WITH-SECRET-STRING (default is %s)\n --root-folder FOLDER (default is %s)\n --initial-cwd FOLDER (default is %s)\n --port TCP-PORT-NUMBER (default is %s)\n --title TITLE (default is \"Web Manager\")\n --killall\n\n",
		secret_file,root_folder,default_cwd,port_number);exit(1);}


int main(int argc, char **argv) {
	struct sockaddr_storage from;
	int c, err, Nsock, sock;
	socklen_t adl;
	struct addrinfo  req, *list;
	char line[2500];
	char new_secret[2500];
	char *aux;

	c=1;
	while(c<argc) {
		if(!strcmp(argv[c],"--help")) usage_message();
		if(!strcmp(argv[c],"--secret-file")) {
			c++; if(c>=argc) usage_message();
			secret_file=argv[c];
		}
		else
		if(!strcmp(argv[c],"--root-folder")) {
			c++; if(c>=argc) usage_message();
			root_folder=argv[c];
		}
		else
		if(!strcmp(argv[c],"--initial-cwd")) {
			c++; if(c>=argc) usage_message();
			default_cwd=argv[c];
		}
		else
		if(!strcmp(argv[c],"--title")) {
			c++; if(c>=argc) usage_message();
			title=argv[c];
		}
		else
		if(!strcmp(argv[c],"--port")) {
			c++; if(c>=argc) usage_message();
			port_number=argv[c];
		}
		else
		if(!strcmp(argv[c],"--killall")) {
			web_manager_kill_all();
			exit(0);
		}
		else
			usage_message();
		c++;
		
	}


	FILE *f=fopen(secret_file,"r");
	if (!f) { printf("Failed to read the access secret from file %s (failed to open)\n",secret_file); usage_message();}
	if (!fgets(new_secret,2500,f)) { printf("Failed to read the access secret from file %s (failed to read)\n",secret_file); fclose(f); usage_message();}
	fclose(f);
	chmod(secret_file,S_IRUSR|S_IWUSR); // read and write for owner only
	aux=new_secret; while(*aux>32) aux++; *aux=0;
	if(strlen(new_secret)<min_secret_len) {printf("The secret length is bellow the minimum (%i)\n", min_secret_len); usage_message();}
	access_secret=new_secret;

	if(strncmp(root_folder,default_cwd,strlen(root_folder))) default_cwd=root_folder;
	
	sprintf(html_header,HTML_HEADER_TEMPLATE,title);
	

	

	bzero((char *)&req,sizeof(req));
	req.ai_family = AF_INET6;       // requesting an IPv6 local address will allow both IPv4 and IPv6 clients to connect
	req.ai_socktype = SOCK_STREAM;
	req.ai_flags = AI_PASSIVE;      // local address

	err=getaddrinfo(NULL, port_number , &req, &list);
	if(err) {
        	printf("Failed to get local address, error: %s\n",gai_strerror(err)); exit(1); }

	sock=socket(list->ai_family,list->ai_socktype,list->ai_protocol);
	if(sock==-1) {
        	perror("Failed to open local socket"); freeaddrinfo(list); exit(1);}

	err=1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &err, sizeof(int)) == -1) { perror("Failed to set SO_REUSEADDR"); close(sock); exit(1); }

	if(bind(sock,(struct sockaddr *)list->ai_addr, list->ai_addrlen)==-1) {
        	perror("Bind failed");close(sock);freeaddrinfo(list);exit(1);}

	freeaddrinfo(list);

	signal(SIGCHLD, SIG_IGN); // AVOID LEAVING TERMINATED CHILD PROCESSES AS ZOMBIES


	// check if the wget command is available
	if(!access("/usr/bin/wget",X_OK)) wget_command="/usr/bin/wget";
	if(!wget_command && !access("/bin/wget",X_OK)) wget_command="/bin/wget";
	if(!wget_command && !access("/usr/local/bin/wget",X_OK)) wget_command="/usr/local/bin/wget";

	// check if the file command is available
	if(!access("/usr/bin/file",X_OK)) file_command="/usr/bin/file";
	if(!file_command && !access("/bin/file",X_OK)) file_command="/bin/file";
	if(!file_command && !access("/usr/local/bin/file",X_OK)) file_command="/usr/local/bin/file";


	// create the clipboard folder
	mkdir(clipboard_folder,0700);



	listen(sock,SOMAXCONN);
	adl=sizeof(from);
	for(;;)	{
        	Nsock=accept(sock,(struct sockaddr *)&from,&adl); // WAIT A FOR CLIENT CONNECTION
        	if(!fork()) {
                	close(sock);
			readLineCRLF(Nsock,line); // read the request line
			// printf("Request line: %s\n", line);
			if(!strncmp(line,"GET /favicon.ico",16)) { do readLineCRLF(Nsock,line); while(*line); sendHttpResponse(Nsock,"200 Ok","image/x-icon",favicon,favicon_length);}
			else
			if(!strncmp(line,"GET /filemanager",16)) processGETfilemanager(Nsock,line);
			else
			if(!strncmp(line,"POST /filemanager",17)) processPOSTfilemanager(Nsock, line);
			else {
				printf("Request line not supported by this server: %s\n",line);
				sprintf(line,"%s<body bgcolor=yellow><h1>HTTP method not supported</h1>%s",html_header,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "405 Method Not Allowed", "text/html", line);
			}
                	close(Nsock);
                	exit(0);
                	}
        	close(Nsock);
        	}
	close(sock);
	}




