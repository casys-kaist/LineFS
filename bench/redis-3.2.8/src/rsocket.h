#ifndef _RSOCKET_H_
#define _RSOCKET_H_

#include <stdio.h>
#include <unistd.h>

#ifdef __RDMA__
#include <rdma/rsocket.h>

#define MAXFDS 256
extern char rsocket_fds[MAXFDS];

#define IS_RSOCK_FD(fd) (fd < MAXFDS && rsocket_fds[fd])

#define call_if(sock_fun, fd, args...) ({ \
   int __ret; \
   if (IS_RSOCK_FD(fd)) {\
      __ret = r##sock_fun(fd, args); \
   }\
   else {\
      __ret = sock_fun(fd, args); \
   }\
   __ret;\
})


static inline int __rsocket(int domain, int type, int protocol)
{
   int ret = rsocket(domain, type, protocol);
   if (ret >= 0) {
      if (ret >= MAXFDS) {
         fprintf(stderr, "Rsocket with fd too high (%d)!, adjust MAXFDS macro in(%s)\n",
               ret, __FILE__);
         exit(1);
      }
      rsocket_fds[ret] = 1;
   }
   return ret;
}

static inline int __raccept(int sockfd, struct sockaddr* addr, socklen_t *addrlen)
{

   int ret;
   if (IS_RSOCK_FD(sockfd)) {
      ret = raccept(sockfd, addr, addrlen);
      if (ret >= 0)
         rsocket_fds[ret] = 1;
   }
   else {
      ret = accept(sockfd, addr, addrlen);
   }
   return ret;
}

static inline int __rclose(int fd)
{
   int ret;
   if (IS_RSOCK_FD(fd)) {
      ret = rclose(fd);
      if (ret >= 0)
         rsocket_fds[fd] = 0;
   } else
      ret = close(fd);
   return ret;
}

# define rs_socket(args...)       __rsocket(args)
# define rs_bind(args...)         call_if(bind, args)
# define rs_listen(args...)       call_if(listen, args)
# define rs_accept(args...)       __raccept(args)
# define rs_connect(args...)      call_if(connect, args)
# define rs_shutdown(args...)     call_if(shutdow, args)
# define rs_close(args...)        __rclose(args)
# define rs_recv(args...)         call_if(recv, args)
# define rs_recvfrom(args...)     call_if(recvfrom, args)
# define rs_recvmsg(args...)      call_if(recvmsg, args)
# define rs_read(args...)         call_if(read, args)
# define rs_readv(args...)        call_if(readv, args)
# define rs_send(args...)         call_if(send, args)
# define rs_sendto(args...)       call_if(sendto, args)
# define rs_sendmsg(args...)      call_if(sendmsg, args)
# define rs_write(args...)        call_if(write, args)
# define rs_writev(args...)       call_if(writev, args)
# define rs_poll(args...)         rpoll(args)
# define rs_select(args...)       rselect(args)
# define rs_getpeername(args...)  call_if(getpeername, args)
# define rs_getsockname(args...)  call_if(getsockname, args)
# define rs_setsockopt(args...)   call_if(setsockopt, args)
# define rs_getsockopt(args...)   call_if(getsockopt, args)
# define rs_fcntl(args...)        call_if(fcntl, args)

#else

# define rs_socket(args...)       socket(args)
# define rs_bind(args...)         bind(args)
# define rs_listen(args...)       listen(args)
# define rs_accept(args...)       accept(args)
# define rs_connect(args...)      connect(args)
# define rs_shutdown(args...)     shutdown(args)
# define rs_close(args...)        close(args)
# define rs_recv(args...)         recv(args)
# define rs_recvfrom(args...)     recvfrom(args)
# define rs_recvmsg(args...)      recvmsg(args)
# define rs_read(args...)         read(args)
# define rs_readv(args...)        readv(args)
# define rs_send(args...)         send(args)
# define rs_sendto(args...)       sendto(args)
# define rs_sendmsg(args...)      sendmsg(args)
# define rs_write(args...)        write(args)
# define rs_writev(args...)       writev(args)
# define rs_poll(args...)         poll(args)
# define rs_select(args...)       select(args)
# define rs_getpeername(args...)  getpeername(args)
# define rs_getsockname(args...)  getsockname(args)
# define rs_setsockopt(args...)   setsockopt(args)
# define rs_getsockopt(args...)   getsockopt(args)
# define rs_fcntl(args...)        fcntl(args)

#endif

#endif
