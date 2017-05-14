/*
 * I N C L U D E S
 */
#include <sys/socket.h> // socket(), bind(), ...
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h> // memset(), ...
#include <unistd.h>  // write(), close(), ...
#include <stdio.h> // printf(), sprintf()
#include <arpa/inet.h> // inet_ntoa(), ...
#include <string.h>  // strlen()
#include <sys/wait.h>  // waitpid()
#include <signal.h> // signal()
#include <stdlib.h> // exit()

#define PORTNUMBER 6655


// BUILD: clang -Wall --pedantic -D_POSIX_C_SOURCE=200809L Server.c




void SIGCHLD_handler(int);
void install_SIGCHLD_handler(void);

int main (int argc, char **argv)
{
 	int listenfd, connfd;

	struct sockaddr_in servaddr, cliaddr;
	pid_t ForkPID;
	socklen_t len ;
	int optval = 1;
	int ret = 0;
	char buf[100];

 	// establish SIGCHLD signal handler that deals with zombies
	install_SIGCHLD_handler();

	// socket() open a file descriptor
	listenfd = socket(PF_INET, SOCK_STREAM, 0);

	if (listenfd == -1)
	{
		perror("socket error");
		exit(-1);
	}

	// setsockopt() free previously used sockets()
  	ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

  	if (ret == -1)
	{
		perror("setsockopt error");
	}

	// fill in the structure required to handle bind()
	memset((void *)&servaddr, 0, sizeof(servaddr));

	servaddr.sin_family = PF_INET ;  // we use IPv4
  	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  	servaddr.sin_port = htons(PORTNUMBER) ;

  	// bind()
  	ret = bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

	if (ret == -1)
	{
		perror("bind error");
	}


	// listen()
	ret = listen(listenfd, 5);

	if (ret == -1)
	{
    	perror("listen error");
	}

	// endless loop
  	while(1)
	{
    	len = sizeof(cliaddr);

		// accept()
    	connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &len);

		if (connfd == -1)
		{
			printf("SIN FAMILY: %d\nSIN_PORT: %d\n", cliaddr.sin_family, htonl(cliaddr.sin_addr.s_addr));


			printf("ACCEPT ERROR!\n");
			perror("accept error");
		}

    	// let the child handle the connection
    	ForkPID = fork();

		if (ForkPID == 0)
		{
    		// child process
        	close(listenfd); // close listen port


			sprintf(buf,"What's the answer to god, the universe and everything?\n");
  			write(connfd, buf, strlen(buf));

        	//dup2(connfd, 0);          // redirect STDIN
        	//dup2(connfd, 1);          // redirect STDOUT

			// execute a STDIN/STDOUT app.
        	//ret = execlp("./demo","demo",(char *)NULL);

			if (ret == -1)
			{
				perror("ERROR: Writing to client!\n");
			}

    	}
		else
		{
    		// parent process
        	close(connfd);
    	}
  	}
}

// below is the signal handler to avoid zombie processes as
// well as the appropriate installer
//
// SIGCHLD handler, derived from W. Richard Stevens,
// Network Programming, Vol.1, 2nd Edition, p128
void SIGCHLD_handler(int signo)
{
 pid_t  pid ;
 int  stat ;

 while ( (pid=waitpid(-1,&stat,WNOHANG)) > 0 )
     ;
  // optional actions, usually nothing ;
 return ;
}

// installer for the SIGCHLD handler
void install_SIGCHLD_handler(void)
{
 struct sigaction act ;
 // block all signals during exec of SIGCHLD_handler
 sigfillset(&act.sa_mask) ;
 act.sa_handler = &SIGCHLD_handler ;
 // auto restart interrupted system calls
 act.sa_flags = SA_RESTART ;
 sigaction (SIGCHLD,&act,NULL) ;
}
