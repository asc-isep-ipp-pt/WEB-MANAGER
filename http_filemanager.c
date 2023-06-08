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
#include "http_filemanager.h"
#include "web-manager.h"




// Only the first request is a GET, following requests are POSTs
// If the access secret is ok, the GET is redirected to a POST
// The requestLine format is GET /filemanager/{SECRET}
//
void processGETfilemanager(int sock, char *requestLine) {
	char *aux, line[1000];
	char uri[1000];

	do {    // read and ignore the remaining header lines
		readLineCRLF(sock,line);
	}
	while(*line);

	strcpy(uri,requestLine+17);
	aux=uri;
        while(*aux!=32) {aux++;} *aux=0;
	// The URI's last element is the access secret
	if(strcmp(uri,access_secret)) {
		sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
		sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line);
		puts("Oops, not authorized.");
		}
	else    {
                sprintf(line,"%s<body bgcolor=gray onLoad=\"act('list','','')\"> \
			<form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value=\"%s\"> \
			<input type=hidden name=action value=list><input type=hidden name=object value=> \
			<input type=hidden name=object2 value=><input type=hidden name=cwd value=></form>%s",HTML_HEADER,access_secret,HTML_BODY_FOOTER);
		sendHttpStringResponse(sock, "200 Ok", "text/html", line);
		}
		
	}



void processPOSTfilemanager(int sock, char *request_line) {
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

		if(!*cwd) {strcpy(cwd, default_cwd); strcpy(action,"list"); }
		if(strncmp(root_folder,cwd,strlen(root_folder))) { strcpy(cwd, default_cwd); strcpy(action,"list");}

		////////////////////////////////////////////// LIST FOLDER CONTENT

		if(!strcmp(action,"list")) { free(content); sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// CDUP

		if(!strcmp(action,"cdup")) { free(content); 
			if(strcmp(root_folder,cwd)) {
			strcpy(line,cwd); aux=line+strlen(line)-1; while(*aux!='/') aux--; if(aux==line) aux++; *aux=0;
			}
			sendListResponse(sock, line); return; }

		////////////////////////////////////////////// DELETE CLIPBOARD

		if(!strcmp(action,"deleteclip")) { free(content); 
			sprintf(line,"rm -Rf %s/*",clipboard_folder);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// PASTE FROM CLIPBOARD

		if(!strcmp(action,"pasteclip")) { free(content); 
			sprintf(line,"cp -Rf %s/* %s/",clipboard_folder,cwd);
			system(line);
			sendListResponse(sock, cwd); return; }

	
		/////////////////////////////////////// Get the object value
		aux=strstr(content,"object=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object[200];
		aux1=object; aux+=7; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;


		/// If object not provided don't do anything bellow this and return the list
		if(!*object) { free(content); sendListResponse(sock, cwd); return; }




		////////////////////////////////////////////// DOWNLOAD A FILE TO THE SERVER (wget)

		if(!strcmp(action,"wget")) { free(content); 
			sprintf(line,"%s -N -P \"%s\" \"%s\"",wget_command,cwd,object);
			system(line);
			sendListResponse(sock, cwd); return; }


		////////////////// for other commands the slash is not allowed in the object name
		
		aux=object; while(*aux>31 && *aux!='/') aux++; *aux=0;


		////////////////////////////////////////////// CD

		if(!strcmp(action,"cd")) { free(content); 
			strcpy(line,cwd);
			if(strcmp(line,"/")) strcat(line,"/");
			strcat(line,object);

			sendListResponse(sock, line); return; }

		////////////////////////////////////////////// MKDIR

		if(!strcmp(action,"mkdir")) { free(content); 
			
			sprintf(line,"mkdir -p \"%s/%s\"",cwd,object);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// MK EMPTY FILE

		if(!strcmp(action,"mkfile")) { free(content); 

			sprintf(line,"touch \"%s/%s\"",cwd,object);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// REMOVE FILES OR FOLDERS WITH CONTENTS

		if(!strcmp(action,"rm")) { free(content); 

			sprintf(line,"rm -Rf \"%s/%s\"",cwd,object);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// DOWNLOAD A FILE TO THE BROWSER

		if(!strcmp(action,"download")) { free(content); 

			sendHttpFileDownloadResponse(sock, cwd, object); return; }

		////////////////////////////////////////////// COPY TO CLIPBOARD

		if(!strcmp(action,"copytoclip")) { free(content); 

			sprintf(line,"rm -Rf %s/*",clipboard_folder);
			system(line);
			sprintf(line,"cp -R \"%s/%s\" %s/",cwd,object,clipboard_folder);
			system(line);
			sendListResponse(sock, cwd); return; }









		// TODO
		////////////////////////////////////////////// DETAILS

		if(!strcmp(action,"details")) { free(content); 

			//sprintf(line,"touch %s/%s",cwd,object);
			//system(line);
			
			sendDetailsResponse(sock, cwd, object); return; }






		/////////////////////////////////////// get the object2
		aux=strstr(content,"object2=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object2[200];
		aux1=object2; aux+=8; while(*aux>31) {*aux1=*aux; aux1++; aux++;}; *aux1=0;


		/// If object2 not provided don't do anything bellow this and return the list
		if(!*object2) { free(content); sendListResponse(sock, cwd); return; }



		////////////////////////////////////////////// RENAME

		if(!strcmp(action,"rename")) { free(content); 
			if(strcmp(object,object2)) {
				sprintf(line,"mv \"%s/%s\" \"%s/%s\"",cwd,object,cwd,object2);
				system(line);
			}
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// CLONE

		if(!strcmp(action,"clone")) { free(content); 
			if(strcmp(object,object2)) {
				sprintf(line,"cp -R \"%s/%s\" \"%s/%s\"",cwd,object,cwd,object2);
				system(line);
			}
			sendListResponse(sock, cwd); return; }






		////////////////////

		puts(action);
		puts(object);
		puts(object2);
		puts(cwd);


		free(content);
		}



	//			secret=ola&action=list&object=&object2=&cwd=
	//
	//


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







void sendDetailsResponse(int sock, char *cwd, char *obj) {


	}






/// the list response also contains several general options, e.g, create 

void sendListResponse(int sock, char *cwd) {
	char list[500000], *aux;
	DIR *d;
	struct dirent *e;



	sprintf(list,"%s<body bgcolor=gray> \
		       <form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		       <input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		       <p><font size=6>&nbsp; &nbsp; Current folder: <b>%s</b></font> \
		       <p><table width=100%% border=0 cellspacing=3><tr> \
		       <td align=center valign=top style=\"width:250px\"><details><summary>CREATE</summary><p><input id=mkobjname type=text> \
		       <p><input type=button value=\"FOLDER\" onclick=\"act('mkdir',document.getElementById('mkobjname').value,'');\"> \
		       <input type=button value=\" FILE \" onclick=\"act('mkfile',document.getElementById('mkobjname').value,'');\"></p></details</td> \
		       <td></td>",HTML_HEADER,access_secret,cwd,cwd);

	if(wget_command) {
		strcat(list,"<td align=center valign=top style=\"width:300px\"><details><summary>Upload file from URL (wget)</summary><p><input id=urlwget type=text name=urlwget> \
			<p><input type=button value=\"DOWNLOAD\" onclick=\"act('wget',document.getElementById('urlwget').value,'');\"></p></details></td>");
	}



	// TODO: The Clipboard
	
	char clipboardContent[200];
	d=opendir(clipboard_folder);
	e=readdir(d);
	while(e) {
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) break;
		e=readdir(d);
	}

	if(e) { strcpy(clipboardContent,e->d_name); if(e->d_type==DT_DIR) strcat(clipboardContent,"/"); }
	else *clipboardContent=0;
	closedir(d);

	aux=list+strlen(list);
	if(*clipboardContent) {
		sprintf(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>CLIPBOARD (*)</summary><p>%s",clipboardContent);
		strcat(aux,"</p><p><input type=button value=\"PASTE\" onclick=\"act('pasteclip','','');\">&nbsp;<input type=button value=\"DELETE\" onclick=\"act('deleteclip','','');\"></p></details></td>");
	}
	else {
		sprintf(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>CLIPBOARD</summary><p></p></details></td>");
	}





	strcat(aux,"</tr></table></p><hr>");
	aux=aux+strlen(aux);

	// Add to the list the current folder listing contents
	d=opendir(cwd);
	printf("Listing folder %s\n",cwd);
	if(!d) { sprintf(list,"%s<body bgcolor=yellow><h1>Failed to open directory %s for listing.</h1>%s",HTML_HEADER,cwd,HTML_BODY_FOOTER);
			           sendHttpStringResponse(sock, "500 Internal Server Error", "text/html", list); return; }
	
	// add ../ for cdup
	if(strcmp(cwd,root_folder)) strcat(aux,"<p><a href=\"javascript:act('cdup','','');\"><b>..</b><i>(parent folder)</i></a></li>");

	int elem=0;  // used to grant unique html IDs
	do {
		e=readdir(d);
		if(!e) break;
		// use the details tag
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) {
			elem++;

			strcat(aux,"<p><details><summary>&nbsp;&nbsp;&nbsp;&nbsp;");
			// if a folder, permit cd into it
			aux=aux+strlen(aux);
			if(e->d_type==DT_DIR) sprintf(aux,"<a href=\"javascript:act('cd','%s','');\"><b>%s/</b></a>",e->d_name,e->d_name);
			else sprintf(aux,"<b>%s</b>",e->d_name);
			strcat(aux,"</summary><p><table width=100%% border=0 cellspacing=3><tr>");


			// Manage ???
			strcat(aux,"<td align=center valign=top style=\"width:250px\"><input type=button value=\"Manage Object\" onClick=\"javascript:act('details','");
			strcat(aux,e->d_name); strcat(aux,"','');\"></td>");



			// if a regular file, add the option of downloading the file
			aux=aux+strlen(aux);
			if(e->d_type==DT_REG) sprintf(aux,"<td align=center valign=top style=\"width:250px\"><input type=button value=\"Download\" onClick=\"javascript:act('download','%s','');\"></td>",
					e->d_name);

			// RENAME or CLONE 
			aux=aux+strlen(aux);
			sprintf(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>RENAME/CLONE</summary> \
		       		<p><input id=ren%i type=text value=\"%s\"> \
				<p><input type=button value=Rename onclick=\"act('rename','%s',document.getElementById('ren%i').value);\"> \
		       		<input type=button value=Clone onclick=\"act('clone','%s',document.getElementById('ren%i').value);\"></p></details</td>",
				elem, e->d_name, e->d_name, elem, e->d_name, elem);

			// DELETE 
			strcat(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>DELETE</summary><p>");
			aux=aux+strlen(aux);
			if(e->d_type==DT_DIR) sprintf(aux,"<input type=button value=\"Confirm Remove Folder and Contents\" onClick=\"javascript:act('rm','%s','');\"></p></details></td>",e->d_name); 
			else sprintf(aux,"<input type=button value=\"Confirm Remove File\" onClick=\"javascript:act('rm','%s','');\"></p></details></td>",e->d_name); 


			// COPY TO CLIPBOARD (TODO)
			aux=aux+strlen(aux);
			if(!*clipboardContent) sprintf(aux,"<td align=center valign=top style=\"width:250px\"><input type=button value=\"COPY to CLIPBOARD\" onClick=\"javascript:act('copytoclip','%s','');\"></td>",
					e->d_name);
			else
				sprintf(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>COPY to CLIPBOARD</summary><p> \
						<input type=button value=\"Confirm Overwrite Clipboard\" onClick=\"javascript:act('copytoclip','%s','');\"></p></details></td>",e->d_name);




			// TODO:


			
			// Edit textfile (TODO)



			strcat(aux,"</tr></table><hr></p></details>");
			}

		}
	while(e);
	closedir(d);
	strcat(aux,HTML_BODY_FOOTER);
	sendHttpStringResponse(sock, "200 Ok", "text/html", list);
	return;
	}



void sendHttpFileDownloadResponse(int sock, char *cwd, char *obj) {
	FILE *f;
	char aux[200];
	long len;
	int done;

	sprintf(aux,"%s/%s",cwd,obj);
	f=fopen(aux,"r");
	if(!f) {
		sendHttpStringResponse(sock, "404 Not Found", "text/html", "<html><body><h1>404 File not found</h1></body></html>");
		return;
		}
	
	fseek(f,0,SEEK_END);
	len=ftell(f);

	sprintf(aux,"%s %s",HTTP_VERSION, "200 Ok");
	writeLineCRLF(sock,aux);
	writeLineCRLF(sock, "Content-type: binary/data");
	sprintf(aux,"Content-Disposition: attachment; filename=%s",obj);
	writeLineCRLF(sock,aux);
	sprintf(aux,"Content-length: %ld",len);
	writeLineCRLF(sock,aux);
	writeLineCRLF(sock,HTTP_CONNECTION_CLOSE);
	writeLineCRLF(sock,"");

	rewind(f);
	do {
		done=fread(aux,1,200,f);
		if(done>0) write(sock,aux,done);
		}
	while(done);
	fclose(f);
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




