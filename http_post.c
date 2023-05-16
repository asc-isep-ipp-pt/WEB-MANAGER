#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>

#include "http.h"
#include "http_post.h"
#include "web-manager.h"




void processPOST(int sock, char *request_line, char *access_secret, char *root_folder, char *default_CWD) {
	char line[1000], content_type[60];
	int content_len;

	content_len=0;
	*content_type=0;

        do {    // read the remaining header lines
                readLineCRLF(sock,line);
		//puts(line);
		if(!strncasecmp(line,HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH))) content_len=atoi(line+strlen(HTTP_CONTENT_LENGTH));
		if(!strncasecmp(line,HTTP_CONTENT_TYPE, strlen(HTTP_CONTENT_TYPE))) strcpy(content_type, line+strlen(HTTP_CONTENT_TYPE));
                }
        while(*line);

	if(!content_len) { puts("Fatal error: empty POST request"); return; }

	printf("Content type is %s\n",content_type);

	//if(!strcasecmp(content_type,HTTP_CONTENT_TYPE_FORM_URLENCODED)) {
	if(!strcasecmp(content_type,"text/plain")) {
		// read the entire content to memory
		int i, todo;
		char *aux, *aux1, *content;

		todo=content_len;
		content=malloc(content_len);
		if(!content) { puts("Fatal error allocating memory for POST data"); return; }
		aux=content;
		while(todo) {
			i=read(sock,aux,todo);
			if(i<0) { puts("Fatal error: error reading POST data"); free(content); return; }
			aux+=i; todo-=i;
			}
		content[content_len]=0;

		puts(content);

		/////////////////////////////////////// check the secret
		aux=strstr(content,"secret=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		aux1=line; aux+=7; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;
		
		if(strcmp(line,access_secret)) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
							sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		/////////////////////////////////////// get the action
		aux=strstr(content,"action=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char action[100];
		aux1=action; aux+=7; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;


		/////////////////////////////////////// get the cwd
		aux=strstr(content,"cwd=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char cwd[200];
		aux1=cwd; aux+=4; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;

		if(!*cwd) strcpy(cwd, default_CWD);

		//////////////////////////////////////////////

		if(!strcmp(action,"list")) { free(content); sendListResponse(sock, access_secret, root_folder, cwd); return; }

		/////////////////////////////////////// get the object
		aux=strstr(content,"object=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object[200];
		aux1=object; aux+=7; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;

		////////////////////////////////////////////// CD

		if(!strcmp(action,"cd")) { free(content); 

			if(*object=='/') strcpy(line,object);
			else
			if(!strcmp(object,"..")) { strcpy(line,cwd); aux=line+strlen(line)-1; while(*aux!='/') aux--; if(aux==line) aux++; *aux=0; }
			else
			if(!strcmp(object,".")) strcpy(line,cwd); 
			else
				{ strcpy(line,cwd); aux=line; while(*aux) aux++; aux--; if(*aux=='/') strcat(line,object); else sprintf(line,"%s/%s",cwd,object); }

			// block if things end up out of the root folder
			if(strncmp(line,root_folder,strlen(root_folder))) strcpy(line,cwd);

			sendListResponse(sock, access_secret, root_folder, line); return; }

		////////////////////////////////////////////// CD

		if(!strcmp(action,"mkdir")) { free(content); 


			sendListResponse(sock, access_secret, root_folder, line); return; }


		/////////////////////////////////////// get the object2
		aux=strstr(content,"object2=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object2[200];
		aux1=object2; aux+=8; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;


		////////////////////

		puts(action);
		puts(object);
		puts(object2);
		puts(cwd);


		free(content);
		}



	//			secret=ola&action=list&object=&object2=&cwd=


	sendHttpStringResponse(sock, "200 Ok", "text/html", "OK");

	}


	/// TODO
	//




	//if(!strncmp(request+5,"/upload",7)) processPOSTupload(sock, baseFolder);
	//else processPOSTlist(sock, baseFolder);
	//}
	//
	//
	//
	//





void sendListResponse(int sock, char *access_secret, char *root_folder, char *cwd) {
	char list[50000];
	DIR *d;
	struct dirent *e;

	d=opendir(cwd);
	printf("Listing folder %s\n",cwd);
	if(!d) { sprintf(list,"%s<body bgcolor=yellow><h1>Failed to open directory for listing.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
			           sendHttpStringResponse(sock, "500 Internal Server Error", "text/html", list); return; }


	sprintf(list,"%s<body bgcolor=gray> \
		       <form name=main method=POST action=/ enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		       <input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		       <table width=100%% border=0 cellspacing=3><tr> \
		       <td align=center valign=middle bgcolor=#B0C0B0><b>Create object</b><p>Name: <input id=objname type=text name=objname> \
		       <p><input type=button value=\"Create Folder\" onclick=\"act('mkdir',document.getElementById('objname').value,'');\"> \
		       <input type=button value=\"Create Empty File\" onclick=\"act('mkfile',document.getElementById('objname').value,'');\"></td> \
		       </tr></table> \
		       <hr><h3>%s</h3><ul>",HTML_HEADER,access_secret,cwd,cwd);




	do {
		e=readdir(d);
		if(!e) break;
		if(e->d_type==DT_DIR) { strcat(list,"<li><a href=\"javascript:act('cd','"); strcat(list,e->d_name); strcat(list,"','');\"><b>");
			strcat(list,e->d_name);strcat(list,"/</b></a></li>");}
		else { strcat(list,"<li><b>"); strcat(list,e->d_name); strcat(list,"</b></li>");}

		}
	while(e);
	closedir(d);
	strcat(list,"</ul>");
	strcat(list,HTML_BODY_FOOTER);
	sendHttpStringResponse(sock, "200 Ok", "text/html", list);
	puts("Sent list response");
	return;
	}







void processPOSTupload(int sock, char *baseFolder) {
	char line[200];
	char separator[100];
	char filename[100];
	char filePath[100];
	int readNow,done,len;
	char *cLen="Content-Length: ";
	char *cTypeMP="Content-Type: multipart/form-data; boundary=";
	char *cDisp="Content-Disposition: form-data; name=\"filename\"; filename=\"";
	int cLenS=strlen(cLen);
	int cTypeMPS=strlen(cTypeMP);
	int cDispS=strlen(cDisp);
	FILE *f;

	*separator=0;
	*filename=0;
	len=0;

	do {	// FIRST HEADER
        	readLineCRLF(sock,line);
		if(!strncmp(line,cLen,cLenS)) {
			len=atoi(line+cLenS);
			}		
		else
			if(!strncmp(line,cTypeMP,cTypeMPS)) {
			strcpy(separator,line+cTypeMPS);
			}
        	}
	while(*line);

//	if(!*separator)
//		replyPostError(sock, "Content-Type: multipart/form-data; expected and not found");
//	if(!len)
//		replyPostError(sock, "Content-Length: expected and not found");

	readLineCRLF(sock,line); // SEPARATOR
	//if(strcmp(line+2,separator))
	//	replyPostError(sock, "Multipart separator expected and not found");
	len=len-strlen(line)-2;

	do {	// SECOND HEADER
		readLineCRLF(sock,line);
		len=len-strlen(line)-2;
		if(!strncmp(line,cDisp,cDispS)) {
			strcpy(filename,line+cDispS); filename[strlen(filename)-1]=0;
			}
		}
	while(*line);

	if(!*filename) {
		do {  				// READ THE CONTENT
			done=read(sock,line,200); len=len-done; }
		while(len>0);
	//	replyPostError(sock, "Content-Disposition: form-data; expected and not found (NO FILENAME)");
		}

	strcpy(filePath,baseFolder); strcat(filePath,"/"); strcat(filePath,filename);
	f=fopen(filePath,"w+");
	if(!f) {
		sprintf(line, "Failed to create %s file\n",filePath);
	//	replyPostError(sock, line);
		}

	// SUBTRACT THE SEPARATOR LENGHT, PLUS -- ON START PLUS -- ON END PLUS CRLF
	len=len-strlen(separator)-6;

	do { // FILE CONTENT
		if(len>200) readNow=200; else readNow=len;
		done=read(sock,line,readNow);
		len=len-done;
		if(done>0) fwrite(line,1,done,f);
        	}	
	while(len>0);
	readLineCRLF(sock,line);
	fclose(f);
	//replyPostList(sock, baseFolder);
	}




