#ifndef _R_SOCK_H
#define _R_SOCK_H

#ifdef __RDMA__
#include<rdma/rsocket.h>

#define rs_bind(args...)         rbind(args)
#define rs_listen(args...)       rlisten(args)
#define rs_connect(args...)      rconnect(args)
#define rs_shutdown(args...)     rshutdown(args)
#define rs_recv(args...)         rrecv(args)
#define rs_recvfrom(args...)     rrecvfrom(args)
#define rs_recvmsg(args...)      rrecvmsg(args)
#define rs_read(args...)         rread(args)
#define rs_readv(args...)        rreadv(args)
#define rs_send(args...)         rsend(args)
#define rs_sendto(args...)       rsendto(args)
#define rs_sendmsg(args...)      rsendmsg(args)
#define rs_write(args...)        rwrite(args)
#define rs_writev(args...)       rwritev(args)
#define rs_getpeername(args...)  rgetpeername(args)
#define rs_getsockname(args...)  rgetsockname(args)
#define rs_setsockopt(args...)   rsetsockopt(args)
#define rs_getsockopt(args...)   rgetsockopt(args)
#define rs_fcntl(args...)        rfcntl(args)
#define rs_select(args...)       rselect(args)
#define rs_poll(args...)         rpoll(args)
#define rs_fcntl(args...)        rfcntl(args)
#define rs_close(args...)        rclose(args)
#define rs_socket(args...)       rsocket(args)
#define rs_accept(args...)       raccept(args)

#else

#define rs_bind(args...)         bind(args)
#define rs_listen(args...)       listen(args)
#define rs_connect(args...)      connect(args)
#define rs_shutdown(args...)     shutdown(args)
#define rs_recv(args...)         recv(args)
#define rs_recvfrom(args...)     recvfrom(args)
#define rs_recvmsg(args...)      recvmsg(args)
#define rs_read(args...)         read(args)
#define rs_readv(args...)        readv(args)
#define rs_send(args...)         send(args)
#define rs_sendto(args...)       sendto(args)
#define rs_sendmsg(args...)      sendmsg(args)
#define rs_write(args...)        write(args)
#define rs_writev(args...)       writev(args)
#define rs_getpeername(args...)  getpeername(args)
#define rs_getsockname(args...)  getsockname(args)
#define rs_setsockopt(args...)   setsockopt(args)
#define rs_getsockopt(args...)   getsockopt(args)
#define rs_fcntl(args...)        fcntl(args)
#define rs_select(args...)       select(args)
#define rs_poll(args...)         poll(args)
#define rs_fcntl(args...)        fcntl(args)
#define rs_close(args...)        close(args)
#define rs_socket(args...)       socket(args)
#define rs_accept(args...)       accept(args)

#endif
#endif
