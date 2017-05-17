/************************************************************/
/*   HTTP Calculator Server v1.0                            */
/*   ===========================                            */
/*   Copyright(c) 2017 by Felix Knobl, FH Technikum Wien    */
/************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <time.h>
#include <math.h>


#define DEFAULT_PORTNUMBER 	6655

#define MAX_BUFFER_LENGTH 			1024
#define MAX_CLIENTS		  			1024
#define MAX_PENDING_CONNECTIONS		4096
#define MAX_REQUEST_LENGTH			32768
#define MAX_RESPONSE_LENGTH			4096
#define MAX_PATH_LENGTH				256
#define MAX_CONTENT_LENGTH_BUFFER 	256
#define MAX_PAYLOAD_LENGTH			4096
#define MAX_URI_LENGTH				64
#define MAX_CONTENT_TYPE_LENGTH		32

// BUILD: clang -Wall -lm --pedantic -D_POSIX_C_SOURCE=200809L Server.c
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
    while ((c = getopt(argc, argv, "p:h")) != -1)
	{
		if (c == 'h')
		{
			printf("HTTP Calculator Server v1.0\n");
			printf("===========================\n");
			printf("Usage: $ ./httpcalc                 ... Starts the server at default port %d\n", DEFAULT_PORTNUMBER);
			printf("       $ ./httpcalc -p <portnumber> ... Starts the server at port <portnumber>\n");
			printf("       $ ./httpcalc -h              ... Prints this help and exits the program\n\n");
			exit(0);
		}

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

	// Init random number generator
	srand(time(NULL));

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
char responsePayloadBuffer[MAX_PAYLOAD_LENGTH];

void buildResponseHeader(int statusCode, char *contentType)
{
	// Day/Month constants, since asctime returns incorrect format
	const char daysOfWeek[7][4] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
	const char monthsOfYear[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	const char statusCode200[] = "200 OK";
	const char statusCode400[] = "400 Bad Request";
	const char statusCode404[] = "404 Not Found";
	const char statusCode405[] = "405 Method Not Allowed\r\nAllow: GET, HEAD";
	const char statusCode414[] = "414 Request-URI Too Long";
	const char statusCode500[] = "500 Internal Server Error";

	char statusCodeBuffer[128] = {0, };

	switch (statusCode)
	{
		case 200:
			strncpy(statusCodeBuffer, statusCode200, strlen(statusCode200));
			break;

		case 400:
			strncpy(statusCodeBuffer, statusCode400, strlen(statusCode400));
			break;

		case 404:
			strncpy(statusCodeBuffer, statusCode404, strlen(statusCode404));
			break;

		case 405:
			strncpy(statusCodeBuffer, statusCode405, strlen(statusCode405));
			break;

		case 414:
			strncpy(statusCodeBuffer, statusCode414, strlen(statusCode414));
			break;

		case 500:
			strncpy(statusCodeBuffer, statusCode500, strlen(statusCode500));
			break;

		default:
			return;
			break;
	}

	// Get current time
	struct tm *GMT;
	time_t now;

	time(&now);

	// Get GMT time from current time
	GMT = gmtime(&now);

	// Clear response buffers
	memset((void *)responseHeaderBuffer,  0, MAX_RESPONSE_LENGTH);
	memset((void *)responsePayloadBuffer, 0, MAX_PAYLOAD_LENGTH);

	// Create HTTP client response
	snprintf(responseHeaderBuffer, MAX_RESPONSE_LENGTH, "HTTP/1.1 %s\r\nContent-Type: %s; charset=utf-8\r\nCache-Control: no-cache\r\nDate: %.3s, %02d %.3s %d %02d:%02d:%02d GMT\r\nServer: KnoblHyperActiveServer(1.0)\r\nConnection: close\r\n",
			 statusCodeBuffer, contentType, daysOfWeek[GMT->tm_wday], GMT->tm_mday, monthsOfYear[GMT->tm_mon], GMT->tm_year + 1900, GMT->tm_hour, GMT->tm_min, GMT->tm_sec);
}

void appendContentLength(int contentLength)
{
	char contentLengthBuffer[MAX_CONTENT_LENGTH_BUFFER] = {0, };

	// Build content length buffer
	snprintf(contentLengthBuffer, MAX_CONTENT_LENGTH_BUFFER, "Content-Length: %d\r\n\r\n", contentLength);

	// Append content length to the header
    strncpy(&responseHeaderBuffer[strlen(responseHeaderBuffer)], contentLengthBuffer, strlen(contentLengthBuffer));
}

void printResponseHeaderBuffer()
{
	printf("------HTTP RESPONSE------\n");

	if (write(STDOUT_FILENO, responseHeaderBuffer, strlen(responseHeaderBuffer)) == -1)
	{
		perror ("ERROR: Error writing to screen!\n\n");
		exit(EXIT_FAILURE);
	}
}

void closeConnection(int clientIndex)
{
    // Close SOCKET
    if (shutdown(clients[clientIndex], SHUT_RDWR) == -1)
	{
		printf("ERROR: Could not shutdown client socket!\n");
	}

    if (close(clients[clientIndex]) == -1)
	{
		printf("ERROR: Could not close client socket!\n");
	}

    clients[clientIndex] = -1;
}

// This function appends the content length property to the header.
// It sends the header and also the payload if required and available to the client.
// Finally, the connection gets closed and the client freed.
void sendDataToClient(int clientIndex, bool sendPayload, char *file)
{
	if (file != NULL)
	{
		// Send a file
		int fd = 0;
		char fileName[MAX_PATH_LENGTH] = {0, };
		char *rootDirectory;

		// Get the root directory
		rootDirectory = getenv("PWD");

		// Check root directory
		if (rootDirectory == NULL)
		{
			printf("ERROR: Could not get root directory!\n");
			closeConnection(clientIndex);
			return;
		}

		// Get the absolute location to the file name
		strncpy(fileName, rootDirectory, strlen(rootDirectory));
		strncpy(&fileName[strlen(rootDirectory)], file, strlen(file));

		printf("INFO: Client requested file: %s\n", fileName);

		// Open the file if it exists
		if ((fd = open(fileName, O_RDONLY)) != -1)
		{
			off_t fsize;
			int bytesRead = 0;

			// Get file length
			fsize = lseek(fd, 0, SEEK_END);

			// Check file length
			if (fsize == -1)
			{
				printf("ERROR: File size error!\n");
				closeConnection(clientIndex);
				return;
			}
			else
			{
				// Seek to the beginning of the file
				lseek(fd, 0, SEEK_SET);
			}

			// Check file extension and build content type
		    char *fileExtension = NULL;
			char contentTypeBuffer[MAX_CONTENT_TYPE_LENGTH] = "text/html";

			fileExtension = strrchr(fileName, '.');

			// File extension not found?
			if (fileExtension == NULL)
			{
				printf("ERROR: File extension not found!\n");
			}
			else
			{
				// Compare file extension and set the content type
				if (strncmp(fileExtension, ".ico", 4) == 0)
				{
					memset(contentTypeBuffer, 0, MAX_CONTENT_TYPE_LENGTH);
					memcpy(contentTypeBuffer, "image/x-icon", sizeof("image/x-icon"));
				}
			}

			// Build response
			buildResponseHeader(200, contentTypeBuffer);

			// Add Content Length parameter
			if (sendPayload)
			{
				appendContentLength(fsize);
			}
			else
			{
				appendContentLength(0);
			}

			printResponseHeaderBuffer();

			// Send response header buffer to client
			if (write(clients[clientIndex], responseHeaderBuffer, strlen(responseHeaderBuffer)) == strlen(responseHeaderBuffer))
			{
				printf("INFO: Response header sent OK!\n");
			}
			else
			{
				printf("ERROR: Failed sending response header to client!\n");
				closeConnection(clientIndex);
				return;
			}

			if (sendPayload)
			{
				// Clear payload buffer
				memset((void *)&responsePayloadBuffer, 0, MAX_PAYLOAD_LENGTH);

				// Read the file until end
				while ((bytesRead = read(fd, responsePayloadBuffer, MAX_PAYLOAD_LENGTH)) > 0)
				{
					// Send payload response buffer to client
					if (write(clients[clientIndex], responsePayloadBuffer, bytesRead) != bytesRead)
					{
						printf("ERROR: Error sending file fragment to client!\n");

						// Close the file
						if (close(fd) == -1)
						{
							printf("ERROR: Could not close the file!\n");
						}

						// Close connection
						closeConnection(clientIndex);

						return;
					}
				}
			}

			// Close the file
			if (close(fd) == -1)
			{
				printf("ERROR: Could not close the file!\n");
			}
        }
		else
		{
			printf("ERROR: File not found!\n");

			// Overwrite current built response and build a new one for 404
			buildResponseHeader(404, "text/html");

			// Recursive call to send 404
			sendDataToClient(clientIndex, false, NULL);

			return;
		}
	}
	else
	{
		// Send the custom template
		// Append Content Length property
		appendContentLength(strlen(responsePayloadBuffer));

		printResponseHeaderBuffer();

		// Send response header buffer to client
		if (write(clients[clientIndex], responseHeaderBuffer, strlen(responseHeaderBuffer)) > 0)
		{
			if (sendPayload && strlen(responsePayloadBuffer) > 0)
			{
				// Send payload response buffer to client
				if (write(clients[clientIndex], responsePayloadBuffer, strlen(responsePayloadBuffer)) == strlen(responsePayloadBuffer))
				{
					printf("INFO: Data sent to client OK!\n");
				}
				else
				{
					printf("ERROR: Error sending Payload to client!\n");
				}
			}
		}
		else
		{
			printf("ERROR: Error sending Header to client!\n");
		}
	}

	// Close connection
	closeConnection(clientIndex);
}

bool convertToDouble(char *input, double *result)
{
	char *strEnd = NULL;

	*result = strtod(input, &strEnd);

	// Check conversion
	if (input == strEnd || *strEnd != '\0')
	{
		return false;
	}
	else
	{
		return true;
	}
}

// Process client connection
void processClient(int clientIndex)
{
    char *requestMethod, *requestURL, *protocolVersion;
	char clientRequestBuffer[MAX_REQUEST_LENGTH] = {0, };
	int n = 0, bytesRead = 0;
	bool sendPayload = false;

	// Receive client request
    bytesRead = recv(clients[clientIndex], clientRequestBuffer, MAX_REQUEST_LENGTH, 0);

    if (bytesRead < 0)
	{
        printf("ERROR: Receive error client ID: %d!\n", clientIndex);
		closeConnection(clientIndex);
		return;
	}

    if (bytesRead == 0)
	{
        printf("ERROR: Client ID: %d disconnected upexpectedly. Receive Socket closed!\n", clientIndex);
		closeConnection(clientIndex);
		return;
	}

    // Data received
	printf("------HTTP REQUEST------\n%s\n\n", clientRequestBuffer);

	// Parse request method
	requestMethod = strtok(clientRequestBuffer, " \t\r\n");

	// Check request method
	if (requestMethod == NULL)
	{
		printf("ERROR: HTTP REQUEST not found!");
		buildResponseHeader(405, "text/html");
		sendDataToClient(clientIndex, false, NULL);
		return;
	}
	else if (strncmp(requestMethod, "GET\0", 4) == 0)
	{
		sendPayload = true;
	}
	else if (strncmp(requestMethod, "HEAD\0", 5) == 0)
    {
		sendPayload = false;
	}
	else
	{
		buildResponseHeader(405, "text/html");
		sendDataToClient(clientIndex, false, NULL);
		return;
	}

	// Parse requestURL
    requestURL = strtok(NULL, " \t");

	// Check requestURL
	if (requestURL == NULL)
	{
		buildResponseHeader(400, "text/html");
		sendDataToClient(clientIndex, false, NULL);
		return;
	}

	// Parse protocolVersion
    protocolVersion = strtok(NULL, " \t\r\n");

	// Check protocolVersion
	if (requestURL == NULL)
	{
		buildResponseHeader(400, "text/html");
		sendDataToClient(clientIndex, false, NULL);
		return;
	}

	// Check URL length
	if (strlen(requestURL) > MAX_URI_LENGTH)
	{
		buildResponseHeader(414, "text/html");
		sendDataToClient(clientIndex, sendPayload, NULL);
		return;
	}

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

		printf("------REQUEST DATA (TRAILED)------\nrequestMethod = '%s'\nrequestURL = '%s'\nprotocolVersion = '%s'\n\n", requestMethod, requestURL, protocolVersion);

		// HANDLER
		if (strncmp(requestURL, "/serv/random", 12) == 0)
		{
			// HANDLING: A random floating-point number in the range between 0 and <Number>
			const char randomServiceTemplate[] = "<html><head><title>Random Number Service</title></head><body>Your random number between 0 and %f is %f.</body></html>";
			double number = 0;
			double result = 0;

			requestURL += 12;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else if (convertToDouble(++requestURL, &number) && number >= 0)
			{
				// Generate a random number
				srand(time(NULL));
				result = ((double)rand() / (double)(RAND_MAX)) * number;

				// Build HTTP response
				buildResponseHeader(200, "text/html");

				// Create webpage from template
				snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, randomServiceTemplate, number, result);
			}
			else
			{
				// Build HTTP response
				buildResponseHeader(500, "text/html");
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else if (strncmp(requestURL, "/calc/sqrt", 10) == 0)
		{
			// HANDLING: The square root of the floating-point number
			const char squareRootTemplate[] = "<html><head><title>Square Root Calculator</title></head><body>The square root of the number %f is %f.</body></html>";
			double number = 0;
			double result = 0;

			requestURL += 10;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else if (convertToDouble(++requestURL, &number) && number >= 0)
			{
				// Calculate result
				result = sqrt(number);

				// Build HTTP response
				buildResponseHeader(200, "text/html");

				// Create webpage from template
				snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, squareRootTemplate, number, result);
			}
			else
			{
				// Build HTTP response
				buildResponseHeader(500, "text/html");
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else if (strncmp(requestURL, "/calc/func/sin", 14) == 0)
		{
			// HANDLING: The value of the sine function for the given floating-point radian angle <Number>
			const char sinTemplate[] = "<html><head><title>Sine Calculator</title></head><body>The result of the sine function for the radian angle number %f is %f.</body></html>";
			double number = 0;
			double result = 0;

			// Move pointer to the start of the number
			requestURL += 14;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else if (convertToDouble(++requestURL, &number))
			{
				// Calculate result
				result = sin(number);

				// Build HTTP response
				buildResponseHeader(200, "text/html");

				// Create webpage from template
				snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, sinTemplate, number, result);
			}
			else
			{
				// Build HTTP response
				buildResponseHeader(500, "text/html");
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else if (strncmp(requestURL, "/calc/func/cos", 14) == 0)
		{
			// HANDLING: The value of the cosinus function for the given floating-point radian angle <Number>
			const char cosTemplate[] = "<html><head><title>Cosine Calculator</title></head><body>The result of the cosine function for the radian angle number %f is %f.</body></html>";
			double number = 0;
			double result = 0;

			// Move pointer to the start of the number
			requestURL += 14;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else if (convertToDouble(++requestURL, &number))
			{
				// Calculate result
				result = cos(number);

				// Build HTTP response
				buildResponseHeader(200, "text/html");

				// Create webpage from template
				snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, cosTemplate, number, result);
			}
			else
			{
				// Build HTTP response
				buildResponseHeader(500, "text/html");
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else if (strncmp(requestURL, "/calc/func/tan", 14) == 0)
		{
			// HANDLING: The value of the tangens function for the given floating-point radian angle <Number>
			const char tanTemplate[] = "<html><head><title>Tangens Calculator</title></head><body>The result of the tangens function for the radian angle number %f is %f.</body></html>";
			double number = 0;
			double result = 0;

			// Move pointer to the start of the number
			requestURL += 14;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else if (convertToDouble(++requestURL, &number))
			{
				// Calculate result
				result = tan(number);

				// Build HTTP response
				buildResponseHeader(200, "text/html");

				// Create webpage from template
				snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, tanTemplate, number, result);
			}
			else
			{
				// Build HTTP response
				buildResponseHeader(500, "text/html");
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else if ((strncmp(requestURL, "/calc/add", 9) == 0) ||
				 (strncmp(requestURL, "/calc/sub", 9) == 0) ||
				 (strncmp(requestURL, "/calc/mul", 9) == 0) ||
				 (strncmp(requestURL, "/calc/div", 9) == 0) ||
				 (strncmp(requestURL, "/calc/mod", 9) == 0))
		{
			// HANDLING: The add, sub, mul, div, mod of the two floating-point numbers <Number 1> and <Number 2>
			// Get the operation
			requestURL += 6;

			char operationBuffer[4] = {0, };
			memcpy(operationBuffer, requestURL, 3);

			const char calcTemplate[] = "<html><head><title>Calculator</title></head><body>The result of your requested operation (%s) is %f.</body></html>";
			double number1 = 0;
			double number2 = 0;
			double result = 0;
			char *token;

			// Move pointer to the start of the number
			requestURL += 3;

			if (*requestURL == '\0')
			{
				// Build HTTP response
				buildResponseHeader(400, "text/html");
			}
			else
			{
				token = strtok(++requestURL, "/");

				if (token == NULL)
				{
					// Build HTTP response
					buildResponseHeader(400, "text/html");
				}
				else if (convertToDouble(token, &number1))
				{
					token = strtok(NULL, "/");

					if (token == NULL)
					{
						// Build HTTP response
						buildResponseHeader(400, "text/html");
					}
					else if (convertToDouble(token, &number2))
					{
						// Calculate result
						if (strncmp(operationBuffer, "add", 3) == 0)
						{
							result = number1 + number2;
						}
						else if (strncmp(operationBuffer, "sub", 3) == 0)
						{
							result = number1 - number2;
						}
						else if (strncmp(operationBuffer, "mul", 3) == 0)
						{
							result = number1 * number2;
						}
						else if (strncmp(operationBuffer, "div", 3) == 0)
						{
							if (number2 == 0)
							{
								// Build HTTP response
								buildResponseHeader(500, "text/html");

								// Send data
								sendDataToClient(clientIndex, sendPayload, NULL);

								return;
							}
							else
							{
								result = number1 / number2;
							}
						}
						else if (strncmp(operationBuffer, "mod", 3) == 0)
						{
							if (number2 == 0)
							{
								// Build HTTP response
								buildResponseHeader(500, "text/html");

								// Send data
								sendDataToClient(clientIndex, sendPayload, NULL);

								return;
							}
							else
							{
								result = (int)number1 % (int)number2;
							}
						}

						// Build HTTP response
						buildResponseHeader(200, "text/html");

						// Create webpage from template
						snprintf(responsePayloadBuffer, MAX_PAYLOAD_LENGTH, calcTemplate, operationBuffer, result);
					}
					else
					{
						// Build HTTP response
						buildResponseHeader(500, "text/html");
					}
				}
				else
				{
					// Build HTTP response
					buildResponseHeader(500, "text/html");
				}
			}

			// Send data
			sendDataToClient(clientIndex, sendPayload, NULL);

			return;
		}
		else
		{
			// HANDLING: Send a File
			sendDataToClient(clientIndex, sendPayload, requestURL);
		}
    }
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
