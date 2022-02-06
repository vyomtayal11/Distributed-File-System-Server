#ifndef __MFS_h__
#define __MFS_h__

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

// other definitons 
#define total_inodes (4096)
#define inode

// number of directiry entries 14, i have taken

#define MFS_BLOCK_SIZE   (4096)

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} MFS_Stat_t;

typedef struct __MFS_DirEnt_t{
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  inum;      // inode number of entry (-1 means entry not used)
} MFS_DirEnt_t;



typedef struct __inode_t {
    int size;
    int type;
    int dpointer[14];
} inode_t;

typedef struct __MFS_imap_t {
    int inodmap[16];
} MFS_imap_t;

typedef struct __MFS_CheckpointReg_t {
    int disk_pointer;
    int inodemap[256];    // 4096 ( total number of inodes) / 16 (imap piece size)
} MFS_CheckpointReg_t;


typedef struct __directoryData_t {
    MFS_DirEnt_t dirfiles[14];
} directoryData_t;


typedef struct __MSG_t {
    // this is actually an union of all the 
    // input fields in the MFS function.
    int inum;    // inode number 
    int type;    // type of file : file or directory 
    //char *name;   // name as in file name or directory name : 28 bytes max 
    char name[28];
    char buffer[4096]; // data buffer : 4096 bytes 
    int block;
    MFS_Stat_t statinfo;
    int req_type; // type of request or indirectly what functiSon to call : need to decide encoding , 
                  // assuming below order in zero indez now 
} MSG_t;

// '''
// Req-type 
// int MFS_init(int , char* ); --- 0
// int MFS_lookup(int, char* );   -- 1
// int MFS_stat(int , MFS_Stat_t* );   -- 2
// int MFS_write(int , char* , int );   -- 3
// int MFS_read(int , char* , int );   --- 4
// int MFS_creat(int , int , char* );   --- 5 
// int MFS_unlink(int , char* );   --- 6 
// int MFS_shutdown();  --- 7
// '''








int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int block);
int MFS_Read(int inum, char *buffer, int block);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

#endif // __MFS_h__