#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
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
	char *aux, line[2*B_SIZE];
	char uri[2*B_SIZE];

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
	char line[5*B_SIZE], content_type[B_SIZE];
	long content_len;

	content_len=0;
	*content_type=0;

        do {    // read the remaining header lines
                readLineCRLF(sock,line);
		//puts(line);
		if(!strncasecmp(line,HTTP_CONTENT_LENGTH, strlen(HTTP_CONTENT_LENGTH))) content_len=atol(line+strlen(HTTP_CONTENT_LENGTH));
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

		char cwd[B_SIZE];
		aux1=cwd; aux+=4; while(*aux!='\r') {*aux1=*aux; aux1++; aux++;}; *aux1=0;

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

		////////////////////////////////////////////// DELETE CLIPBOARD CONTENT

		if(!strcmp(action,"deleteclip")) { free(content); 
			sprintf(line,"rm -Rf %s/*",clipboard_folder);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// PASTE FROM CLIPBOARD

		if(!strcmp(action,"pasteclip")) { free(content); 
			sprintf(line,"cp -Rf %s/* \"%s/\"",clipboard_folder,cwd);
			system(line);
			sendListResponse(sock, cwd); return; }





	
		/////////////////////////////////////// Get the object value
		aux=strstr(content,"object=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object[B_SIZE];
		aux1=object; aux+=7; while(*aux && *aux!='\r') {*aux1=*aux; aux1++; aux++;}; *aux1=0;


		/// If object not provided don't do anything bellow this and return the list
		if(!*object) { free(content); sendListResponse(sock, cwd); return; }


		////////////////////////////////////////////// UPLOAD FROM URL (wget)

		if(!strcmp(action,"wget")) { free(content); 
			sprintf(line,"%s -N -P \"%s\" \"%s\"",wget_command,cwd,object);
			system(line);
			sendListResponse(sock, cwd); return; }



		////////////////////////////////////////////// CD

		if(!strcmp(action,"cd")) { free(content); 
			if(*object=='/') strcpy(line,object);
			else	{
				strcpy(line,cwd);
				if(strcmp(line,"/")) strcat(line,"/");
				strcat(line,object);
			}

			sendListResponse(sock, line); return; }



		////////////////// for other commands the slash is not allowed in the object name
		
		aux=object; while(*aux && *aux!='/') aux++; *aux=0;



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


		////////////////////////////////////////////// DETAILS

		if(!strcmp(action,"details")) { free(content); 

			sendDetailsResponse(sock, cwd, object); return; }




		/////////////////////////////////////// get the object2
		aux=strstr(content,"object2=");
		if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }

		char object2[B_SIZE];
		aux1=object2; aux+=8; while(*aux && *aux!='\r') {*aux1=*aux; aux1++; aux++;}; *aux1=0;


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


		////////////////////////////////////////////// SYM LINK

		if(!strcmp(action,"symlink")) { free(content); 
			if(strcmp(object,object2)) {
				sprintf(line,"ln -s \"%s/%s\" \"%s/%s\"",cwd,object,cwd,object2);
				system(line);
			}
			sendListResponse(sock, cwd); return; }






		////////////////////

		puts(action);
		puts(object);
		puts(object2);
		puts(cwd);


		free(content);
		} // END CONTENT-TYPE PLAIN/TEXT
	else
	if(!strncasecmp(content_type,"multipart/form-data",19)) {

		// Content type is multipart/form-data; boundary=---------------------------6065758406394847092843530405
		//
		char *boundary=content_type+19;
		while(*boundary!='=') boundary++;
		boundary++;
		processMultipartPost(sock,content_len,boundary);
		return;
	}



	//			secret=ola&action=list&object=&object2=&cwd=
	//
	//


	sendHttpStringResponse(sock, "200 Ok", "text/html", "OK");

}





/// SEND DETAILS / PROPERTIES RESPONSE
///////////// TODO
		// TODO TODO TODO
//
void sendDetailsResponse(int sock, char *cwd, char *obj) {
	char list[500000], filename[200], *aux;
	struct stat sb;

	if(strcmp(cwd,"/")) sprintf(filename,"%s/%s",cwd,obj); else sprintf(filename,"/%s",obj);

	sprintf(list,"%s<body bgcolor=gray> \
		<form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		<input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		<p><font size=6>&nbsp; &nbsp; <b>%s</b></font> \
		<p><input type=button value=\" CANCEL \" onclick=\"act('list','','');\"></p><hr> \
		",HTML_HEADER,access_secret,cwd,filename);

	aux=list+strlen(list);

	if (lstat(filename, &sb) == -1) {
		perror("stat");
	} else {
		strcat(aux,"<table border=0 cellspacing=10 cellpadding=10<tr><td bgcolor=lightgray>This object is a <b>");
		aux=aux+strlen(aux);
		switch (sb.st_mode & S_IFMT) {
			case S_IFBLK:	strcat(aux, "block device</b>");	break;
			case S_IFCHR:	strcat(aux, "character device</b>");	break;
			case S_IFDIR:	strcat(aux, "directory</b>");		break;
			case S_IFIFO:	strcat(aux, "FIFO/Pipe</b>");		break;
			case S_IFLNK:
				char fullName[B_SIZE], linkTarget[B_SIZE];
				sprintf(fullName,"%s/%s",cwd,obj);
				int l_size=readlink(fullName, linkTarget, B_SIZE);
				linkTarget[l_size]=0;
				struct stat prop;
				chdir(cwd);
				if(stat(linkTarget,&prop)) sprintf(aux,"symbolic link</b> (<b>%s</b> &rarr; <font color=red><b>%s</b></font>)", obj, linkTarget);
				else sprintf(aux,"symbolic link</b> (<b>%s</b> &rarr; <font color=green><b>%s</b></font>)", obj, linkTarget); 
			break;
			case S_IFREG:	sprintf(aux, "regular file</b> with %li bytes",sb.st_size);		break;
			case S_IFSOCK:	strcat(aux, "socket</b>");		break;
			default: strcat(aux, "an unknown object type</b>");	break;
		}
		
		// timestamps
		//
		strcat(aux,"</td><td bgcolor=lightgray>");
		aux=aux+strlen(aux);
		sprintf(aux,"<small>%s was the last status change<br>%s was the last access<br>%s was the last modification</small>", ctime(&sb.st_ctime), ctime(&sb.st_atime), ctime(&sb.st_mtime));


		strcat(aux,"</td></tr></table>");





	}








	strcat(aux,HTML_BODY_FOOTER);
	sendHttpStringResponse(sock, "200 Ok", "text/html", list);
	return;
}






/// the list response also contains several general options, e.g, create 

void sendListResponse(int sock, char *cwd) {
	char list[5000], *aux;
	DIR *d;
	struct dirent *e;

	FILE *tmpFile=tmpfile();

	sprintf(list,"%s<body bgcolor=gray> \
		       <form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		       <input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		       <p><font size=6>&nbsp; &nbsp; Current working directory: <b>%s</b></font> \
		       <p><table width=100%% border=0 cellspacing=3><tr> \
		       <td align=center valign=top style=\"width:250px\"><details><summary>CREATE</summary><p><input id=mkobjname type=text> \
		       <p><input type=button value=\"FOLDER\" onclick=\"act('mkdir',document.getElementById('mkobjname').value,'');\"> \
		       <input type=button value=\" FILE \" onclick=\"act('mkfile',document.getElementById('mkobjname').value,'');\"></p></details</td> \
		       <td></td>",HTML_HEADER,access_secret,cwd,cwd);

	strcat(list,"<td align=center valign=top style=\"width:300px\"><details><summary>Upload file from URL (wget)</summary><p>");
	if(wget_command) {
		strcat(list,"<input id=urlwget type=url name=urlwget> \
			<p><input type=button value=\"DOWNLOAD\" onclick=\"act('wget',document.getElementById('urlwget').value,'');\"></p></details></td>");
	}
	else {
		strcat(list,"Sorry, the wget command is not available</p></details></td>");
	}


	// Upload files from browser
	//
	aux=list+strlen(list);

	sprintf(aux,"<script> \
			function uploadfiles() { \
			document.getElementById(\"upF\").submit(); \
			document.getElementById(\"msg\").innerHTML=\"<br><b><font color=green size=4>Uploading, please wait ...</font><br><br></b>\";  \
			var visibility = 'hidden'; window.setInterval(function() { document.getElementById(\"msg\").style.visibility = visibility; \
			visibility = (visibility === 'visible') ? 'hidden' : 'visible'; }, 300); } \
			</script>");

	aux=aux+strlen(aux);

	// bgcolor=#cfcfcf

	sprintf(aux,"<td align=center valign=top style=\"width:300px\"><details><summary>Upload files from browser</summary> \
			<table width=90%% border=0><tr><td style=\"width:300px\" bgcolor=#cfcfcf align=center valign=center> \
			<br><form name=upload enctype=multipart/form-data method=POST id=upF action=/filemanager><input type=hidden name=secret value=\"%s\"><input type=hidden name=action value=upload> \
			<input type=hidden name=cwd value=\"%s\">&nbsp;<input type=file name=filename multiple onchange=uploadfiles()> \
			</form><div id=msg></div></td></tr></table></details></td>", access_secret, cwd);


	// The Clipboard
	//
	char clipboardContent[B_SIZE];
	d=opendir(clipboard_folder);
	e=readdir(d);
	while(e) {
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) break;
		e=readdir(d);
	}

	if(e) { strcpy(clipboardContent,e->d_name); if(e->d_type==DT_DIR) strcat(clipboardContent,"/"); else if(e->d_type==DT_LNK) strcat(clipboardContent," &rarr;");}
	else *clipboardContent=0;
	closedir(d);

	aux=aux+strlen(aux);
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
	if(strcmp(cwd,root_folder)) strcat(aux,"<p><a href=\"javascript:act('cdup','','');\"><b> ../ <i>(parent folder)</i></b></a></li>");

	fwrite(list,1,strlen(list),tmpFile);


	int elem=0;  // used to grant unique html IDs
	do {
		e=readdir(d);
		if(!e) break;
		// use the details tag
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) {
			elem++;
			aux=list;
			strcpy(aux,"<p><details><summary>&nbsp;&nbsp;&nbsp;&nbsp;");
			// if a folder, permit cd into it
			aux=aux+strlen(aux);

			if(e->d_type==DT_DIR) sprintf(aux,"<a href=\"javascript:act('cd','%s','');\"><b>%s/</b></a>",e->d_name,e->d_name);
			else if(e->d_type== DT_LNK) {
				// TODO: haldle the sym link case
				char fullName[B_SIZE], linkTarget[B_SIZE];
				sprintf(fullName,"%s/%s",cwd,e->d_name);\
				int l_size=readlink(fullName, linkTarget, B_SIZE);
				linkTarget[l_size]=0;
				struct stat prop;
				chdir(cwd);
				if(stat(linkTarget,&prop)) sprintf(aux,"<b>%s</b> &rarr; <font color=red><b>%s</b></font>", e->d_name,linkTarget);
				else
				if(S_ISDIR(prop.st_mode)) sprintf(aux,"<b>%s</b> &rarr; <a href=\"javascript:act('cd','%s','');\"><b>%s/</b></a>", e->d_name,linkTarget,linkTarget); 
				else sprintf(aux,"<b>%s</b> &rarr; <font color=green><b>%s</b></font>", e->d_name,linkTarget); 
				


			}
			else sprintf(aux,"<b>%s</b>",e->d_name);

			strcat(aux,"</summary><p><table width=100%% border=0 cellspacing=3><tr>");

			aux=aux+strlen(aux);
			sprintf(aux,"<td align=center valign=top style=\"width:250px\"><input type=button value=\"Properties\" onClick=\"javascript:act('details','%s','');\"></td>", e->d_name);

			aux=aux+strlen(aux);
			if(e->d_type==DT_REG) sprintf(aux,"<td align=center valign=top style=\"width:250px\"><input type=button value=\"Download\" onClick=\"javascript:act('download','%s','');\"></td>",
					e->d_name);

			// RENAME or CLONE 
			aux=aux+strlen(aux);
			sprintf(aux,"<td align=center valign=top style=\"width:250px\"><details><summary>RENAME/CLONE</summary> \
		       		<p><input id=ren%i type=text value=\"%s\"> \
				<p><input type=button value=Rename onclick=\"act('rename','%s',document.getElementById('ren%i').value);\"> \
				<input type=button value=Sym.link onclick=\"act('symlink','%s',document.getElementById('ren%i').value);\"> \
		       		<input type=button value=Clone onclick=\"act('clone','%s',document.getElementById('ren%i').value);\"></p></details</td>",
				elem, e->d_name, e->d_name, elem, e->d_name, elem, e->d_name, elem);

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
			fwrite(list,1,strlen(list),tmpFile);
			}


		}
	while(e);
	closedir(d);
	strcpy(list,HTML_BODY_FOOTER);
	fwrite(list,1,strlen(list),tmpFile);
	sendHttpFileContent(sock, tmpFile, "200 Ok", "text/html");
	fclose(tmpFile); // this also removes the temporary file
	return;
}



void sendHttpFileDownloadResponse(int sock, char *cwd, char *obj) {
	FILE *f;
	char aux[2000];
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
		done=fread(aux,1,2000,f);
		if(done>0) write(sock,aux,done);
		}
	while(done);
	fclose(f);
}




		////////////////////////////////////////////// UPLOAD FROM BROWSER (supports multiple files)
		//
		// TODO - work in progress
		// The uploading of multiple files is ok and tested, however, on the browser's side a kind of a progress indicator is required.
		//
		//
		

void processMultipartPost(int sock, long content_len, char *bound) {
	char buffer[8*B_SIZE];
	char cwd[B_SIZE], filename[B_SIZE], boundary[B_SIZE];
	long todo=content_len;
	long done;
	char authorized=0;
	int p, boundaryLen;

	strcpy(boundary,"\r\n--"); strcat(boundary,bound);
	boundaryLen=strlen(boundary);

	//printf("Processing multipart/form-data POST request, boundary is \"%s\", boundary length is %i the content length is %li\n", boundary, boundaryLen, content_len);

 	*cwd=0;

	while(todo) {
		readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer);
		//puts(buffer);
		if(strstr(buffer,"name=\"secret\"")) {
			while(*buffer) { readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer); }  // read lines until an empty line (end of the part's header)
			readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer);
			if(strcmp(buffer,access_secret)) {
				//puts("Bad secret");
				sprintf(buffer,"%s<body bgcolor=yellow><h1>Sorry, access denied.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", buffer); return;
			}
			authorized=1;
		}
		else
		if(strstr(buffer,"name=\"cwd\"")) {
			while(*buffer) { readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer); } // read lines until an empty line (end of the part's header)
			readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer); strcpy(cwd,buffer);
		}
		else
		if(strstr(buffer,"name=\"filename\"")) {		// A PART WITH A FILE
			if(!authorized) puts("Secret not received");
			if(!*cwd) puts("CWD not received");
			char *aux=strstr(buffer,"filename=\""); 
			aux+=10;
			char *aux1=strstr(aux,"\"");
			*aux1=0; strcpy(filename,aux);
			printf("Filename: %s/%s\n",cwd,filename);
			while(*buffer) { readLineCRLF(sock,buffer); todo=todo-2-strlen(buffer); } // read lines until an empty line (end of the part's header)
			
			sprintf(buffer,"%s/%s",cwd,filename);
			FILE *f=fopen(buffer,"w+");


			//
			// read the file content until the boundary is detected
			//
	
			while(todo) {
				done=read(sock,buffer,1); 
				if(done<0) { puts("Fatal error: error reading multipart/form-data POST"); return; }
				todo-=done;
				if(done) {
					p=0;
					while(buffer[p]==boundary[p]) {
						p++; if(p==boundaryLen) break;
						done=read(sock,&buffer[p],1); 
						if(done<0) { puts("Fatal error: error reading multipart/form-data POST"); return; }
						todo-=done;
						if(buffer[p]=='\r') { // could be a boundary start
							fwrite(buffer,1,p,f);
							p=0; buffer[p]='\r';
						}
					}
					if(p==boundaryLen) break; // it's the boundary sequence
					fwrite(buffer,1,p+1,f);
				}

			}
			fclose(f);
		}  // end receive file upload


	}
	sendListResponse(sock, cwd);
	return; 
}
	

		




/*	int i;
	char line[1000];
	FILE *f=fopen("upload.raw","w");

	todo=content_len;
	while(todo) {
		if(todo>1000) i=read(sock,line,1000); else i=read(sock,line,todo);
		if(i<0) { puts("Fatal error: error reading POST data"); return; }
		fwrite(line,1,i,f);
		todo-=i;
		}
	fclose(f);
	sendHttpStringResponse(sock, "200 Ok", "text/html", "OK");
*/






		// OLD STUFF BELOW, TODO: remove this later on
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




