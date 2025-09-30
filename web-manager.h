#ifndef ___WEB_MANAGER_H
#define ___WEB_MANAGER_H


#define BASE_FOLDER "www"

#define HTML_BODY_FOOTER "<footer><hr><small>Ultra Basic Web Manager version 0.1, May 2023<br><i>Andr&eacute; Moreira (asc@isep.ipp.pt)</i></small></footer></body></html>"

#define HTML_HEADER "<html><head> <meta charset=\"UTF-8\"><title>Web Manager</title><script>function act(a,o,o2){document.main.action.value=a;document.main.object.value=o;document.main.object2.value=o2;document.main.submit();} \
	</script> \
	<style> details > summary {  padding: 4px; background-color: #c0c0c0; width: 90%; border: none; box-shadow: 2px 2px 3px #bbbbbb; cursor: pointer; } \
	details > p { background-color: #cfcfcf; width: 90%; padding: 4px; margin: 0; box-shadow: 2px 2px 3px #bbbbbb; } \
	</style> \
	</head>"


#define B_SIZE 500

void processGET(int sock, char *requestLine);

char *wget_command=NULL;
char *root_folder="/";
char *default_cwd="/";
char *access_secret=NULL;
char *port_number="2226";
char *clipboard_folder="/tmp/.web-manager-fileserver-clipboard";



#endif

