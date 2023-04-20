
#include<string.h>
#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<syslog.h>

int main(int argc, char *argv[]){

	char *writefile,*writestr;
	openlog("writerApp", 0, LOG_USER);

	if(argc!=3){
		syslog(LOG_ERR, "Usage:./writer writefile writestr");
		printf("Usage:./writer writefile writestr\n");
		closelog();
		exit(1);
	}
	else{
		writefile=argv[1];
		writestr=argv[2];
		syslog(LOG_DEBUG, "Writing %s to %s\n", writefile, writestr);
	}

	int fd=open( writefile, O_CREAT|O_WRONLY|O_TRUNC, 0666);
	if(fd<0){
		perror("open file");
		syslog(LOG_ERR, "Could not open file");
		closelog();
		exit(1);
	}
	else{
		int w = write( fd, writestr, strlen(writestr));
		if(w<0){
			perror("write to file");
			syslog(LOG_ERR, "Could not write to file");
			close(fd);
			closelog();
			exit(1);
		}
	}

	close(fd);
	closelog();
	exit(0);
}


