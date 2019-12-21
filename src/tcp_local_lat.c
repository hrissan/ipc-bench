/*
    Measure latency of IPC using tcp sockets


    Copyright (c) 2016 Erik Rigtorp <erik@rigtorp.se>

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use,
    copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following
    conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
    OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
    HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
    OTHER DEALINGS IN THE SOFTWARE.
*/

#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>

int set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {   
	    perror("fcntl 1");
        return 0;
    }
	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0) {   
	    perror("fcntl 2");
        return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
  size_t size;
  char *buf;

  int yes = 1;
  int ret;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints;
  struct addrinfo *res;
  int sockfd, new_fd;

  if (argc != 4) {
    printf("usage: tcp_local_lat <bind-to> <port> <message-size>\n");
    return 1;
  }

  size = atoi(argv[3]);

  buf = malloc(size);
  if (buf == NULL) {
    perror("malloc");
    return 1;
  }

  printf("message size: %i octets\n", size);
//   printf("roundtrip count: %li\n", count);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  if ((ret = getaddrinfo(argv[1], argv[2], &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return 1;
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perror("socket");
    return 1;
  }

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("setsockopt");
    return 1;
  }

  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind");
    return 1;
  }

  if (listen(sockfd, 1) == -1) {
    perror("listen");
    return 1;
  }

  addr_size = sizeof their_addr;

  if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) ==
      -1) {
    perror("accept");
    return 1;
  }

	int set = 1;	
  if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(set)) < 0) {
    perror("nodelay");
    return 1;
  }
  if (!set_nonblocking(new_fd)) {
    return 1;
  }

  while (1) {
    size_t sofar = 0;

    while (sofar < size) {
	  struct timeval tp;
      ssize_t len = recv(new_fd, buf + sofar, size - sofar, MSG_DONTWAIT);
      if (len == -1) {
		int err = errno;
		if (err != EAGAIN && err != EWOULDBLOCK) {  // some REAL error
	        perror("read");
	        return 1;
	    }
        continue;
      }
      if (len == 0)
      	break;
      sofar += len;
	  gettimeofday(&tp, NULL);
	     printf("read at: %3li:%6li\n", (long)(tp.tv_sec % 1000), (long)tp.tv_usec);

    }

    if (write(new_fd, buf, size) != size) {
      perror("write");
      return 1;
    }
  }

  return 0;
}
