#ifdef HYPERLOOP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "global/mem.h"
#include "global/global.h"
#include "libhyperclient/hyperclient_compat.h"
#include "libhyperclient/operations_compat.h"
#include "hyperloop/hyperloop_client.h"
#include "distributed/rpc_interface.h"

#define HYPERLOOP_WINDOW_SIZE 1000  // # of operations to pre-post at once.
// #define HYPERLOOP_PRINT_BUFFER 1

static const char* default_device_name = "mlx5_0";
static const int default_device_port = 1;

static void gWriteTest();
static void gMemcpyTest();
static void gCompareAndSwapTest();
static void gFlushTest();

// Client and the first replica of Hyperloop are colocated.
char server_ip_data[100] = "";
char server_ip_meta[100] = "";
static const char* data_server_port = "50000";
static const char* meta_server_port = "50001";
struct compat_hyperclient *data_client;
struct compat_hyperclient *meta_client;
char* buffer;
struct rep_ops {
    uint64_t offset;
    uint64_t size;
};
struct rep_ops *all_ops; // Storing all gwrite operations.
unsigned int cur_id = 0; /* index of op that is going to be sent.
			  * (= the number of ops sent)
			  * NOTE current implementation does not support
			  * multiple threads run.
			  */
unsigned int total_num_ops; // the number of all_ops entries.

uint64_t meta_offset;
int meta_size; // Size of metadata to be replicated.
char *g_meta_addr;

static void load_ops(void);
static void send_hyperloop_operations(void);

/**
 * @Synopsis Initialize hyperloop client. It sets variables, connects to
 * servers, and sends all operations loaded from a trace file.
 * g_sync_ctx[]->peer (= get_next_peer()) is registered as Hyperloop Server's
 * buffer. We are replicating only a part of g_sync_ctx[]->peer so it is
 * required to set a proper offset and size. Additionally, the interesting
 * metadata should be placed contiguously.
 *
 * @Param buf Buffer address for data client. Log base address.
 * @Param buf_size The size of data client buffer. Log size.
 * @Param meta_addr Buffer address for metadata client. It should be
 * g_sync_ctx[0]->peer.
 * @Param metadata_size The size of a metadata client buffer. That is, the size
 * of metadata replicated.
 */
void hyperloop_client_init(char *buf, uint64_t buf_size,
			   char *meta_addr, int metadata_size) {
    buffer = buf;
    g_meta_addr = meta_addr;

    // Get offset of interesting metadata within g_sync_ctx[]->peer.
    meta_offset = (uintptr_t)&get_next_peer()->local_start -
                  (uintptr_t)get_next_peer();
    meta_size = metadata_size;

    mlfs_printf("hyperloop client buf=%p meta_addr=0x%p meta_offset=%lu "
                "meta_size=%d\n",
                buffer, meta_addr, meta_offset, meta_size);

    strcat(server_ip_data, get_next_peer()->info->ip);
    strcat(server_ip_data, ":");
    strcat(server_ip_data, data_server_port);
    strcat(server_ip_meta, get_next_peer()->info->ip);
    strcat(server_ip_meta, ":");
    strcat(server_ip_meta, meta_server_port);

    mlfs_printf("Hyperloop data client is connecting to %s\n", server_ip_data);
    mlfs_printf("Hyperloop meta client is connecting to %s\n", server_ip_meta);

    // Setup chain to the next server.
    data_client = compat_hyperclient_connect(
        default_device_name, default_device_port, server_ip_data, buf, buf_size,
        0, HYPERLOOP_WINDOW_SIZE);

    // Register g_sync_ctx[0]->peer as a buffer.
    meta_client = compat_hyperclient_connect(
        default_device_name, default_device_port, server_ip_meta,
        meta_addr, sizeof(peer_meta_t), 0, HYPERLOOP_WINDOW_SIZE);

    load_ops();
    send_hyperloop_operations();
}


void shutdown_hyperloop_client(void) {
    compat_hyperclient_destory(data_client);
    compat_hyperclient_destory(meta_client);
    mlfs_free(all_ops);
}

// Load all operations from a file.
static void load_ops(void) {
    FILE *fp;
    uint64_t offset;
    uint32_t size, line_cnt = 0, i = 0;
    int ret;

    // fp = fopen("/tmp/hyp_ops.sw.1g.4k", "r");
    printf("file path:%s\n", mlfs_conf.hyperloop_ops_file_path);
    fp = fopen(mlfs_conf.hyperloop_ops_file_path, "r");
    if (!fp)
	panic("Hyperloop trace file open failed.");

    // Get line count.
    while (EOF != (ret = fscanf(fp, "%*[^\n]"), fscanf(fp, "%*c")))
	line_cnt++;

    total_num_ops = line_cnt;
    printf("total_num_ops:%u\n", total_num_ops);
    all_ops = mlfs_zalloc(sizeof(struct rep_ops) * total_num_ops);

    lseek(fileno(fp), 0, SEEK_SET);

    while(fscanf(fp, "%lu,%u", &offset, &size) != EOF){
	all_ops[i].offset = offset;
	all_ops[i].size = size;
	i++;
    }
    mlfs_assert(i == total_num_ops);
}

/**
 * Send all operations to servers.
 *
 * A replication is composed of three hyperloop operations:
 *  - gwrite of data
 *  - gflush of data
 *  - gwrite of metadata
 *
 * It sends all the three operations.
 */
static void send_hyperloop_operations(void) {
    struct compat_hyperloop_operation *data_ops;
    struct compat_hyperloop_operation *meta_ops;
    int i, j;
    uint64_t offset, size;

    data_ops = (struct compat_hyperloop_operation *)mlfs_zalloc(
        sizeof(struct compat_hyperloop_operation) * total_num_ops * 2);
    meta_ops = (struct compat_hyperloop_operation *)mlfs_zalloc(
        sizeof(struct compat_hyperloop_operation) * total_num_ops);

    // build hyperloop operations
    for (i = 0; i < total_num_ops; i++) {
	j = i * 2;
	offset = all_ops[i].offset;
	size = all_ops[i].size;

	// gwrite data
	data_ops[j].type = OPERATION_TYPE_GWRITE;
	data_ops[j].op.gWrite.offset = offset;
	data_ops[j].op.gWrite.size = size;

	// gflush data
	data_ops[j + 1].type = OPERATION_TYPE_GFLUSH;
	data_ops[j + 1].op.gFlush.offset = offset + size - 1; // Read last byte.
	data_ops[j + 1].op.gFlush.size = 1;

	// gwrite metadata
	meta_ops[i].type = OPERATION_TYPE_GWRITE;
        meta_ops[i].op.gWrite.offset = meta_offset;

        meta_ops[i].op.gWrite.size = meta_size;
    }

    compat_hyperclient_send_operations(data_client, data_ops, total_num_ops*2);
    compat_hyperclient_send_operations(meta_client, meta_ops, total_num_ops);

    mlfs_free(data_ops);
    mlfs_free(meta_ops);
}

static uint64_t data_op_cnt = 0;
/**
 * gWrite and gFlush data
 */
void do_hyperloop_gwrite_data(void) {
    compat_hyperclient_execute_operations(data_client, 2); // 2: gwrite + gflush
    // mlfs_printf("hyperloop gwrite DATA:0x%lx size=%lu\n", all_ops[data_op_cnt].offset, all_ops[data_op_cnt].size);
#ifdef HYPERLOOP_PRINT_BUFFER
    compat_hyperclient_send_print_buffer(data_client,
                                         all_ops[data_op_cnt].offset,
                                         (size_t)all_ops[data_op_cnt].size);
#endif
    data_op_cnt += 2;
}

static uint64_t meta_op_cnt = 0;
/**
 * gWrite metadata
 */
void do_hyperloop_gwrite_meta(void) {
    compat_hyperclient_execute_operations(meta_client, 1); // 1: gwrite
    // mlfs_printf("hyperloop gwrite meta:0x%p size=%lu val=%lu\n",
    //             g_meta_addr + meta_offset, (size_t)meta_size,
    //             *(uint64_t *)(g_meta_addr + meta_offset));
#ifdef HYPERLOOP_PRINT_BUFFER
    compat_hyperclient_send_print_buffer(meta_client, meta_offset, (size_t)meta_size);
#endif
    meta_op_cnt++;
}

#endif /* HYPERLOOP */
