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


void usage_message(void) {puts("Mandatory command line options are:\n --secret SECRET-STRING-WITHOUT-BLANKS\nOptional command line options are:\n --root-folder FOLDER\n --initial-cwd FOLDER\n --port TCP-PORT-NUMBER");exit(1);}


int main(int argc, char **argv) {
	struct sockaddr_storage from;
	int c, err, Nsock, sock;
	socklen_t adl;
	struct addrinfo  req, *list;
	char line[2500];

	//
	// ARGUMENTS: server-port-number access-secret root-folder initial-fm-folder
	// ALL ARGUMENTS ARE MANDATORY
	//
	//
	//
	c=1;
	while(c<argc) {
		if(!strcmp(argv[c],"--help")) usage_message();
		if(!strcmp(argv[c],"--secret")) {
			c++; if(c>=argc) usage_message();
			access_secret=argv[c];
		}
		if(!strcmp(argv[c],"--root-folder")) {
			c++; if(c>=argc) usage_message();
			root_folder=argv[c];
		}
		if(!strcmp(argv[c],"--initial-cwd")) {
			c++; if(c>=argc) usage_message();
			default_cwd=argv[c];
		}
		if(!strcmp(argv[c],"--port")) {
			c++; if(c>=argc) usage_message();
			port_number=argv[c];
		}
		c++;
		
	}
	if(!access_secret) usage_message();
	if(strncmp(root_folder,default_cwd,strlen(root_folder))) default_cwd=root_folder;
	
	
	

	

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

	// create the clipboard folder
	mkdir(clipboard_folder,0700);



	listen(sock,SOMAXCONN);
	adl=sizeof(from);
	for(;;)	{
        	Nsock=accept(sock,(struct sockaddr *)&from,&adl); // WAIT FOR CLIENT CONNECTION
        	if(!fork()) {
                	close(sock);
			readLineCRLF(Nsock,line); // read the request line
			// printf("Request line: %s\n", line);
			if(!strncmp(line,"GET /filemanager/",17)) processGETfilemanager(Nsock,line);
			else
			if(!strncmp(line,"POST /filemanager",17)) processPOSTfilemanager(Nsock, line);
			else {
				printf("Request line not supported by this server: %s\n",line);
				sprintf(line,"%s<body bgcolor=yellow><h1>HTTP method not supported</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "405 Method Not Allowed", "text/html", line);
			}
                	close(Nsock);
                	exit(0);
                	}
        	close(Nsock);
        	}
	close(sock);
	}




