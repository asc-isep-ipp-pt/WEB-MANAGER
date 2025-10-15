![](favicon.ico)
# WEB-MANAGER
This project aims to create a monolithic web server allowing remote management operations over the Linux server where it is running.
- The service runs as root; this means it has full access to the server, including its filesystem.
- Access is controlled by an access secret stored in a file within the filesystem.
- The service is implemented by a single self-contained and autonomous binary file.

### Command line syntax:

./web-manager --help

Possible command line options are:

 --secret-file FILE-WITH-SECRET-STRING (default is /.web-manager.secret)
 
 --root-folder FOLDER (default is /)
 
 --initial-cwd FOLDER (default is /)
 
 --port TCP-PORT-NUMBER (default is 2229)

 --title TITLE (default is Web Manager)

 --killall
 