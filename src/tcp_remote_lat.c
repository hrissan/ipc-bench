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
  int size;
  char *buf;
  int64_t count, i, delta;
  struct timeval start, stop;

  int ret;
  struct addrinfo hints;
  struct addrinfo *res;
  int sockfd;

  if (argc != 6) {
    printf("usage: tcp_lat <bind-to> <host> <port> <message-size> "
           "<roundtrip-count>\n");
    return 1;
  }

  size = atoi(argv[4]);
  count = atol(argv[5]);

  buf = malloc(size);
  if (buf == NULL) {
    perror("malloc");
    return 1;
  }

  printf("message size: %i octets\n", size);
  printf("roundtrip count: %li\n", count);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, whichever
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE; // fill in my IP for me
  if ((ret = getaddrinfo(argv[1], NULL, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return 1;
  }

  if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) ==
      -1) {
    perror("socket");
    return 1;
  }

//  int set                              = 1;
//  if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(set)) < 0) {
//    perror("nodelay");
//    return 1;
//  }


//if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
//    perror("bind");
//    return 1;
//  }

  if ((ret = getaddrinfo(argv[2], argv[3], &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    return 1;
  }

//  if (!set_nonblocking(sockfd)) {
//    return 1;
//  }

  if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		int err = errno;
		if (err != EINPROGRESS) {  // some REAL error
		    perror("connect");
	        return 1;
	    }
	    printf("connect EAGAIN\n");
  }

  	sleep(1);

  gettimeofday(&start, NULL);

  for (i = 0; i < count; i++) {
  
  	sleep(1);

	  struct timeval tp1;
	  gettimeofday(&tp1, NULL);
    if (write(sockfd, buf, size) != size) {
      perror("write");
      return 1;
    }

    size_t sofar = 0;

    while (sofar < size) {
	  struct timeval tp2;
      ssize_t len = recv(sockfd, buf + sofar, size - sofar, MSG_DONTWAIT);
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
	  gettimeofday(&tp2, NULL);
	  delta =
    	  (tp2.tv_sec - tp1.tv_sec) * 1000000 + (tp2.tv_usec - tp1.tv_usec);
        printf("write at: %3li:%6li ns\n", (long)(tp1.tv_sec % 1000), (long)tp1.tv_usec);
         printf("read at: %3li:%6li, %li mks\n", (long)(tp2.tv_sec % 1000), (long)tp2.tv_usec, (long)delta);
    }
  }

  gettimeofday(&stop, NULL);

//  delta =
//      (stop.tv_sec - start.tv_sec) * 1000000000 + (stop.tv_usec - start.tv_usec) * 1000;

//  printf("average latency: %li ns\n", delta / (count * 2));

  return 0;
}
