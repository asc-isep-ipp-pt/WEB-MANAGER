

void web_manager_kill_all(void) {
	DIR *d;
	struct dirent *e;
	int my_pid, pid;
	char fileName[200], myName[200], commName[200];
	FILE *f;


	system("mount -t proc proc /proc"); // just in case ...

	my_pid=getpid();
	sprintf(fileName,"/proc/%i/comm",my_pid);
	f=fopen(fileName,"r");
	fgets(myName,200,f);
	fclose(f);
	
	// printf("My PID: %i - %s",my_pid, myName);

	d=opendir("/proc");
	e=readdir(d);
	while(e) {
		if(strcmp(e->d_name,"..") && strcmp(e->d_name,".")) {
			pid=atoi(e->d_name);
			if(pid>0 && pid!=my_pid) {
				sprintf(fileName,"/proc/%i/comm",pid);
				f=fopen(fileName,"r");
				fgets(commName,200,f);
				fclose(f);
				// printf("PID: %i - %s",pid,commName);
				if(!strcmp(myName,commName)) {
					printf("Sending the TERM signal to process %i - %s",pid,commName);
					kill(pid,SIGTERM);
				}
			}
		}
	e=readdir(d);
	}
	closedir(d);
}
