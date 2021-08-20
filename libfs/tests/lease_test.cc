#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "mlfs/mlfs_interface.h"

// #include "storage/spdk/async.h"
// #include "storage/spdk/sync.h"
#include "time_stat.h"
#include "thread.h"
#include <signal.h>

#define NOPRINT_STAT
#ifdef MLFS
#define MLFS_ENABLE 1
#elif CEPH
#define CEPH_ENABLE 1
#endif

#ifdef MLFS_ENABLE
#define test_dir_prefix "/mlfs/"
#elif CEPH_ENABLE
#define test_dir_prefix "/mnt/cephfs/"	// Temp dir. Lenth should be the same as /mlfs/
#else
//#define test_dir_prefix "/mlte/"	// Temp dir. Lenth should be the same as /mlfs/
#define test_dir_prefix "./lease_test/"
#endif

#define TEST_DIR_PATH "fileset/"
#define SHM_PATH "/shm_lease_test"
#define SHM_F_SIZE 128
// #define OUTPUT_TO_FILE 1     // write output to file not stdout.

#define FILE_SIZE   4096        // file size in Bytes.
#define IO_SIZE_K   16           // io size in Kilo Bytes.

void* create_shm(int &fd, int &res) {
	void * addr;
	fd = shm_open(SHM_PATH, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		printf ("[LEASE_TEST] (%s) shm_open failed.\n", __func__);
		exit(-1);
	}

	res = ftruncate(fd, SHM_F_SIZE);
	if (res < 0)
	{
		printf ("[LEASE_TEST] (%s) ftruncate error.\n", __func__);
		exit(-1);
	}

	addr = mmap(NULL, SHM_F_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
	if (addr == MAP_FAILED){
		printf ("[LEASE_TEST] (%s) mmap failed.\n", __func__);
		exit(-1);
	}

	return addr;
}

void destroy_shm(void *addr) {
	int ret, fd;
	ret = munmap(addr, SHM_F_SIZE);
	if (ret < 0)
	{
		printf ("[LEASE_TEST] (%s) munmap error.\n", __func__);
		exit(-1);
	}

	fd = shm_unlink(SHM_PATH);
	if (fd < 0) {
		printf ("[LEASE_TEST] (%s) shm_unlink failed.\n", __func__);
		exit(-1);
	}
}

class io_bench : public CThread 
{
	public:
		io_bench(int _id, unsigned long _file_size_bytes, unsigned int _io_size,
				int _n_files, char _test_mode, bool _is_second_dir, char* _dirname, char* _dirname2, bool _do_verify, ofstream & _ofs);

		int id;
		unsigned long file_size_bytes;
		unsigned int io_size;
		int n_files;
		char test_mode;
		bool is_second_dir;
		char* dirname;
		char* dirname2;
                bool do_verify;
		std::vector<string> filenames;
		std::vector<string> filenames_re;
		char *buf;
		struct time_stats stats;
		struct time_stats rename_stats;
                ofstream *ofs_p;
                ofstream ofs;
		string target_d;
		string target_d_re;

		void prepare(void);

		void checkdir(void);
		void do_create_files(char *filename, char *filename_re);
		void do_read(char *filename);
		void do_write(int fd);
		void do_rename(char *filename, char *filename_re);
		void do_sanity_check(char *filename_re);
                void set_buffer(char* buf);

		// Thread entry point.
		void Run(void);

		// util methods
		static unsigned long str_to_size(char* str);
		static void hexdump(void *mem, unsigned int len);
		static void show_usage(const char *prog);
};

io_bench::io_bench(int _id, unsigned long _file_size_bytes, 
		unsigned int _io_size, int _n_files, char _test_mode,
                bool _is_second_dir, char* _dirname, char* _dirname2,
                bool _do_verify, ofstream & _ofs)
	: id(_id), 
	file_size_bytes(_file_size_bytes), 
	io_size(_io_size), 
	n_files(_n_files), 
	test_mode(_test_mode),
	is_second_dir(_is_second_dir),
	dirname(_dirname),
	dirname2(_dirname2),
        do_verify(_do_verify)
{
	std::string tdp(test_dir_prefix);
	if (dirname != NULL){
		target_d = std::string(tdp+dirname);
	}
	if (dirname2 != NULL){
		target_d_re = std::string(tdp+dirname2);
	}
	if (test_mode == 'n')
            cout << "[LEASE_TEST] Rename directory path:" << target_d << " -> " << target_d_re  << endl;
	ofs_p = &_ofs;
}

void io_bench::set_buffer(char* b) {
    for (unsigned long i = 0; i < file_size_bytes; i++)
        b[i] = '0' + (i % 9);
}

void io_bench::checkdir(void){
	int ret;
	struct stat file_info;
	std::string tdp(test_dir_prefix);
	std::string dir_path(TEST_DIR_PATH);
	string dir;
	string dir_re;
	if (dirname != NULL){
		dir = target_d + "/";
	}
	else{
		dir = tdp+dir_path;
	}

	if (dirname2 != NULL){
		dir_re = target_d_re + "/";
	}

	if(stat(dir.c_str(), &file_info) == -1) {
		//cout << "dir  not exists." << endl;
		ret = mkdir(dir.c_str(), 0777);
		if (ret < 0 && errno != EEXIST) { 
			perror("mkdir\n");
			exit(-1);
		}
	}else {
		;
		//cout << "dir exist! do not mkdir." << endl;
	}

	if (test_mode == 'n') {
		if(stat(dir_re.c_str(), &file_info) == -1){
			ret = mkdir(dir_re.c_str(), 0777);
			if (ret < 0 && errno != EEXIST) { 
				perror("mkdir\n");
				exit(-1);
			}
		}
	}
}

void io_bench::prepare(void)
{
	int ret;
	//struct stat file_info;
	std::string tdp(test_dir_prefix);
	std::string dir_path(TEST_DIR_PATH);
	string dir;
	string dir_re;
	if (dirname != NULL){
		dir = target_d + "/";
	}
	else{
		dir = tdp+dir_path;
	}

	if (dirname2 != NULL){
		dir_re = target_d_re + "/";
	}

	//if(stat(dir.c_str(), &file_info) == -1) {
	//	cout << "dir  not exists." << endl;
	//	ret = mkdir(dir.c_str(), 0777);
	//	if (ret < 0 && errno != EEXIST) { 
	//		perror("mkdir\n");
	//		exit(-1);
	//	}
	//}else {
	//	cout << "dir exist! do not mkdir." << endl;
	//}

	//if (test_mode == 'n') {
	//	if(stat(dir_re.c_str(), &file_info) == -1){
	//		ret = mkdir(dir_re.c_str(), 0777);
	//		if (ret < 0 && errno != EEXIST) { 
	//			perror("mkdir\n");
	//			exit(-1);
	//		}
	//	}
	//}

	buf = new char[file_size_bytes];

//	srand(time(NULL));   

        // Generate file names.
        string filename;
        string filename_re;
	if (test_mode != 'r'){
		for (int i = 0; i < n_files; i++) {
			if (test_mode == 'n'){
                                filename_re = dir_re + "f" + std::to_string(i)+ "_"+ std::to_string(getpid());
				// filename_re = dir_re + "f" + std::to_string(i);
				filenames_re.push_back(filename_re);
			}
			filename = dir + "f" + std::to_string(i)+ "_"+ std::to_string(getpid());
			filenames.push_back(filename);
		}
	} else {	// test_mode == 'r'
		// filename is already specified by argument.
		;
	}

	// Init time stat.
	time_stats_init(&stats, 1);
	time_stats_init(&rename_stats, n_files);
}

void io_bench::do_write(int fd)
{
WRITE_START:
        set_buffer(buf);
	// for (unsigned long i = 0; i < file_size_bytes; i++)
		// buf[i] = '0' + (i % 9);
		//buf[i] = '0' + (i % 5);

#define N_TESTS 1
	for (int test = 0 ; test < N_TESTS ; test++) {
		if (fd < 0) {
			err(1, "open");
		}

		int bytes_written;
		for (unsigned long i = 0; i < file_size_bytes; i += io_size) {
			if (i + io_size > file_size_bytes)
				io_size = file_size_bytes - i;
			else
				io_size = io_size;  // Why do this?

			//printf("Writing to file %u\n", io_size);
			bytes_written = write(fd, buf+i, io_size);

			if (bytes_written != io_size) {
				printf("write request %u received len %d\n",
						io_size, bytes_written);
				errx(1, "write");
			}
		}

	}

	/*
	printf("Throughput: %3.3f MB\n",(float)(file_size_bytes)
			/ (1024.0 * 1024.0 * (float) time_stats_get_avg(&stats)));

	time_stats_print(&stats, (char *)"---------------");
	*/

	return ;
}

void io_bench::do_read(char *filename)
{
	int fd;
	unsigned long i;
	int ret;

READ_START:
	printf("Try to read file %s\n", filename);
	if ((fd = open(filename, O_RDWR)) < 0)
		err(1, "open");

	printf("File size %lu bytes\n", (unsigned long)file_size_bytes);

	for(unsigned long i = 0; i < file_size_bytes; i++)
		buf[i] = 1;

	//time_stats_init(&stats, 1);
	//time_stats_start(&stats);

	cout << "File read:" << endl;
	cout << "---------------------" << endl;
	for (i = 0; i < file_size_bytes ; i += io_size) {
		if (i + io_size > file_size_bytes)
			io_size = file_size_bytes - i;
		else
			io_size = io_size;

		ret = read(fd, buf + i, io_size);
		cout << buf + i;
//#if 0
		if (ret != io_size) {
			printf("read size mismatch: return %d, request %u\n",
					ret, io_size);
		}
//#endif
	}
	cout << endl;
	cout << "---------------------" << endl;
	cout << "File read done." << endl;

	//time_stats_stop(&stats);

#if 0
	for (unsigned long i = 0; i < file_size_bytes; i++) {
		int bytes_read = read(fd, buf+i, io_size + 100);

		if (bytes_read != io_size) 
			printf("read too far: length %d\n", bytes_read);
	}
#endif


	/*
	printf("%f\n", (float) time_stats_get_avg(&stats));

	printf("Throughput: %3.3f MB\n",(float)((file_size_bytes) >> 20)
			/ (float) time_stats_get_avg(&stats));

	time_stats_print(&stats, (char *)"---------------");
	*/

	close(fd);

	return ;
}

void io_bench::do_sanity_check(char *filename_re)
{
	int fd;
	unsigned long i;
	int ret;
        struct stat file_info;
        char* buf_orig;
        bool dump_file;

        // Check existence.
        if (stat (filename_re, &file_info) != 0)
            cout << "[LEASE_TEST] Sanity check failed. File does not exist in the target directory: " << endl
                << "    file name: " << filename_re << endl;

        // Check file size.
        if (file_info.st_size != file_size_bytes)
            cout << "[LEASE_TEST] Sanity check failed. File size does not match with the original file size." << endl
                << "    file name: " << filename_re << endl
                << "    current file size: " << file_info.st_size << endl
                << "    original file size: " << file_size_bytes << endl;

        // Set buffer to original value.
	buf_orig = new char[file_size_bytes];
        set_buffer(buf_orig);
	// for (unsigned long i = 0; i < file_size_bytes; i++)
		// buf_orig[i] = '0' + (i % 5);

        // Read file and validate data.
	if ((fd = open(filename_re, O_RDWR)) < 0)
		err(1, "open");

        // Init read buffer.
	for(unsigned long i = 0; i < file_size_bytes; i++)
		buf[i] = 1;

        dump_file = false;
        for (i = 0; i < file_size_bytes ; i += io_size) {
            if (i + io_size > file_size_bytes)
                io_size = file_size_bytes - i;
            else
                io_size = io_size;

            ret = read(fd, buf + i, io_size);
            // Check read size.
            if (ret != io_size) {
                cout << "[LEASE_TEST] Sanity check failed. Read size mismatch: return "
                    << ret << ", request " << io_size << endl;
            }
            // Validate data.
            if (memcmp(buf + i, buf_orig + i, io_size) != 0) {
                cout << "[LEASE_TEST] Sanity check failed. A renamed file has incorrect data." << endl
                    << "    filename: " << filename_re << endl
                    << "    offset: " << i << endl
                    << "    current data:" << endl
                    << string(buf + i, io_size) << endl
                    << "    original data:" << endl
                    << string(buf_orig + i, io_size) << endl;

#if 0   /* Dump file content */
                dump_file = true;
#endif
            }
        }
        if (dump_file) {
            cout << "   whole file content:" << endl
                << "        current file:" << endl
                << buf << endl
                << "        original file:" << endl
                << buf_orig << endl;
        }
	close(fd);
        delete buf_orig;
	return ;
}

void io_bench::do_create_files(char *filename, char *filename_re)
{
    int fd;
    checkdir();

        //if ((ret = creat(filename, 0600)) < 0) {
        if ((fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0600)) < 0) {
                perror("open");
                exit(1);
        } else {
		// Do write.
		if (test_mode == 'w' || test_mode == 'n'){
			do_write(fd);
		}

#ifdef OUTPUT_TO_FILE
            *ofs_p << "[LEASE_TEST] ("<< id <<") create file " << filename << endl;
#else
            //cout << "[LEASE_TEST] ("<< id <<") create file " << filename << endl;
#endif
		// In Postfix, file is closed and fsynced in mail_copy() function.
		if (test_mode == 'w' || test_mode == 'n')
			fsync(fd);
		close(fd);

		// Do rename.
		if (test_mode == 'n'){
			// Start time stats.
			time_stats_start(&rename_stats);
			do_rename(filename, filename_re);
			time_stats_stop(&rename_stats);
		}

        }
}

void io_bench::do_rename(char *filename, char *filename_re)
{
	string src_name = string(filename);
	string target_name = string(filename_re);
#ifdef OUTPUT_TO_FILE
	*ofs_p << "[LEASE_TEST] <rename> src: " << src_name;
	*ofs_p << " -> target: " << target_name << endl;
#else
	//cout << "[LEASE_TEST] <rename> src: " << src_name;
	//cout << " -> target: " << target_name << endl;
#endif
	if (rename (filename, filename_re) < 0)
		cout << "[LEASE_TEST] (Error) rename failed. src: " << src_name << "-> target: " << target_name << endl;
}

void io_bench::Run(void)
{
	int ret;
        struct stat file_info;
#ifdef OUTPUT_TO_FILE
	*ofs_p << "[LEASE_TEST] thread " << id << " start - ";
	*ofs_p << "test mode " << test_mode << endl;
#else
	cout << "[LEASE_TEST] thread " << id << " start - ";
	cout << "test mode " << test_mode << endl;
#endif

	// Start time stats.
	time_stats_start(&stats);

	// Create files
	if (test_mode == 'c' || test_mode == 'w'){
		for (auto it : filenames)
			do_create_files((char *)it.c_str(), NULL);
	} else if (test_mode == 'n') {
		std::vector<string>::iterator re_it = filenames_re.begin();
		for (auto it : filenames){
			do_create_files((char *)it.c_str(), (char *)((*re_it).c_str()));
			re_it++;
                        //sleep(5);
		}
	}

	// Stop time stats.
	time_stats_stop(&stats);

#ifdef NOPRINT_STAT
	time_stats_print(&stats, (char *)"--------------- LEASE TEST time stats (Total) ----------------");
	printf("--------------------------------------------\n");

	time_stats_print(&rename_stats, (char *)"--------------- LEASE TEST time stats (Rename) ----------------");
	printf("--------------------------------------------\n");
#endif

	// Verify data. (Sanity check)
        if (do_verify) {
            cout << "[LEASE_TEST] Start sanity check. No fail message means it passed all the sanity checks." << endl;
            // Check no file remains in the source directory.
            for (auto it : filenames) {
                if (stat (it.c_str(), &file_info) == 0)
                    cout << "[LEASE_TEST] Sanity check failed. File still exists in the source directory: " << endl
                        << "    path: "
                        << it.c_str() << endl;
            }

            // Check all the generated files exist in the target directory and have valid data.
            for (auto it : filenames_re)
                do_sanity_check((char *)it.c_str());
        } 

#if 0
	int i = 0;
	for (auto it : filenames) {
		if (i % 3 == 0) {
			cout << "delete file " << it << endl;
			ret = unlink((char *)it.c_str());
		}
		i++;
	}

	i = 0;
	for (auto it : filenames) {
		if (i % 3 == 0) {
			cout << "create file " << it << endl;
			ret = creat(it.c_str(), 0600);
			do_write((char *)it.c_str());
		}
		i++;
	}
#endif

	delete buf;
	return;
}

unsigned long io_bench::str_to_size(char* str)
{
	/* magnitude is last character of size */
	char size_magnitude = str[strlen(str)-1];
	/* erase magnitude char */
	str[strlen(str)-1] = 0;

	unsigned long file_size_bytes = strtoull(str, NULL, 0);
	switch(size_magnitude) {
		case 'g':
		case 'G':
			file_size_bytes *= 1024;
		case 'm':
		case 'M':
			file_size_bytes *= 1024;
		case '\0':
		case 'k':
		case 'K':
			file_size_bytes *= 1024;
			break;
		case 'p':
		case 'P':
			file_size_bytes *= 4;
			break;
		default:
			std::cout << "[LEASE_TEST] incorrect size format " << str << endl;
			break;
	}
	return file_size_bytes;
}

#define HEXDUMP_COLS 8
void io_bench::hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	for(i = 0; i < len + ((len % HEXDUMP_COLS) ?
				(HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++) {
		/* print offset */
		if(i % HEXDUMP_COLS == 0) {
			printf("0x%06x: ", i);
		}

		/* print hex data */
		if(i < len) {
			printf("%02x ", 0xFF & ((char*)mem)[i]);
		} else {/* end of block, just aligning for ASCII dump */
			printf("	");
		}

		/* print ASCII dump */
		if(i % HEXDUMP_COLS == (HEXDUMP_COLS - 1)) {
			for(j = i - (HEXDUMP_COLS - 1); j <= i; j++) {
				if(j >= len) { /* end of block, not really printing */
					printf(" ");
				} else if(isprint(((char*)mem)[j])) { /* printable char */
					printf("%c",(0xFF & ((char*)mem)[j]));
				} else {/* other char */
					printf(".");
				}
			}
			printf("\n");
		}
	}
}

void io_bench::show_usage(const char *prog)
{
	std::cerr << "usage:" << endl
		<< prog << " <c/w> <# of file> <# of thread> [target dir name]" << endl
		<< prog << " <n> <# of file> <# of thread> <src dir name> <target dir name> [v]" << endl
		<< prog << " <s>" << endl
		<< prog << " <r> <absolute path of file to read>" << endl << endl
		<< " c: only create files." << endl
		<< " w: create and write." << endl
		<< " n: create, write and rename(move to other dir)." << endl
		<< " r: read a file. (Not supported)" << endl
		<< " s: reset shm value to 0. (Used for sync of <c/w/n> tests in multiprocess situation.)" << endl
                << " v: Do sanity check."
		<< endl;
}

int main(int argc, char *argv[])
{
	int n_threads, i;
	std::vector<io_bench *> io_workers;
	//unsigned long file_size_bytes;
	unsigned long file_size_bytes = FILE_SIZE;
        //unsigned int io_size = 16;
        unsigned int io_size = IO_SIZE_K;
	int ret, n_files;
	struct timeval t_start,t_end,t_elap;
	struct stat f_stat;
        string out_file_path = "output.lease_test";
	int fd, res;
	int *shm_i;
        char input;
	io_bench* ib;
        bool do_verify = false;

	if (argc < 2) {
		io_bench::show_usage(argv[0]);
		exit(-1);
	} else if (argv[1][0] == 'r') {
		if (argc != 3){
			io_bench::show_usage(argv[0]);
			exit(-1);
		}
		n_files = 1;
		n_threads = 1;
	} else if (argv[1][0] == 's') {
		n_files = 0;
		n_threads = 0;
	} else if (argv[1][0] == 'n'){
		if (argc == 6){
                    ;
                    // Ok. Without sanity check.
		} else if (argc == 7) {
                    if (argv[6][0] != 'v'){
			io_bench::show_usage(argv[0]);
			exit(-1);
                    }
                    do_verify = true;
                } else {
                    io_bench::show_usage(argv[0]);
                    exit(-1);
                }
		n_files = std::stoi(argv[2]);
		n_threads = std::stoi(argv[3]);
	} else {
		if (argc < 4) {
			io_bench::show_usage(argv[0]);
			exit(-1);
		}
		n_files = std::stoi(argv[2]);
		n_threads = std::stoi(argv[3]);
	}

	//file_size_bytes = 1048576;    // 1M
        //file_size_bytes = 4096;       // 4K
	//file_size_bytes = 0;       // 0
        std::ofstream ofs;
#ifdef OUTPUT_TO_FILE
	ofs.open(out_file_path.data(), ios_base::out);
#endif

	if (argv[1][0] != 'r'){
		// Get shared memory.
		shm_i = (int*)create_shm(fd, res);
		//printf ("fd:%d, res:%d, address of shm_i:%p\n", fd, res, shm_i);
	}

	if (argv[1][0] == 's') {
#ifdef OUTPUT_TO_FILE
		ofs << "[LEASE_TEST] Start lease test processes. By set shm_i = 0. Current shm_i:"<< *shm_i <<")" << std::endl;
#else
		std::cout << "[LEASE_TEST] Start lease test processes. By set shm_i = 0. Current shm_i:"<< *shm_i <<")" << std::endl;
#endif
		*shm_i = 0;
#ifdef OUTPUT_TO_FILE
		ofs << "[LEASE_TEST] Changed value: shm_i:"<< *shm_i <<")" << std::endl;
#else
		std::cout << "[LEASE_TEST] Changed value: shm_i:"<< *shm_i <<")" << std::endl;
#endif

	} else {
		// Init Assise.
#ifdef MLFS_ENABLE
		init_fs();
#endif
#ifdef OUTPUT_TO_FILE
		ofs <<  "[LEASE_TEST] init_fs() done." << std::endl
		ofs << "[LEASE_TEST] Total file size: " << file_size_bytes << " B" << std::endl
			<< "[LEASE_TEST] io size: " << io_size << " KB" << std::endl
			<< "[LEASE_TEST] # of thread: " << n_threads << std::endl;
#else
		std::cout << "[LEASE_TEST] Total file size: " << file_size_bytes << " B" << std::endl
			<< "[LEASE_TEST] io size: " << io_size << " KB" << std::endl
			<< "[LEASE_TEST] # of thread: " << n_threads << std::endl;
#endif

		bool is_second = false;
		if (argv[1][1] != NULL && argv[1][1] == 'd'){
			is_second = true;
		}

		if (argv[1][0] == 'r'){
			ib = new io_bench(0, 
						//file_size_bytes << 10,
						file_size_bytes,
						io_size << 10,
						n_files,
						*argv[1],
						false,
						NULL,
						NULL,
                                                false,
						ofs);
			std::string fn(argv[2]);
			ib->filenames.push_back(fn);	// read file name.
			io_workers.push_back(ib);
		} else {
			for (i = 0; i < n_threads; i++) {
				io_workers.push_back(new io_bench(i, 
							//file_size_bytes << 10,
							file_size_bytes,
							io_size << 10,
							n_files,
							*argv[1],
							is_second,
							argv[4],
							argv[5],
                                                        do_verify,
							ofs));
			}
		}

		for (auto it : io_workers) 
			it->prepare();

#ifdef OUTPUT_TO_FILE
		ofs.flush();
#endif

		if (argv[1][0] != 'r'){
			*shm_i += 1;
			//std::cout << "[LEASE_TEST] Input 'r' to start lease_test (pid: " << getpid() << "):" << std::endl;
			std::cout << "[LEASE_TEST] waiting lease_test to start (pid: " <<
                            getpid() << " shm_i:"<< *shm_i <<"). Run ./lease_test s in another terminal." << std::endl;

			// busy waiting.
			while (*shm_i > 0){
				usleep(100);
			}
		}

		//pause(); // waiting signal.
		//while (input != 'r'){
		//    input = cin.get();
		//}

		for (auto it : io_workers) 
			it->Start();

		for (auto it : io_workers) 
			it->Join();
	}

	fflush(stdout);
	fflush(stderr);
#ifdef OUTPUT_TO_FILE
        ofs.flush();
        ofs.close();
#endif

#ifdef MLFS_ENABLE
	if (argv[1][0] != 's') {
		//std::cout << "[LEASE_TEST] Input 'd' to digest(shutdown_fs()):" << std::endl;
		//while (input != 'd'){
		//    input = cin.get();
		//}

                // For digestion test.
                // std::cout << "[LEASE_TEST] waiting before digestion. ./lease_test s" << std::endl;
                // while (*shm_i > 0){
                        // usleep(100);
                // }
                make_digest_request_sync(100);

		// Exit Assise.
		//shutdown_fs();	// Uncommented because of concurrent shutdown_fs() issue.
		//std::cout << "[LEASE_TEST] shutdown_fs() done." << std::endl;
	}
#endif

	//destroy_shm((void *)shm_i);

        //std::cout << "[LEASE_TEST] Input 'q' to quit lease_test:" << std::endl;
        //while (input != 'q'){
        //    input = cin.get();
        //}

        std::cout << "[LEASE_TEST] Done. pid:" << std::to_string(getpid()) << std::endl;
	return 0;
}
