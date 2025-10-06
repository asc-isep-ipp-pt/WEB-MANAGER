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
//



void sendDetailsResponse(int sock, char *cwd, char *obj) {
	char list[500000], filename[200], *aux;
	char commandLine[300];
	char typeDesc[300];
	int col, c, fileMaxContent=2000;
	char isText, isFile;
	unsigned char fileContent[fileMaxContent];
	FILE *p;
	struct stat sb;

	if(strcmp(cwd,"/")) sprintf(filename,"%s/%s",cwd,obj); else sprintf(filename,"/%s",obj);

	sprintf(list,"%s<body bgcolor=gray> \
		<form name=main method=POST action=/filemanager enctype=text/plain><input type=hidden name=secret value='%s'><input type=hidden name=action value=list><input type=hidden name=object value=> \
		<input type=hidden name=object2 value=><input type=hidden name=cwd value='%s'></form> \
		<p><font size=7>&nbsp; &nbsp; <b>%s</b><br></font><font size=5>(%s)</font> \
		<p><input type=button value=\" CANCEL \" onclick=\"act('list','','');\"></p><hr> \
		",HTML_HEADER,access_secret,cwd,obj,filename);

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




		// PERMISSIONS
		//
		char buttons[500];
		char *aB;
		
		// OWNER (USER) permissions
		aux=aux+strlen(aux);
		strcat(aux,"<td bgcolor=lightgray align=center>");
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








	strcat(aux,"<table border=0 cellspacing=0 cellpadding=10<tr><td bgcolor=#c0c0d0 align=center style=\"width:600px\"><b>Signature analysis by the <i>file</i> command (magic-number)</b></td></tr><tr>");

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
	sprintf(aux,"<td bgcolor=#c0c0d0><textarea cols=120 rows=2 readonly disabled>%s</textarea></td>", typeDesc);


	strcat(aux,"</tr>");


	// file's content (probe for text file)
	//
	if(isFile) {
		p=fopen(filename,"r"); col=0; c=0; isText=1;
		while(fread(&fileContent[c],1,1,p)) {
			//if(fileContent[c]>127) { isText=0; fileContent[c]=32; }
			//else
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
			if(c==2000) break;
			col++; if(col>150) { fileContent[c]=10; c++; col=0;}
		}
		fclose(p);
		fileContent[c]=0;
		if(isText) strcat(aux,"<tr><td bgcolor=#c0d0c0 align=center><b>Content sample (text file)</b></td></tr>");
		else strcat(aux,"<tr><td bgcolor=#c0d0c0 align=center><b>Content sample (NOT A TEXT FILE)</b></td></tr>");
		aux=aux+strlen(aux);
		sprintf(aux,"<tr><td bgcolor=#c0d0c0><textarea cols=120 rows=30 readonly disabled>%s</textarea></td></tr></table>", fileContent);


		// TODO - if it's a text file, edit option with textFileEditor
		//
		//
		//



	}





	strcat(aux,"</tr></table>");
	strcat(aux,HTML_BODY_FOOTER);
	sendHttpStringResponse(sock, "200 Ok", "text/html", list);
	return;
}






/// the list response  - DIRECTORY CONTENT LISTING
//
//

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


			// COPY TO CLIPBOARD
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
	

		

