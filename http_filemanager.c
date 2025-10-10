#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

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
	long i, todo;
	char *aux, *aux1, *content;
	DIR *d;
	struct dirent *e;

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
	printf("Content length is %li\n",content_len);

	//if(!strcasecmp(content_type,HTTP_CONTENT_TYPE_FORM_URLENCODED)) {
	if(!strcasecmp(content_type,"text/plain")) {
		// read the entire content to memory

		todo=content_len;
		content=malloc(content_len+1);
		if(!content) { puts("Fatal error allocating memory for POST data"); return; }
		aux=content;
		while(todo) {
			i=read(sock,aux,todo);
			if(i<0) { puts("Fatal error: error reading POST data"); free(content); return; }
			aux+=i; todo-=i;
			}
		content[content_len]=0;

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

		char action[B_SIZE];
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


		////////////////// for other commands the slash is not allowed in the object name - REMOVE SLASHES FROM OBJECT
		
		aux=object;
		while(*aux) {
			if(*aux=='/') {
				aux1=aux+1; while(*aux1) { *(aux1-1)=*aux1; aux1++; }
				*(aux1-1)=0;
			}
			if(*aux!='/') aux++;
		}

		if(!*object) { free(content); sendListResponse(sock, cwd); return; }

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

			sprintf(line,"cp -fdR \"%s/%s\" \"%s/\"",cwd,object,clipboard_folder);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// DELETE CLIPBOARD OBJECT 

		if(!strncmp(action,"deleteclip",10)) { free(content); 
			if(!strcmp(action,"deleteclipALL")) sprintf(line,"rm -Rf \"%s/\"* \"%s/\".*",clipboard_folder,clipboard_folder);
			else sprintf(line,"rm -Rf \"%s/%s\"",clipboard_folder,object);
			system(line);
			sendListResponse(sock, cwd); return; }

		////////////////////////////////////////////// PASTE FROM OBJECT

		if(!strncmp(action,"pasteclip",9)) {
			free(content); 
			if(!strcmp(action,"pasteclipALL")) {
				d=opendir(clipboard_folder);  // this approach avoids issueis when copying all objects starting with a dot (the issue is .. will be included)
				e=readdir(d);
				while(e) {
					 if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) {
						sprintf(line,"cp -fdR \"%s/%s\" \"%s/\"", clipboard_folder,e->d_name,cwd);
						system(line);
					 }
				e=readdir(d);
				}
				closedir(d);
			}
			else {
				sprintf(line,"cp -fdR \"%s/%s\" \"%s/\"",clipboard_folder,object,cwd);
				system(line);
			}
			sendListResponse(sock, cwd); return; 
		}

		////////////////////////////////////////////// DETAILS

		if(!strcmp(action,"details")) { free(content); 

			sendDetailsResponse(sock, cwd, object); return; }


		////////////////////////////////////////////// VIEW / EDIT TEXT FILE

		if(!strcmp(action,"viewedit")) { free(content); 

			sendTextFileEditorResponse(sock, cwd, object); return; }


		////////////////////////////////////////////// VIEW / EDIT TEXT FILE - SAVE FILE (TODO)

		if(!strncmp(action,"viewedit-save",13)) {
			aux=strstr(content,"usertext=");
			if(!aux) { sprintf(line,"%s<body bgcolor=yellow><h1>Sorry, access denied due to incomplete POST data.</h1>%s",HTML_HEADER,HTML_BODY_FOOTER);
				sendHttpStringResponse(sock, "401 Unauthorized", "text/html", line); free(content); return; }
			sprintf(line,"%s/%s",cwd,object);
			FILE *f=fopen(line,"w");
			aux=aux+9;

			// issue: one initial newline is being removed in each save - TODO trying to find why !!!
			
			content[content_len-2]=0; // the last CR+LF is not part of the textarea field, so remove the last two bytes
			while(*aux) {
				fwrite(aux,1,1,f);
				aux++;
				}
			fclose(f);
			free(content);
			if(!strcmp(action,"viewedit-save-close")) sendListResponse(sock, cwd);
			else sendTextFileEditorResponse(sock, cwd, object);
			return;
		}





		/////////////////////////////////////// The remaining actions require "object2"
		//
		/////////////////////////////////////// Get "object2" for actions that require "object2"
		//
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
			sendListResponse(sock, cwd); return; 
		}


		////////////////////////////////////////////// CHOWN

		if(!strcmp(action,"chown")) { free(content); 
			sprintf(line,"chown -R %s \"%s/%s\"",object2,cwd,object);
			system(line);
			sendDetailsResponse(sock, cwd, object); return;
		}


		////////////////////////////////////////////// CHGRP

		if(!strcmp(action,"chgrp")) { free(content); 
			sprintf(line,"chgrp -R %s \"%s/%s\"",object2,cwd,object);
			system(line);
			sendDetailsResponse(sock, cwd, object); return;
		}


		////////////////////////////////////////////// CHMOD

		if(!strcmp(action,"chmod")) { free(content); 
			sprintf(line,"chmod -R %s \"%s/%s\"",object2,cwd,object);
			system(line);
			sendDetailsResponse(sock, cwd, object); return;
		}



		////////////////////

		/* puts(action);
		puts(object);
		puts(object2);
		puts(cwd); */


		free(content);
		} // END CONTENT-TYPE PLAIN/TEXT
		  //
		  //
	else // content is not text/plain
	if(!strncasecmp(content_type,"multipart/form-data",19)) {

		// Content type is multipart/form-data; boundary=---------------------------6065758406394847092843530405
		//
		char *boundary=content_type+19;
		while(*boundary!='=') boundary++;
		boundary++;
		processMultipartPost(sock,content_len,boundary);
		return; 
	}

	sendHttpStringResponse(sock, "200 Ok", "text/html", "OK");
}





	/// SEND DETAILS / PROPERTIES RESPONSE
	///////////// TODO: view/edit
	//

void sendDetailsResponse(int sock, char *cwd, char *obj) {
	char list[10*B_SIZE], filename[B_SIZE], *aux;
	char commandLine[2*B_SIZE];
	char typeDesc[B_SIZE];
	int c, fileMaxContent=6*B_SIZE;
	char isText, isFile;
	unsigned char fileContent[fileMaxContent+1];
	FILE *p;
	struct stat sb;
	FILE *tmpFile=tmpfile();

	if(strcmp(cwd,"/")) sprintf(filename,"%s/%s",cwd,obj); else sprintf(filename,"/%s",obj);

	sprintf(list,"%s<body bgcolor=gray> \
		<form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		<input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		<p><img src=/favicon.ico width=32 height=32><font size=6> &nbsp; <i>Properties/details about</i> [<b>%s</b>]</font> \
		<p><input type=button value=\" CLOSE \" onclick=\"act('list','','');\"></p><hr> \
		",HTML_HEADER,access_secret,cwd,filename);

	aux=list+strlen(list);



	strcat(aux,"<table border=0 cellspacing=3 cellpadding=10<tr><td bgcolor=lightgray align=center><b>Object type and timestamp</b></td> \
			<td bgcolor=lightgray align=center style=\"width:200px\"><b>User</b><br><small>(owner)</small></td><td bgcolor=lightgray align=center style=\"width:200px\"><b>Group</b></td> \
			<td bgcolor=lightgray align=center style=\"width:200px\"><b>User permissions</b></td><td bgcolor=lightgray align=center style=\"width:200px\"><b>Group permissions</b></td> \
			<td bgcolor=lightgray align=center style=\"width:200px\"><b>Others permissions</b></td> \
			<td bgcolor=lightgray align=center style=\"width:200px\"><b>Special permissions</b></td></tr>");
	aux=aux+strlen(aux);
	
	isFile=0;
	if (lstat(filename, &sb) == -1) {
		perror("lstat() error");
		strcat(aux,"<tr><td bgcolor=lightgray><font color=red>Sorry, the <i>lstat()</i> function returned an error.</font></td><td bgcolor=lightgray><font color=red>Sorry, the <i>lstat()</i> function returned an error.</font></td>");
		aux=aux+strlen(aux);
	} else {
		strcat(aux,"<tr><td align=center bgcolor=lightgray>");
		aux=aux+strlen(aux);
		switch (sb.st_mode & S_IFMT) {
			case S_IFBLK:	strcat(aux, "Block device");	break;
			case S_IFCHR:	strcat(aux, "Character device");	break;
			case S_IFDIR:	strcat(aux, "Directory");		break;
			case S_IFIFO:	strcat(aux, "FIFO/Pipe");		break;
			case S_IFLNK:
				char fullName[B_SIZE], linkTarget[B_SIZE];
				sprintf(fullName,"%s/%s",cwd,obj);
				int l_size=readlink(fullName, linkTarget, B_SIZE);
				linkTarget[l_size]=0;
				struct stat prop;
				chdir(cwd);
				if(stat(linkTarget,&prop)) sprintf(aux,"Symbolic link> (<b>%s</b> &rarr; <font color=red><b>%s</b></font>)", obj, linkTarget);
				else sprintf(aux,"Symbolic link (<b>%s</b> &rarr; <font color=green><b>%s</b></font>)", obj, linkTarget); 
			break;
			case S_IFREG:	sprintf(aux, "Regular file with %li bytes",sb.st_size); isFile=1; break;
			case S_IFSOCK:	strcat(aux, "Socket");		break;
			default: strcat(aux, "Unknown object type>");	break;
		}
		


		// timestamps
		aux=aux+strlen(aux);
		sprintf(aux,"<p><small>Last modification: %s</small></td>", ctime(&sb.st_mtime));


		// owner and group
		aux=aux+strlen(aux);
		sprintf(aux,"<td bgcolor=lightgray align=center>%s (UID=%i)<br>", getpwuid(sb.st_uid)->pw_name, sb.st_uid);
		aux=aux+strlen(aux);
		sprintf(aux,"<details><summary>Change User</summary><p><input id=newownname type=text><p><input type=button value=\"Change\" \
				onclick=\"act('chown', '%s', document.getElementById('newownname').value);\"></p></details></td>",obj);

		aux=aux+strlen(aux);
		sprintf(aux,"<td bgcolor=lightgray align=center>%s (GID=%i)<br>", getgrgid(sb.st_gid)->gr_name, sb.st_gid);
		aux=aux+strlen(aux);
		sprintf(aux,"<details><summary>Change Group</summary><p><input id=newgrpname type=text><p><input type=button value=\"Change\" \
				onclick=\"act('chgrp', '%s', document.getElementById('newgrpname').value);\"></p></details></td>",obj);


		fwrite(list,1,strlen(list),tmpFile);
		aux=list;


		// PERMISSIONS
		//
		char buttons[B_SIZE];
		char *aB;
		
		// OWNER (USER) permissions
		strcpy(aux,"<td bgcolor=lightgray align=center>");
		aB=buttons;*aB=0;
		strcat(aB,"<br><details><summary>Change</summary><p>");
		if (sb.st_mode & S_IRUSR) {strcat(aux,"[R] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[R]\" onclick=\"act('chmod','%s','u-r');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[R]\" onclick=\"act('chmod', '%s', 'u+r');\">",obj); }
		if (sb.st_mode & S_IWUSR) {strcat(aux," [W] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[W]\" onclick=\"act('chmod','%s','u-w');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[W]\" onclick=\"act('chmod','%s','u+w');\">",obj); }
		if (sb.st_mode & S_IXUSR) {strcat(aux," [X] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[X]\" onclick=\"act('chmod','%s','u-x');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[X]\" onclick=\"act('chmod','%s','u+x');\">",obj); }
		strcat(aB,"</p></details></td>");
		strcat(aux,buttons);

		// GROUP permissions
		aux=aux+strlen(aux);
		strcat(aux,"<td bgcolor=lightgray align=center>");
		aB=buttons;*aB=0;
		strcat(aB,"<br><details><summary>Change</summary><p>");
		if (sb.st_mode & S_IRGRP) {strcat(aux," [R] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[R]\" onclick=\"act('chmod','%s','g-r');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[R]\" onclick=\"act('chmod', '%s', 'g+r');\">",obj); }
		if (sb.st_mode & S_IWGRP) {strcat(aux," [W] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[W]\" onclick=\"act('chmod','%s','g-w');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[W]\" onclick=\"act('chmod','%s','g+w');\">",obj); }
		if (sb.st_mode & S_IXGRP) {strcat(aux," [X] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[X]\" onclick=\"act('chmod','%s','g-x');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[X]\" onclick=\"act('chmod','%s','g+x');\">",obj); }
		strcat(aB,"</p></details></td>");
		strcat(aux,buttons);

		// OTHERS permissions
		aux=aux+strlen(aux);
		strcat(aux,"<td bgcolor=lightgray align=center>");
		aB=buttons;*aB=0;
		strcat(aB,"<br><details><summary>Change</summary><p>");
		if (sb.st_mode & S_IROTH) {strcat(aux," [R] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[R]\" onclick=\"act('chmod','%s','o-r');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[R]\" onclick=\"act('chmod', '%s', 'o+r');\">",obj); }
		if (sb.st_mode & S_IWOTH) {strcat(aux," [W] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[W]\" onclick=\"act('chmod','%s','o-w');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[W]\" onclick=\"act('chmod','%s','o+w');\">",obj); }
		if (sb.st_mode & S_IXOTH) {strcat(aux," [X] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[X]\" onclick=\"act('chmod','%s','o-x');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[X]\" onclick=\"act('chmod','%s','o+x');\">",obj); }
		strcat(aB,"</p></details></td>");
		strcat(aux,buttons);

		// special permissions
		aux=aux+strlen(aux);
		strcat(aux,"<td bgcolor=lightgray align=center>");
		aB=buttons;*aB=0;
		strcat(aB,"<br><details><summary>Change</summary><p>");
		if (sb.st_mode & S_ISUID) {strcat(aux," [SetUID] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[SetUID]\" onclick=\"act('chmod','%s','u-s');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[SetUID]\" onclick=\"act('chmod', '%s', 'u+s');\">",obj); }
		if (sb.st_mode & S_ISGID) {strcat(aux," [SetGID] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[SetGID]\" onclick=\"act('chmod','%s','g-s');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[SetGID]\" onclick=\"act('chmod','%s','g+s');\">",obj); }
		if (sb.st_mode & S_ISVTX) {strcat(aux," [Sticky] "); aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"-[Sticky]\" onclick=\"act('chmod','%s','-t');\">",obj); }
		else { aB=aB+strlen(aB); sprintf(aB,"<input type=button value=\"+[Sticky]\" onclick=\"act('chmod','%s','+t');\">",obj); }
		strcat(aB,"</p></details></td>");
		strcat(aux,buttons);
		strcat(aux,"</tr></table>");

	} // lstat() success

	fwrite(list,1,strlen(list),tmpFile);
	aux=list;

	strcpy(aux,"<table border=0 cellspacing=0 cellpadding=10<tr><td bgcolor=#c0c0d0 align=center style=\"width:600px\"><b>Signature analysis by the <i>file</i> command (magic-number)</b></td></tr><tr>");

	// file's content type (uses the file command, if available)
	if(file_command) {
		sprintf(commandLine, "%s -b \"%s\"", file_command, filename);
		p=popen(commandLine,"r");
		fgets(typeDesc,300,p);
		pclose(p);
	} else {
		strcpy(typeDesc, "<td bgcolor=#c0c0d0><font color=red>Sorry, the <i>file</i> command is not available.</font></td>");
	}
	aux=aux+strlen(aux);
	sprintf(aux,"<td bgcolor=#c0c0d0><textarea cols=120 rows=3 readonly disabled>%s</textarea></td>", typeDesc);


	strcat(aux,"</tr>");


	// file's content (probe for text file)
	//
	if(isFile) {
		p=fopen(filename,"r"); c=0; isText=1;
		while(fread(&fileContent[c],1,1,p)) {
			//if(fileContent[c]>127) { isText=0; fileContent[c]=32; }
			//else
			if(fileContent[c]<32) {
				switch(fileContent[c]) {
					case 13: // CR
						break;
					case 10: // LF
						break;
					case 9: // TAB
						break;
					default:
						//printf("Bad caracter: %u\n",fileContent[c]);
						isText=0; fileContent[c]=32;
				}
			}
			c++; 
			if(c==fileMaxContent) break;
		}
		fclose(p);
		fileContent[c]=0;
		if(isText) strcat(aux,"<tr><td bgcolor=#c0d0c0 align=center><b>Content sample (text file)</b></td></tr>");
		else strcat(aux,"<tr><td bgcolor=#c0d0c0 align=center><b>Content sample (NOT A TEXT FILE)</b></td></tr>");
		aux=aux+strlen(aux);
		sprintf(aux,"<tr><td bgcolor=#c0d0c0><textarea cols=120 rows=30 readonly disabled>%s</textarea></td></tr></table>", fileContent);



	}


	strcat(aux,"</tr></table>");
	strcat(aux,HTML_BODY_FOOTER);
	fwrite(list,1,strlen(list),tmpFile);
	sendHttpFileContent(sock, tmpFile, "200 Ok", "text/html");
	fclose(tmpFile); // this also removes the temporary file
	return;
}






/// the list response  - DIRECTORY CONTENT LISTING
//
//

void sendListResponse(int sock, char *cwd) {
	char list[10*B_SIZE], *aux;
	DIR *d;
	struct dirent *e;
	int i;
	FILE *tmpFile=tmpfile();

	sprintf(list,"%s<body bgcolor=gray> \
		       <form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		       <input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		       <p><img src=/favicon.ico width=32 height=32><font size=6> &nbsp; <i>Directory content listing for</i> [<b>%s</b>]</font> \
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
	sprintf(aux,"<td align=center valign=top style=\"width:300px\"><details><summary>Upload files from browser</summary> \
			<table width=90%% border=0><tr><td style=\"width:300px\" bgcolor=#cfcfcf align=center valign=center> \
			<br><form name=upload enctype=multipart/form-data method=POST id=upF action=/filemanager><input type=hidden name=secret value=\"%s\"><input type=hidden name=action value=upload> \
			<input type=hidden name=cwd value=\"%s\">&nbsp;<input type=file name=filename multiple onchange=uploadfiles()> \
			</form><div id=msg></div></td></tr></table></details></td>", access_secret, cwd);



	fwrite(list,1,strlen(list),tmpFile);




	// CLIPBOARD
	//
	//
	
	strcpy(list,"<td align=center valign=top style=\"width:450px\"><details><summary>CLIPBOARD</summary><p><table width=100% border=0 cellpadding=2>");
	aux=list+strlen(list);
	
	d=opendir(clipboard_folder);
	e=readdir(d);
	i=0;
	while(e) {
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) {
			sprintf(aux,"<tr><td align=center style=\"width:200px\" style=\"vertical-align:middle\"><small><b>%s", e->d_name);
			if(e->d_type==DT_DIR) strcat(aux,"/"); else if(e->d_type==DT_LNK) strcat(aux," &rarr;");
			aux=aux+strlen(aux);
			sprintf(aux,"</b></small></td><td align=center style=\"width:200px\"><input type=button value=\"PASTE\" onclick=\"act('pasteclip','%s','');\"> &nbsp; \
					<input type=button value=\"DELETE\" onclick=\"act('deleteclip','%s','');\"></td></tr>", e->d_name, e->d_name);
			aux=aux+strlen(aux);
			i++;
		}
		e=readdir(d);
	}
	closedir(d);
	if(i>1) {
		strcpy(aux,"<tr><td colspan=2 align=center><hr><input type=button value=\"PASTE ALL\" onclick=\"act('pasteclipALL','all','');\">&nbsp; \
			         &nbsp; <input type=button value=\"DELETE ALL\" onclick=\"act('deleteclipALL','all','');\"></td></tr>");
		aux=aux+strlen(aux);
	}
	strcpy(aux,"</table></details></tr></table></p><hr>");










	// Add to the list the current folder listing contents
	d=opendir(cwd);
	printf("Listing folder %s\n",cwd);
	if(!d) { sprintf(list,"%s<body bgcolor=yellow><h1>Failed to open directory %s for listing.</h1>%s",HTML_HEADER,cwd,HTML_BODY_FOOTER);
			           fclose(tmpFile); sendHttpStringResponse(sock, "500 Internal Server Error", "text/html", list); return; }
	
	// add ../ for cdup
	if(strcmp(cwd,root_folder)) strcat(list,"<p><a href=\"javascript:act('cdup','','');\"><b> ../ <i>(parent folder)</i></b></a></li>");

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

			// DETAILS
			aux=aux+strlen(aux);
			sprintf(aux,"<td align=center valign=top style=\"width:200px\"><input type=button value=\"Properties\" onClick=\"javascript:act('details','%s','');\"></td>", e->d_name);

			//
			// VIEW-EDIT TODO - TODO view and edit file content (if it's text)
			aux=aux+strlen(aux);
			if(e->d_type==DT_REG) sprintf(aux,"<td align=center valign=top style=\"width:200px\"><input type=button value=\"View/Edit\" onClick=\"javascript:act('viewedit','%s','');\"></td>",
					e->d_name);
			// DOWNLOAD
			aux=aux+strlen(aux);
			if(e->d_type==DT_REG) sprintf(aux,"<td align=center valign=top style=\"width:200px\"><input type=button value=\"Download\" onClick=\"javascript:act('download','%s','');\"></td>",
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

			// COPY TO CLIPBOARD
			aux=aux+strlen(aux);
			sprintf(aux,"<td align=center valign=top style=\"width:200px\"><input type=button value=\"COPY to CLIPBOARD\" onClick=\"javascript:act('copytoclip','%s','');\"></td>",
					e->d_name);
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
}
	




void sendTextFileEditorResponse(int sock, char *cwd, char *obj) {
	char list[10*B_SIZE];
	char filename[B_SIZE];
	int done, col, c, fileMaxContent=6*B_SIZE;
	char isText;
	unsigned char fileContent[fileMaxContent+1];
	FILE *f, *tmpFile=tmpfile();

	if(strcmp(cwd,"/")) sprintf(filename,"%s/%s",cwd,obj); else sprintf(filename,"/%s",obj);

	sprintf(list,"%s<body bgcolor=gray> \
		<form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		<input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'> \
		<p><img src=/favicon.ico width=32 height=32><font size=6> &nbsp; <i>View/Edit file</i> [<b>%s</b>]</font> \
		",HTML_HEADER,access_secret,cwd,filename);
	fwrite(list,1,strlen(list),tmpFile);

	// probe the file to check if it's text
	// at the same time prepare a content to be displayed in case it's not a text file
	f=fopen(filename,"r"); col=0; c=0; isText=1;
	while(fread(&fileContent[c],1,1,f)) {
		if(fileContent[c]<32) {
			switch(fileContent[c]) {
				case 13: // CR
					col=0;
					break;
				case 10: // LF
					col=0;
					break;
				case 9: // TAB
					break;
				default:
					//printf("Bad caracter: %u\n",fileContent[c]);
					isText=0; fileContent[c]=32;
				}
		}
		c++; 
		if(c==fileMaxContent) break;
		col++; if(col>150) { fileContent[c]=10; c++; if(c==fileMaxContent) break; col=0;}
	}
	fclose(f);
	fileContent[c]=0;

	if(isText) {
		sprintf(list,"<p><table border=0 cellspacing=3><tr><td align=center valign=top style=\"width:400px\"><details><summary>CLOSE</summary> \
				<p><input type=button value=\" LOSE CHANGES AND CLOSE \" onclick=\"act('list','','');\"> &nbsp; \
				<input type=button value=\" SAVE AND CLOSE \" onclick=\"act('viewedit-save-close','%s','');\"></details></p></td> \
				<td align=center valign=top style=\"width:300px\"><details><summary>SAVE / SAVE AS</summary> \
				<p><input id=saveasname type=text value='%s' size=30'></p><p><input type=button value=\" SAVE \" onclick=\"act('viewedit-save',document.getElementById('saveasname').value,'');\"></p> \
				</details></td></table> \
				<hr><p><textarea cols=150 rows=40 name=usertext>", obj, obj);

		fwrite(list,1,strlen(list),tmpFile);

		f=fopen(filename,"r"); // read the file content into the textarea - TODO: issue if the file contains the </textarea> html tag
		do {
			done=fread(fileContent,1,B_SIZE,f);
			if(done) fwrite(fileContent,1,done,tmpFile);
		}
		while(done);
		fclose(f);
		strcpy(list,"</textarea></form>");
	}
	else {
		sprintf(list,"</form><p><input type=button value=\" CLOSE \" onclick=\"act('list','','');\"></p><hr><h3>Sorry, this is <u>NOT A TEXT FILE</u>, you can't edit it with this text editor.</h3> \
				<p>Here is a readonly view of parts of the file's content: \
				<p><textarea cols=150 rows=40 readonly disabled>%s</textarea>", fileContent);
	}
	
	strcat(list,HTML_BODY_FOOTER);
	fwrite(list,1,strlen(list),tmpFile);
	sendHttpFileContent(sock, tmpFile, "200 Ok", "text/html");
	fclose(tmpFile); // this also removes the temporary file
}
		

