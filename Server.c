/************************************************************/
/*   HTTP Calculator Server v1.0                            */
/*   ===========================                            */
/*   Copyright(c) 2017 by Felix Knobl, FH Technikum Wien    */
/************************************************************/

#include <sys/socket.h> // socket(), bind(), ...
#include <netinet/in.h> // struct sockaddr_in
#include <strings.h> // memset(), ...
#include <unistd.h>  // write(), close(), ...
#include <stdio.h> // printf(), sprintf()
#include <arpa/inet.h> // inet_ntoa(), ...
#include <string.h>  // strlen()
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <fcntl.h>
#include  <netdb.h>
#include <sys/wait.h>  // waitpid()
#include <signal.h> // signal()
#include <stdlib.h> // exit()
#include <time.h>

#define DEFAULT_PORTNUMBER 	6655

//#define BYTES 		1024

#define MAX_BUFFER_LENGTH 		1024
#define MAX_CLIENTS		  		1024
#define MAX_PENDING_CONNECTIONS	4096
#define MAX_REQUEST_LENGTH		32768
#define MAX_RESPONSE_LENGTH		32768
#define MAX_PATH_LENGTH			256

// BUILD: clang -Wall --pedantic -D_POSIX_C_SOURCE=200809L Server.c
// RUN: change PWD before start

int clients[MAX_CLIENTS];

void SIGCHLD_handler(int);
void install_SIGCHLD_handler(void);
void processClient(int n);

int main (int argc, char **argv)
{
 	int n, listenfd;
	char c;
	char strPort[6] = {0, };

	// Converting default port to char array
	snprintf(strPort, sizeof(strPort), "%d", DEFAULT_PORTNUMBER);

  	// Parsing the command line arguments
    while ((c = getopt(argc, argv, "p:")) != -1)
	{
		if (c == 'p')
		{
			// Validate port
			if (strlen(optarg) > 5)
			{
				printf("ERROR: Invalid port number %s!\n\n", optarg);
				exit(-1);
			}

			// Convert argument to Long
			long longPort = 0;
			longPort = strtol(optarg, NULL, 10);

			// Check port range
			if (longPort <= 1 || longPort > 65535)
			{
				printf("ERROR: Invalid port number %ld!\n\n", longPort);
				exit(-1);
			}

			// Convert and override user specified port to char array
			memset(strPort, 0, sizeof(strPort));
			snprintf(strPort, sizeof(strPort), "%d", (int)longPort);

			break;
		}
	}

	printf("Starting HTTP_Calc Server on port %s...\n", strPort);

	// Establish SIGCHLD signal handler that deals with zombies (by teacher)
	install_SIGCHLD_handler();

	// Mark all client slots as disconnected by setting them to -1
	for (n = 0; n < MAX_CLIENTS; n++)
	{
		clients[n] = -1;
	}

 	struct addrinfo addrFlags, *returnValue, *pCurrent;

    // Prepare getaddrinfo flags
    memset(&addrFlags, 0, sizeof(struct addrinfo));
	addrFlags.ai_family = AF_INET;
    addrFlags.ai_socktype = SOCK_STREAM;
    addrFlags.ai_flags = AI_PASSIVE;

	if (getaddrinfo(NULL, strPort, &addrFlags, &returnValue) != 0)
    {
        printf("ERROR: getaddrinfo() error!\n\n");
        exit(-2);
    }

    // Try to create a socket and bind to it
    for (pCurrent = returnValue; pCurrent != NULL; pCurrent = pCurrent->ai_next)
    {
        listenfd = socket(pCurrent->ai_family, pCurrent->ai_socktype, 0);

		if (listenfd == -1)
		{
			continue;
		}

		if (bind(listenfd, pCurrent->ai_addr, pCurrent->ai_addrlen) == 0)
		{
			break;
		}
    }

    if (pCurrent == NULL)
    {
        printf("ERROR: Could not create socket or binding!\n\n");
        exit(-3);
    }

	// Cleanup / free memory
    freeaddrinfo(returnValue);

    // listen for incoming connections
    if (listen(listenfd, MAX_PENDING_CONNECTIONS) == -1)
    {
        printf("ERROR: Could not start listening!\n\n");
        exit(1);
    }

	struct sockaddr_in clientAddr;
	socklen_t len;
	int slot = 0;

	// Endless loop
  	while (1)
	{
    	len = sizeof(clientAddr);

		// Accept new incoming connection
		clients[slot] = accept(listenfd, (struct sockaddr *)&clientAddr, &len);

		if (clients[slot] < 0)
		{
            printf("ERROR: Could not accept connection!\n\n");
			exit(-1);
		}
        else
        {
            if (fork() == 0)
            {
                processClient(slot);
                exit(0);
            }
        }

        while (clients[slot] != -1)
		{
			slot = (slot + 1) % MAX_CLIENTS;
		}
  	}
}

char responseHeaderBuffer[MAX_RESPONSE_LENGTH];

void build200Response()
{


}


// Process client connection
void processClient(int clientIndex)
{
    char *rootDirectory;
	char clientRequestBuffer[MAX_REQUEST_LENGTH], clientResponseBuffer[MAX_RESPONSE_LENGTH], fileName[MAX_PATH_LENGTH];
    int rcvd, fd, n;

	int bytesSent = 0, bytesRead = 0;

	char *requestMethod, *requestURL, *protocolVersion;

	const char daysOfWeek[7][3] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	const char monthsOfYear[12][3] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	// Get root directory
	rootDirectory = getenv("PWD");

	if (rootDirectory == NULL)
	{
		printf("ERROR: Could not get root directory!\n\n");
	}

	// Clear client request buffer
    memset((void *)clientRequestBuffer, 0, MAX_REQUEST_LENGTH);

	// Receive client request
    rcvd = recv(clients[clientIndex], clientRequestBuffer, MAX_REQUEST_LENGTH, 0);

    if (rcvd < 0)
	{
        printf("ERROR: Receive error client ID: %d!\n", clientIndex);
	}
    else if (rcvd == 0)
	{
        printf("ERROR: Client ID: %d disconnected upexpectedly. Receive Socket closed!\n", clientIndex);
	}
    else
    {
        // Data received
		printf("------HTTP REQUEST------\n%s\n\n", clientRequestBuffer);

		requestMethod = strtok(clientRequestBuffer, " \t\r\n");		// Parse Request Method

		if (requestMethod == NULL)
		{
			printf("ERROR: HTTP REQUEST not found!");
		}
		else if (strncmp(requestMethod, "GET\0", 4) == 0)
        {
			// Check if GET request has been received
            requestURL = strtok (NULL, " \t");	 	// Parse URL
            protocolVersion = strtok (NULL, " \t\r\n"); 	// Parse HTTP version

			printf("------REQUEST DATA:------\nrequestMethod = '%s'\nrequestURL = '%s'\nprotocolVersion = '%s'\n\n", requestMethod, requestURL, protocolVersion);

			// Check HTTP protocol version
			if (strncmp(protocolVersion, "HTTP/1.0", 8) != 0 && strncmp(protocolVersion, "HTTP/1.1", 8) != 0)
            {
				// Wrong HTTP version or bad request
				write(clients[clientIndex], "HTTP/1.0 400 Bad Request\r\n", 26);
            }
            else
            {
				// Check and remove trailing "/"
				for (n = strlen(requestURL) - 1; n > 0; n--)
				{
					if (requestURL[n] == '/')
					{
						requestURL[n] = '\0';
					}
					else
					{
						break;
					}
				}

				// Check URL
				if ((strncmp(requestURL, "/\0", 2) == 0) ||	(strncmp(requestURL, "/index.htm\0", 11) == 0))
				{
					requestURL = "/index.html";
				}


				// Handle Random number service
				if (strncmp(requestURL, "/serv/random/", 13) == 0)
				{
					printf("OK!!!!!! --> '%s'", requestURL + 13);
					long maxRandomNumber = 0;

					maxRandomNumber = strtol("", NULL, 10);

					// Check conversion
					if (maxRandomNumber == 0)
					{

					}

				}




				// Get the absolute location of the file name
				strncpy(fileName, rootDirectory, strlen(rootDirectory));
                strncpy(&fileName[strlen(rootDirectory)], requestURL, strlen(requestURL));

				printf("------REQUEST DATA PROCESSED:------\nrequestMethod = '%s'\nrequestURL = '%s'\nprotocolVersion = '%s'\nfileName = '%s'\n\n", requestMethod, requestURL, protocolVersion, fileName);

				// Get current time
				struct tm *GMT;
				time_t rawtime;

				time(&rawtime);

				// Get GMT time from current time
				memset((void *)&GMT, 0, sizeof(struct tm));
				GMT = gmtime(&rawtime);

				//printf("HH:MM:SS = %02d:%02d:%02d\n", GMT->tm_hour, GMT->tm_min, GMT->tm_sec);

				// Does the file exist?
                if ((fd = open(fileName, O_RDONLY)) != -1)
                {
					off_t fsize;

					// Get file length
					fsize = lseek(fd, 0, SEEK_END);

					// Check file length
					if (fsize == -1)
					{
						fsize = 0;
					}

					// Clear response buffer
					memset((void *)&clientResponseBuffer, 0, MAX_RESPONSE_LENGTH);

					// Create HTTP client response
					snprintf(clientResponseBuffer, MAX_RESPONSE_LENGTH, "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-cache\r\nDate: %.3s, %02d %.3s %d %02d:%02d:%02d GMT\r\nServer: KnoblHyperActiveServer(1.0)\r\nContent-Length: %d\r\n\r\n",
							daysOfWeek[GMT->tm_wday], GMT->tm_mday, monthsOfYear[GMT->tm_mon], GMT->tm_year + 1900, GMT->tm_hour, GMT->tm_min, GMT->tm_sec, (int)fsize);

					// Seek to the beginning of the file
					lseek(fd, 0, SEEK_SET);

					printf("------HTTP RESPONSE:------\n%s\n\n", clientResponseBuffer);

					// Send response header to client
					bytesSent = write(clients[clientIndex], clientResponseBuffer, strlen(clientResponseBuffer));

					// Check send status
					if (bytesSent == strlen(clientResponseBuffer))
					{
						printf("INFO: Response header sent OK!\n\n");
					}
					else
					{
						printf("ERROR: Failed sending response header to client!\n\n");
					}

					// Clear response buffer
					memset((void *)&clientResponseBuffer, 0, MAX_RESPONSE_LENGTH);

					// Read the file until end
                    while ((bytesSent = read(fd, clientResponseBuffer, MAX_RESPONSE_LENGTH)) > 0)
					{
                    	// Send payload response buffer to client
						write(clients[clientIndex], clientResponseBuffer, bytesSent);
					}
                }
                else
				{
					// Send File Not Found to client
					write(clients[clientIndex], "HTTP/1.0 404 Not Found\r\n", 24);
				}
            }
        }
		else
		{
			write(clients[clientIndex], "HTTP/1.1 405 Method Not Allowed\r\nAllow: GET, HEAD\r\n\r\n", 53);
		}

    }

	printf("Closing Socket\n");

    // Closing SOCKET
    shutdown(clients[clientIndex], SHUT_RDWR);
    close(clients[clientIndex]);
    clients[clientIndex] = -1;
}

// Below is the signal handler to avoid zombie processes as
// well as the appropriate installer.
// SIGCHLD handler, derived from W. Richard Stevens,
// Network Programming, Vol.1, 2nd Edition, p128
void SIGCHLD_handler(int signo)
{
	pid_t pid;
	int stat;

	while ((pid = waitpid(-1, &stat, WNOHANG)) > 0);

	// optional actions, usually nothing ;
	return;
}

// installer for the SIGCHLD handler
void install_SIGCHLD_handler(void)
{
	struct sigaction act;

	// block all signals during exec of SIGCHLD_handler
	sigfillset(&act.sa_mask);
	act.sa_handler = &SIGCHLD_handler;

	// auto restart interrupted system calls
 	act.sa_flags = SA_RESTART;
	sigaction (SIGCHLD, &act, NULL);
}
