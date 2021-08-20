

struct mlfs_rpc_write_io {
    int n;
    string data<>;
    int type;
};
typedef struct mlfs_rpc_write_io mlfs_write_io; 


 /*
  * The directory program definition
  */
 program MLFS_RPC { 
      version MLFS_RPC_VER { 
              int mlfs_rpc_write(mlfs_write_io) = 1;
         } = 1;
 } = 76; 
