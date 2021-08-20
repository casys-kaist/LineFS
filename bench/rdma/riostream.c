/*
 * Copyright (c) 2011-2012 Intel Corporation.  All rights reserved.
 * Copyright (c) 2014 Mellanox Technologies LTD. All rights reserved.
 *
 * This software is available to you under the OpenIB.org BSD license
 * below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AWV
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <rdma/rdma_cma.h>
#include <rdma/rsocket.h>
#include "common.h"

struct test_size {
	int size;
	char str[20];
};

#define kb 1024
#define mb 1024*1024

#define PERSIST 1

static struct test_size test_sizes[] = {
	{ 4*kb, "4kb" }, { 64*kb, "64kb" },
    { 1*mb, "1mb" }, { 4*mb, "4mb" }
};
#define TEST_CNT (sizeof test_sizes / sizeof test_sizes[0])

static int rs, lrs;
static int use_async;
static int use_rgai;
static int verify;
static int flags = MSG_DONTWAIT;
static int poll_timeout = 0;
static int custom;
static enum rs_optimization optimization;
static int size_option;
static int iterations = 300;
static int buffer_size, inline_size = 64;
static char test_name[10] = "custom";
static char *port = "7471";
static char *dst_addr;
static char *src_addr;
static int n_clients;
static struct timeval start, end;
static void *buf;
static volatile uint8_t *poll_byte;
static struct rdma_addrinfo rai_hints;
static struct addrinfo ai_hints;




static int send_msg(int size)
{
	struct pollfd fds;
	int offset, ret;

	if (use_async) {
		fds.fd = rs;
		fds.events = POLLOUT;
	}

	for (offset = 0; offset < size; ) {
		if (use_async) {
			ret = do_poll(&fds, poll_timeout);
			if (ret)
				return ret;
		}

		ret = rsend(rs, buf + offset, size - offset, flags);
		if (ret > 0) {
			offset += ret;
		} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("rsend");
			return ret;
		}
	}

	return 0;
}

static int send_xfer(int size)
{
	struct pollfd fds;
	int offset, ret;

	if (use_async) {
		fds.fd = rs;
		fds.events = POLLOUT;
	}

	for (offset = 0; offset < size; ) {
		if (use_async) {
			ret = do_poll(&fds, poll_timeout);
			if (ret)
				return ret;
		}

		ret = riowrite(rs, buf + offset, size - offset, offset, flags);
		if (ret > 0) {
			offset += ret;
		} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("riowrite");
			return ret;
		}
	}

	return 0;
}

static int recv_msg(int size)
{
	struct pollfd fds;
	int offset, ret;

	if (use_async) {
		fds.fd = rs;
		fds.events = POLLIN;
	}

	for (offset = 0; offset < size; ) {
		if (use_async) {
			ret = do_poll(&fds, poll_timeout);
			if (ret)
				return ret;
		}

		ret = rrecv(rs, buf + offset, size - offset, flags);
		if (ret > 0) {
			offset += ret;
		} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("rrecv");
			return ret;
		}
	}

	return 0;
}

static int recv_xfer(int size, uint8_t marker)
{
	int ret;

	while (*poll_byte != marker)
		;

	if (verify) {
		ret = verify_buf(buf, size - 1);
		if (ret)
			return ret;
	}

	return 0;
}

static int send_ack(int size)
{
    struct pollfd fds;
	int offset, ret;
    //send only last byte                                                                                                                                                                                                                    
	ret = riowrite(rs, buf + size -1, 1, size-1, flags);
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
        perror("riowrite");
	   return ret;
    }
    return 0;
}


static int recv_ack(int size, uint8_t marker)
{
 	int ret;
    while (*poll_byte != marker);
	return 0;
}

static int sync_test(void)
{
	int ret;

	ret = dst_addr ? send_msg(16) : recv_msg(16);
	if (ret)
		return ret;

	return dst_addr ? recv_msg(16) : send_msg(16);
}

int setup_fs(void){
    int fd = open("log", O_CREAT | O_APPEND | O_WRONLY, S_IRUSR | S_IWUSR);
	if(fd == -1) {
        perror("cannot open log file");
    }
    
    return fd;
}

static int persist(int fd, int size)
{
    //TODO: write a switch that differentiates
    //MLFS and ext4 and whatnot

    // Persist RPC packet                                                                                                                                                                                                                      
    int ret = write(fd, buf, (ssize_t) size - 1);
    if (ret == -1) {
        perror("cannot write data to log");
        return ret;
    }
    fsync(fd);
}

static int run_test(int size, char* name)
{
	int ret, i, t;
	off_t offset;
	uint8_t marker = 0;

	poll_byte = buf + size - 1;
	*poll_byte = -1;
	offset = riomap(rs, buf, size, PROT_WRITE, 0, 0);
	if (offset ==  -1) {
		perror("riomap");
		ret = -1;
		goto out;
	}
	ret = sync_test();
	if (ret)
		goto out;

    double time_acc = 0.0;
    double time_min = 0.0;
    double time_max = 0.0;

    int fd = setup_fs();

    printf("%-8s%-20s%-8s%-8s%-8s\n", "size", "throughput (mb/s)", "avg(us)", "max(us)", "min(us)");
	for (i = 0; i < iterations; i++) {
		gettimeofday(&start, NULL);
        /*
         *  CLIENT/SENDER
         */
        if (dst_addr) {
			*poll_byte = (uint8_t) marker++;
			ret = send_xfer(size);
			if (ret)
				goto out;

			ret = recv_ack(size, marker++);
		} 
        /*
         *  SERVER/RECEIVER
         */
        else {
			ret = recv_xfer(size, marker++);
			if (ret)
				goto out;
        
            if(PERSIST) {
                persist(fd, size);
            }

			*poll_byte = (uint8_t) marker++;
			ret = send_xfer(size);
		}
		if (ret)
			goto out;
        
        gettimeofday(&end, NULL);

        double elaps = (end.tv_sec - start.tv_sec) * 1000000 
                + (end.tv_usec - start.tv_usec);
    
        time_acc += elaps;
        time_max = elaps > time_max ? elaps : time_max;
        time_min = elaps < time_min || time_min == 0.0 ? elaps : time_min;
	}
	
	long long bytes = iterations * size;
    
    printf("%-8s%-20.4f%-9.0f%-9.0f%-9.0f\n", name, bytes/time_acc, time_acc/iterations, time_max, time_min);
	

	ret = riounmap(rs, buf, size);
out:
	return ret;
}

static void set_options(int rs)
{
	int val;

	if (buffer_size) {
		rsetsockopt(rs, SOL_SOCKET, SO_SNDBUF, (void *) &buffer_size,
			    sizeof buffer_size);
		rsetsockopt(rs, SOL_SOCKET, SO_RCVBUF, (void *) &buffer_size,
			    sizeof buffer_size);
	} else {
		val = 1 << 19;
		rsetsockopt(rs, SOL_SOCKET, SO_SNDBUF, (void *) &val, sizeof val);
		rsetsockopt(rs, SOL_SOCKET, SO_RCVBUF, (void *) &val, sizeof val);
	}

	val = 1;
	rsetsockopt(rs, IPPROTO_TCP, TCP_NODELAY, (void *) &val, sizeof(val));
	rsetsockopt(rs, SOL_RDMA, RDMA_IOMAPSIZE, (void *) &val, sizeof val);

	if (flags & MSG_DONTWAIT)
		rfcntl(rs, F_SETFL, O_NONBLOCK);

	/* Inline size based on experimental data */
	if (optimization == opt_latency) {
		rsetsockopt(rs, SOL_RDMA, RDMA_INLINE, &inline_size,
			    sizeof inline_size);
	} else if (optimization == opt_bandwidth) {
		val = 0;
		rsetsockopt(rs, SOL_RDMA, RDMA_INLINE, &val, sizeof val);
	}
}

static int server_listen(void)
{
	struct rdma_addrinfo *rai = NULL;
	struct addrinfo *ai;
	int val, ret;

	if (use_rgai) {
		rai_hints.ai_flags |= RAI_PASSIVE;
		ret = rdma_getaddrinfo(src_addr, port, &rai_hints, &rai);
	} else {
		ai_hints.ai_flags |= AI_PASSIVE;
		ret = getaddrinfo(src_addr, port, &ai_hints, &ai);
	}
	if (ret) {
		printf("getaddrinfo: %s\n", gai_strerror(ret));
		return ret;
	}

	lrs = rai ? rsocket(rai->ai_family, SOCK_STREAM, 0) :
		    rsocket(ai->ai_family, SOCK_STREAM, 0);
	if (lrs < 0) {
		perror("rsocket");
		ret = lrs;
		goto free;
	}

	val = 1;
	ret = rsetsockopt(lrs, SOL_SOCKET, SO_REUSEADDR, &val, sizeof val);
	if (ret) {
		perror("rsetsockopt SO_REUSEADDR");
		goto close;
	}

	ret = rai ? rbind(lrs, rai->ai_src_addr, rai->ai_src_len) :
		    rbind(lrs, ai->ai_addr, ai->ai_addrlen);
	if (ret) {
		perror("rbind");
		goto close;
	}

	ret = rlisten(lrs, 1);
	if (ret)
		perror("rlisten");

close:
	if (ret)
		rclose(lrs);
free:
	if (rai)
		rdma_freeaddrinfo(rai);
	else
		freeaddrinfo(ai);
	return ret;
}

static int server_connect(void)
{
	struct pollfd fds;
	int ret = 0;

	set_options(lrs);
	do {
		if (use_async) {
			fds.fd = lrs;
			fds.events = POLLIN;

			ret = do_poll(&fds, poll_timeout);
			if (ret) {
				perror("rpoll");
				return ret;
			}
		}

		rs = raccept(lrs, NULL, 0);
	} while (rs < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
	if (rs < 0) {
		perror("raccept");
		return rs;
	}

	set_options(rs);
	return ret;
}

static int client_connect(void)
{
	struct rdma_addrinfo *rai = NULL;
	struct addrinfo *ai;
	struct pollfd fds;
	int ret, err;
	socklen_t len;

	ret = use_rgai ? rdma_getaddrinfo(dst_addr, port, &rai_hints, &rai) :
			 getaddrinfo(dst_addr, port, &ai_hints, &ai);
	if (ret) {
		printf("getaddrinfo: %s\n", gai_strerror(ret));
		return ret;
	}

	rs = rai ? rsocket(rai->ai_family, SOCK_STREAM, 0) :
		   rsocket(ai->ai_family, SOCK_STREAM, 0);
	if (rs < 0) {
		perror("rsocket");
		ret = rs;
		goto free;
	}

	set_options(rs);
	/* TODO: bind client to src_addr */

	ret = rai ? rconnect(rs, rai->ai_dst_addr, rai->ai_dst_len) :
		    rconnect(rs, ai->ai_addr, ai->ai_addrlen);
	if (ret && (errno != EINPROGRESS)) {
		perror("rconnect");
		goto close;
	}

	if (ret && (errno == EINPROGRESS)) {
		fds.fd = rs;
		fds.events = POLLOUT;
		ret = do_poll(&fds, poll_timeout);
		if (ret) {
			perror("rpoll");
			goto close;
		}

		len = sizeof err;
		ret = rgetsockopt(rs, SOL_SOCKET, SO_ERROR, &err, &len);
		if (ret)
			goto close;
		if (err) {
			ret = -1;
			errno = err;
			perror("async rconnect");
		}
	}

close:
	if (ret)
		rclose(rs);
free:
	if (rai)
		rdma_freeaddrinfo(rai);
	else
		freeaddrinfo(ai);
	return ret;
}

static int run(void)
{
	int i, ret = 0;

    //allocate largest buffer we use (assume its the last)
    buf = malloc(test_sizes[TEST_CNT - 1].size);

	if (!dst_addr) {
		ret = server_listen();
		if (ret)
			goto free;
	}

    //our RPC throughput depends on latency, so ill use the latency optimization
    //for now.
	//optimization = opt_bandwidth;
    
	optimization = opt_latency;
	ret = dst_addr ? client_connect() : server_connect();
	if (ret)
		goto free;

	for (i = 0; i < TEST_CNT; i++) {
		run_test(test_sizes[i].size, test_sizes[i].str);
	}

	rshutdown(rs, SHUT_RDWR);
	rclose(rs);

free:
	free(buf);
	return ret;
}

int main(int argc, char **argv)
{
	int op, ret;
	ai_hints.ai_socktype = SOCK_STREAM;
	rai_hints.ai_port_space = RDMA_PS_TCP;

    //always test all sizes
    size_option = 1;
    n_clients = 1;
	while ((op = getopt(argc, argv, "s:b:f:B:i:I:C:S:p:T:")) != -1) {
		switch (op) {
		case 's':
			dst_addr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
        case 'n':
			n_clients = atoi(optarg);
			break;
		default:
			printf("usage: %s\n", argv[0]);
			printf("\t[-s server_address]\n");
			printf("\t[-p port_number]\n");
			exit(1);
		}
	}

	return run();
}
