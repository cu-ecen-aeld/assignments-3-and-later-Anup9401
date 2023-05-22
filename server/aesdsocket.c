#include <syslog.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>

#define PORT 9000
#define MEM_SIZE 1024

int offset=0;
int sfd=-1, nsfd=-1, fd=-1;
char *packet=NULL;

void cleanup(int closeSocket){

	offset=0;
	if(packet!=NULL){
		free(packet);
		packet=NULL;
	}
	if(nsfd!=-1){
		shutdown(nsfd,SHUT_RDWR);
		close(nsfd);
		nsfd=-1;
	}

	if(closeSocket){
		if(sfd!=-1){
			shutdown(sfd,SHUT_RDWR);
			close(sfd);
		}
		if(fd!=-1){
			close(fd);
		}
		remove("/var/tmp/aesdsocketdata");
	}

}


void signalHandler(int signal){

	if(signal==2){
		syslog(LOG_DEBUG,"caught signal, exiting\n");
		cleanup(1);
		exit(0);
	}
	else if(signal==15){
		syslog(LOG_DEBUG,"caught signal, exiting\n");
		cleanup(1);
		exit(0);
	}

}

void socketServer(){

	struct sockaddr_in srv,cln;
	sfd=socket(AF_INET,SOCK_STREAM,0);
	if(sfd==-1){
		syslog(LOG_ERR,"socket() failed\n");
		closelog();
		exit(1);
	}

	syslog(LOG_DEBUG,"Socket created....\n");
	srv.sin_family=AF_INET;
	srv.sin_port=htons(PORT);
	srv.sin_addr.s_addr=inet_addr("0.0.0.0");

	int yes=1;
	if(setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		syslog(LOG_ERR,"setsockopt() failed\n");
		cleanup(1);
		exit(1);
	}

	if(bind(sfd,(struct sockaddr *)&srv, sizeof(srv))){
		syslog(LOG_ERR,"bind() failed\n");
		cleanup(1);
		exit(1);
	}

	if(listen(sfd,1)){
		syslog(LOG_ERR,"listen() failed\n");
		cleanup(1);
		exit(1);
	}

	fd=open("/var/tmp/aesdsocketdata",O_CREAT|O_TRUNC|O_RDWR,0666);
	if(fd==-1){
		syslog(LOG_ERR,"open() /var/tmp/aesdsocketdata failed\n");
		cleanup(1);
		exit(1);
	}

	while(1){

		unsigned int len=sizeof(cln);
		syslog(LOG_DEBUG,"Waiting for client....\n");
		nsfd=accept(sfd,(struct sockaddr*)&cln, &len);
		if(nsfd==-1){
			syslog(LOG_ERR,"accept() failed\n");
			cleanup(1);
			exit(1);
		}
		syslog(LOG_DEBUG,"Accepted connection from %s\n", inet_ntoa(cln.sin_addr));

		int packetSize=MEM_SIZE,currPackLen=0;


		while(1){

			packet=(char*)realloc( packet, packetSize);
			if(packet==NULL){
				syslog(LOG_ERR,"realloc() failed\n");
				cleanup(1);
				exit(1);
			}

			int recvLen=recv( nsfd, packet+offset, MEM_SIZE, 0);
			if(recvLen==0||recvLen==-1){
				syslog(LOG_DEBUG,"Closed connection from %s\n", inet_ntoa(cln.sin_addr));
				cleanup(0);
				break;
			}

			for( int i=offset;i<offset+recvLen;i++){
				if(packet[i]=='\n'){
					currPackLen=i+1;
					break;
				}
			}

			offset=offset+recvLen;
			if(currPackLen){
				int writeLen=write(fd, packet, currPackLen);
				if(writeLen==-1){
					syslog(LOG_DEBUG,"write() packet failed\n");
					cleanup(0);
					break;
				}
				if(lseek(fd,0,SEEK_SET)==-1){
					syslog(LOG_DEBUG,"lseek() failed\n");
					cleanup(0);
					break;
				}
				char buf[MEM_SIZE+1];
				int readLen=0,sendLen=0;
				while((readLen=read(fd,buf,MEM_SIZE))){
					sendLen=send(nsfd, buf, readLen, 0);
					if(sendLen==-1){
						syslog(LOG_DEBUG,"send() packets failed\n");
						break;
					}
				}
				if(sendLen==-1){
					cleanup(0);
					break;
				}

				offset=offset-currPackLen;
				if(offset!=0){
					strncpy(packet,packet+currPackLen+1,offset);
				}
				memset(packet+offset,0,packetSize-offset);
				currPackLen=0;
				packetSize=offset+MEM_SIZE;
			}
			else{
				packetSize=packetSize+MEM_SIZE;
			}

		}
	}
}

int main( int argc, char *argv[]){

	openlog("aesdsocket", 0, LOG_USER);

	struct sigaction s;
	s.sa_handler=signalHandler;
	sigemptyset(&s.sa_mask);
	s.sa_flags=0;
	sigaction(2,&s,0);
	sigaction(15,&s,0);

	char *option="-d";
	int enableDaemon=0;

	if(argc>2){
		syslog(LOG_ERR,"More arguments than expected\n");
		closelog();
		exit(1);
	}

	if(argc==2){

		if(strcmp(option,argv[1])==0){
			enableDaemon=1;
			syslog(LOG_DEBUG,"Running as daemon\n");
		}
		else{
			syslog(LOG_ERR,"Invalid argument use -d\n");
			closelog();
			exit(1);
		}
	}

	if(enableDaemon){

		pid_t pid=0;
		pid_t sid=0;

		pid=fork();
		if(pid<0){
			syslog(LOG_ERR,"fork() failed\n");
			exit(1);
		}
		if(pid>0){
			exit(0); //exit the parent
		}

		umask(0);
		sid=setsid();

		if(sid<0){
			syslog(LOG_ERR,"setsid() failed\n");
			exit(1);
		}

		int ret=chdir("/");
		if(ret<0){
			syslog(LOG_ERR,"chdir() failed\n");
			exit(1);
		}

		close(0);
		close(1);
		close(2);
		int fdNull=open("/dev/null", O_RDWR, 0666);
		if(fdNull<0){
			syslog(LOG_ERR,"open() /dev/null failed\n");
			exit(1);
		}
		dup(0);
		dup(0);
		syslog(LOG_DEBUG,"Daemon PID=%d\n",getpid());

	}

	socketServer();

}

