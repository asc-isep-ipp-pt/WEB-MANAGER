#ifndef ___WEB_FILE_MANAGER_H
#define ___WEB_FILE_MANAGER_H


#define BASE_FOLDER "www"


#define HTML_BODY_FOOTER "<footer><hr><small>Ultra Basic Web File Manager version 0.1, May 2023<br><i>Andr&eacute; Moreira (asc@isep.ipp.pt)</i></small></footer></body></html>"

#define HTML_HEADER "<html><head><title>File Manager</title><script>function act(a,o,o2){document.main.action.value=a;document.main.object.value=o;document.main.object2.value=o2;document.main.submit();}</script></head>"


char *access_secret;
void processHttpRequest(int sock); //declared ahead

#endif

