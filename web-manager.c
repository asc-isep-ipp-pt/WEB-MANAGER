#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>

#include "http.h"
#include "http_post.h"
#include "web-manager.h"


int main(int argc, char **argv) {
	struct sockaddr_storage from;
	int err, Nsock, sock;
	socklen_t adl;
	struct addrinfo  req, *list;
	char line[2500];
	char *access_secret;

	if(argc!=5) {puts("Sorry, the mandatory command line arguments are: server-port-number access-secret root-folder initial-folder"); exit(1);}
	access_secret=argv[2];

	//
	// ARGUMENTS: server-port-number access-secret root-folder initial-folder
	// ALL ARGUMENTS ARE MANDATORY
	//

	bzero((char *)&req,sizeof(req));
	req.ai_family = AF_INET6;       // requesting an IPv6 local address will allow both IPv4 and IPv6 clients to connect
	req.ai_socktype = SOCK_STREAM;
	req.ai_flags = AI_PASSIVE;      // local address

	err=getaddrinfo(NULL, argv[1] , &req, &list);
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

	listen(sock,SOMAXCONN);
	adl=sizeof(from);
	for(;;)	{
        	Nsock=accept(sock,(struct sockaddr *)&from,&adl); // WAIT FOR CLIENT CONNECTION
        	if(!fork()) {
                	close(sock);
			readLineCRLF(Nsock,line); // read the request line
			// printf("Request line: %s\n", line);
			if(!strncmp(line,"GET /",5)) processGET(Nsock,line,access_secret);
			else
			if(!strncmp(line,"POST /",6)) processPOST(Nsock, line, access_secret, argv[3], argv[4]);
			else {
				sprintf(line,"%s<body bgcolor=yellow><h1>HTTP method not supported</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "405 Method Not Allowed", "text/html", line);
				puts("Oops, the method is not supported by this server");
			}
                	close(Nsock);
                	exit(0);
                	}
        	close(Nsock);
        	}
	close(sock);
	}


// Only the first request is a GET, following requests are POSTs
// If the access secret is ok, the GET is redirected to a POST
void processGET(int sock, char *requestLine, char *access_secret) {
        char *aux, line[1000];
        char uri[1000];

        do {    // read and ignore the remaining header lines
                readLineCRLF(sock,line);
                }
        while(*line);

        strcpy(uri,requestLine+5);
        aux=uri;
        while(*aux!=32) {aux++;} *aux=0;
	// The URI is the access secret
        if(strcmp(uri,access_secret)) {
		sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
		sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line);
		puts("Oops, not authorized.");
		}
	else	{
		sprintf(line,"%s<body bgcolor=gray onLoad=\"act('list','','')\"> \
				<form name=main method=POST action=/ enctype=text/plain><input type=hidden name=secret value=\"%s\"><input type=hidden name=action value=list><input type=hidden name=object value=> \
				<input type=hidden name=object2 value=><input type=hidden name=cwd value=></form>%s",HTML_HEADER,access_secret,HTML_BODY_FOOTER);



//echo "<body bgColor=grey>"
//echo "<form name=main method=POST action=\"${SCRIPT_NAME}\">"
//echo "<input type=hidden name=secret value=\"${SECRET}\">"
//echo "<input type=hidden name=action value=>"
//echo "<input type=hidden name=cwd value=\"${POST_CWD}\">"
//echo "<input type=hidden name=object value=>"
//echo "<input type=hidden name=object2 value=>"
//echo "</form>"
//echo "<table width=100% border=0 cellspacing=3><tr>"





		sendHttpStringResponse(sock, "200 Ok", "text/html", line);
		}

        }






