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

#define TCP_LAT_USING_EPOLL 0

#if TCP_LAT_USING_EPOLL
#include <sys/epoll.h>
#endif

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

void set_quickack(int fd) {
    int zero = 0;
    if (setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &zero, sizeof(zero)) < 0) {
        perror("TCP_QUICKACK");
    }
}

int process_message(int new_fd, int efd, int message_size, int polling) {
    char buf[message_size];
    int sofar = 0;
    int chunks = 0;

    while (sofar < message_size) {
        struct timeval tp;
#if TCP_LAT_USING_EPOLL
        struct epoll_event events[1];
    //printf("Before epoll_wait\n");
        int n = epoll_wait(efd, events, 1, 0);
        if (n < 0) {
            n = errno;
            if (n != EINTR) {
                perror("epoll_wait");
                return 0;
            }
            continue;
        }
        if (n == 0)
            continue; // busy polling
//        printf("Before recv\n");
#endif
        ssize_t len = recv(new_fd, buf + sofar, message_size - sofar, polling ? MSG_DONTWAIT : 0);
        if (len == -1) {
            int err = errno;
            if (err != EAGAIN && err != EWOULDBLOCK) {  // some REAL error
                perror("read");
                return 0;
            }
            continue;
        }
        if (len == 0) {
            printf("disconnect'n");
            return 0; // disconnect
        }
        set_quickack(new_fd);
        sofar += len;
//        if (chunks == 0) {
//            gettimeofday(&tp, NULL);
//            printf("read at: %3li:%6li\n", (long) (tp.tv_sec % 1000), (long) tp.tv_usec);
//        }
        chunks += 1;
    }

    if (write(new_fd, buf, message_size) != message_size) {
        perror("write");
        return 0;
    }
    return 1;
}

void socket_accepted(int new_fd, int message_size, int polling) {
    int set = 1;
    int efd = -1;
    if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(set)) < 0) {
        perror("nodelay");
        return;
    }
    if (polling) {
        if (!set_nonblocking(new_fd)) {
            perror("set_nonblocking");
            return;
        }
    }
#if TCP_LAT_USING_EPOLL
    efd = epoll_create1(0);
  if (efd < 0) {
      perror("epoll_create1");
    return;
  }
  struct epoll_event event = {EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP | EPOLLET, {.ptr = 0}};
  if (epoll_ctl(efd, EPOLL_CTL_ADD, new_fd, &event) < 0) {
      perror("epoll_ctl");
      return;
  }
#endif
    set_quickack(new_fd);
  while (process_message(new_fd, efd, message_size, polling))
      ;
}

int main(int argc, char *argv[]) {
  int yes = 1;
  int ret;
  struct sockaddr_storage their_addr;
  socklen_t addr_size;
  struct addrinfo hints;
  struct addrinfo *res;
  int sockfd, new_fd;

  if (argc != 5) {
    printf("usage: tcp_local_lat <bind-to> <port> <message-size> <polling>\n");
    return 1;
  }

  int size = atoi(argv[3]);
  int polling = atoi(argv[4]);

//  buf = malloc(size);
//  if (buf == NULL) {
//    perror("malloc");
//    return 1;
//  }

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

  while(1) {
      addr_size = sizeof their_addr;

      if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size)) ==
          -1) {
          perror("accept");
          continue;
      }
      socket_accepted(new_fd, size, polling);
  }
  return 0;
}
