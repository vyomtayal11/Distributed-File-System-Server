# Distributed-File-System-Server


Purpose of the program
------------------------

The program is aimed at developing a working distributed file server.
The file server is built as a stand-alone UDP-based server. It waits for a message and then process the message as need be, 
replying to the given client. The file server will store all of its data in an on-disk file which will be referred to as 
the file system image. 
This image contains the on-disk representation of your data structures; used system calls to access it: open(), read(), write(), lseek(), 
close(), fsync(), pread(), pwrite().
To access the file server, we have a client library. The interface and structs that the library supports is defined in mfs.h and helper.h. 


Functions implemented : 
MFS_Init()
MFS_Lookup()
MFS_Stat()
MFS_Write()
MFS_Read()
MFS_Creat()
MFS_Unlink()
MFS_Shutdown()
