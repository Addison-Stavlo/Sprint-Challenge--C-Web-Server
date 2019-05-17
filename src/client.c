#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "lib.h"

#define BUFSIZE 4096 // max number of bytes we can get at once

/**
 * Struct to hold all three pieces of a URL
 */
typedef struct urlinfo_t
{
  char *hostname;
  char *port;
  char *path;
} urlinfo_t;

/**
 * Tokenize the given URL into hostname, path, and port.
 *
 * url: The input URL to parse.
 *
 * Store hostname, path, and port in a urlinfo_t struct and return the struct.
*/
urlinfo_t *parse_url(char *url)
{
  // copy the input URL so as not to mutate the original
  char *hostname = strdup(url);
  char *port;
  char *path;

  urlinfo_t *urlinfo = malloc(sizeof(urlinfo_t));

  /*
    We can parse the input URL by doing the following:

    1. Use strchr to find the first slash in the URL (this is assuming there is no http:// or https:// in the URL).
    2. Set the path pointer to 1 character after the spot returned by strchr.
    3. Overwrite the slash with a '\0' so that we are no longer considering anything after the slash.
    4. Use strchr to find the first colon in the URL.
    5. Set the port pointer to 1 character after the spot returned by strchr.
    6. Overwrite the colon with a '\0' so that we are just left with the hostname.
  */
  char *http = strstr(url, "http://");
  char *https = strstr(url, "https://");
  if (http != NULL)
  {
    url += 7;
  }
  else if (https != NULL)
  {
    url += 8;
  }
  // FIND PATH (the stuff after the /)
  //find the first slash in the URL
  const char slash = '/';
  path = strchr(url, slash);

  if (path != NULL)
  {
    // Overwrite the slash with a '\0' so that we are no longer considering anything after the slash.
    path[0] = '\0';
    // Set the path pointer to 1 character after
    path += 1;
  }
  else
  {
    path = strdup("");
  }

  // FIND PORT (the stuff between : and /)
  //Use strchr to find the first colon in the URL.
  char colon = ':';
  port = strchr(url, colon);

  if (port != NULL)
  {
    // Overwrite the colon with a '\0' so that we are just left with the hostname.
    port[0] = '\0';
    // Set the port pointer to 1 character after the spot returned by strchr.
    port += 1;
  }
  else
  {
    port = strdup("80");
  }

  // whats left in URL is the hostname
  urlinfo->hostname = strdup(url);
  urlinfo->path = strdup(path);
  urlinfo->port = strdup(port);

  return urlinfo;
}

/**
 * Constructs and sends an HTTP request
 *
 * fd:       The file descriptor of the connection.
 * hostname: The hostname string.
 * port:     The port string.
 * path:     The path string.
 *
 * Return the value from the send() function.
*/
int send_request(int fd, char *hostname, char *port, char *path)
{
  const int max_request_size = 16384;
  char request[max_request_size];
  int rv;

  sprintf(request, "GET /%s HTTP/1.1\n"
                   "Host: %s:%s\n"
                   "Connection: close\n\n",
          path, hostname, port);

  rv = send(fd, request, max_request_size, 0);

  if (rv < 0)
  {
    perror("send");
  }

  return rv;
  // return 0;
}

// Stretch for -h functionality
void print_after_header(char *buf)
{
  // TODO every iteration of the buffer is subject to this header-cut off
  // we wither need a buffer large enough to hold the entire response...
  // or only check this on the first iteration in the loop
  // works great for short responses... can see issue with GET to google.com
  char *end_header = strstr(buf, "\r\n\r\n");
  if (end_header != NULL)
  {
    end_header += 4;
    fprintf(stdout, "%s\n", end_header);
  }
  else
  {
    end_header = strstr(buf, "\n\n");
    if (end_header != NULL)
    {
      end_header += 2;
      fprintf(stdout, "%s\n", end_header);
    }
  }
}

void recv_response(int sockfd, char *buf, int argc, char *argv[], struct urlinfo_t *urlinfo)
{
  int numbytes;
  while ((numbytes = recv(sockfd, buf, BUFSIZE - 1, 0)) > 0)
  {
    // STRETCH - check redirect
    char *redirect = strstr(buf, "301 Moved Permanently");
    if (redirect != NULL)
    {
      char *location = strstr(buf, "Location: ");
      if (location != NULL)
      {
        location += 10;
        char newline = '\n';
        char *end = strchr(location, newline);
        end[0] = '\0';
        urlinfo = parse_url(location);

        close(sockfd);
        sockfd = get_socket(urlinfo->hostname, urlinfo->port);
        send_request(sockfd, urlinfo->hostname, urlinfo->port, urlinfo->path);

        recv_response(sockfd, buf, argc, argv, urlinfo);
      }
    }
    // check to see if we should display header
    if (argc > 2 && strcmp(argv[2], "-h") == 0)
    {
      fprintf(stdout, "%s\n", buf);
    }
    else
    {
      print_after_header(buf);
    }
  }
}

int main(int argc, char *argv[])
{
  int sockfd, numbytes;
  char buf[BUFSIZE];

  if (argc != 2 && argc != 3)
  {
    fprintf(stderr, "usage: client HOSTNAME:PORT/PATH (optional -h)\n");
    exit(1);
  }

  /*
    1. Parse the input URL
    2. Initialize a socket by calling the `get_socket` function from lib.c
    3. Call `send_request` to construct the request and send it
    4. Call `recv` in a loop until there is no more data to receive from the server. Print the received response to stdout.
    5. Clean up any allocated memory and open file descriptors.
  */

  // parse URL input
  struct urlinfo_t *urlinfo = parse_url(argv[1]);

  // initiate socket
  sockfd = get_socket(urlinfo->hostname, urlinfo->port);

  // send request
  send_request(sockfd, urlinfo->hostname, urlinfo->port, urlinfo->path);

  // receive request -> print to stdout
  recv_response(sockfd, buf, argc, argv, urlinfo);

  // clean up
  free(urlinfo->hostname);
  free(urlinfo->path);
  free(urlinfo->port);
  free(urlinfo);

  close(sockfd);

  return 0;
}
