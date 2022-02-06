#include <stdio.h>
#include <string.h>

#include "udp.h"
#include "mfs.h"

// making some global variable 
char *serverhostname;
int serverport;

//  List of functions to implement
int MFS_init(int , char* );
int MFS_lookup(int, char* );
int MFS_stat(int , MFS_Stat_t* );
int MFS_write(int , char* , int );
int MFS_read(int , char* , int );
int MFS_creat(int , int , char* );
int MFS_unlink(int , char* );
int MFS_shutdown();


int UDP_Send( char *serverhostname, int serverport,MSG_t *toSend, MSG_t *toRec)
{
    // https://www.mkssoftware.com/docs/man3/select.3.asp
    fd_set read_fds;
    FD_ZERO(&read_fds);

    int fd = UDP_Open(0);
    if(fd < -1)
    {
        printf(" Socket not opening ");
        return -1;
    }

    struct sockaddr_in addr;
    int socket_connect = UDP_FillSockAddr(&addr, serverhostname, serverport);
    if(socket_connect < 0)
    {
        printf(" FillScokAdress failed ");
        return -1;
    }

    struct timeval timeout;
    struct sockaddr_in addr_select;

    int rec = 0;
    int retries = 4;
    while(retries > 0 && rec==0){
        timeout.tv_sec=4;
        timeout.tv_usec=0;
        FD_ZERO(&read_fds);
        FD_SET(fd,&read_fds);
        UDP_Write(fd, &addr, (char*)toSend, sizeof(MSG_t));
        if(select(fd+1, &read_fds, NULL, NULL, &timeout))
        {
            int ret = UDP_Read(fd, &addr_select, (char*)toRec, sizeof(MSG_t));
            if(ret > 0)
            {
                // got resposnse
                UDP_Close(fd);
                rec = 1;
            }
        }
        retries -= 1;
    }
    if(rec == 1)
        return 0;
    return -1;
}


// Req-type 
// int MFS_init(int , char* ); --- 0
// int MFS_lookup(int, char* );   -- 1
// int MFS_stat(int , MFS_Stat_t* );   -- 2
// int MFS_write(int , char* , int );   -- 3
// int MFS_read(int , char* , int );   --- 4
// int MFS_creat(int , int , char* );   --- 5 
// int MFS_unlink(int , char* );   --- 6 
// int MFS_shutdown();  --- 7


int MFS_Init(char *hostname,int port){
    serverhostname = hostname;
    serverport = port;
    return 0;
}

// Request Type : 1
int MFS_Lookup(int pinum, char *name){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(pinum < 0 || pinum > 4096)
        return -1;
    if(name == NULL || strlen(name) > 28)
        return -1;
    
    MSG_t req;
    MSG_t response;

    // update the fields 
    req.inum = pinum;
    req.req_type = 1; 
    // PANKAJ 
    strcpy((char*)&(req.name), name);

    // send the message 
    int rc = UDP_Send(serverhostname,serverport,&req,&response);
    if(rc == -1)
        return rc;
    else
        return response.inum;
}

// Stat Type : 2 
int MFS_Stat(int inum, MFS_Stat_t *m){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(inum < 0 || inum > 4096)
        return -1;
    // just make a simple request now
    MSG_t req;
    MSG_t response;

    req.inum = inum;
    req.req_type = 2;

    int rc = UDP_Send(serverhostname,serverport,&req,&response);

    if(rc == -1){
        return rc;
    }
    else{
        m->size = response.statinfo.size;
        m->type = response.statinfo.type;
        return 0;
    }
}

// Write Type : 3 
int MFS_Write(int inum, char *buffer, int block){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(inum < 0 || inum > 4096)
        return -1;
    if(block < 0 || block >= 14)
        return -1;

    MSG_t req;
    MSG_t response;

    // update the fields
    req.inum = inum;
    req.block = block;
    req.req_type = 3;
    // write th buffer 
    for(int i =0;i<4096;i++){
        req.buffer[i] = buffer[i];
    }

    int rc = UDP_Send(serverhostname,serverport,&req,&response);

    if(rc == -1){
        return -1;
    }
    return 0;

}

// Read type : 4
int MFS_Read(int inum, char *buffer, int block){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(inum < 0 || inum > 4096)
        return -1;
    if(block < 0 || block >= 14)
        return -1;

    MSG_t req;
    MSG_t response;

    // update the fields
    req.inum = inum;
    req.block = block;
    req.req_type = 4;

    int rc = UDP_Send(serverhostname,serverport,&req,&response);

    if(rc == -1){
        return -1;
    }
    else{
        if(response.inum > -1) {
            for(int i = 0;i<4096;i++){
                buffer[i] = response.buffer[i];
            }
        }
        return response.inum;
    }
}

// create Type : 5 
int MFS_Creat(int pinum, int type, char *name){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(pinum < 0 || pinum > 4096)
        return -1;
    if(name == NULL || strlen(name) > 28)
        return -1;

    MSG_t req;
    MSG_t response;

    req.inum = pinum;
    req.type = type;
    strcpy(req.name,name);
    req.req_type = 5;

    int rc = UDP_Send(serverhostname,serverport,&req,&response);
    if(rc == -1)
        return -1;
    return response.inum;
}

// unlink type : 6 
int MFS_Unlink(int pinum, char *name){

    // DO BASIC CHECK HERE ONLY 
    // no need to send the request only 
    if(pinum < 0 || pinum > 4096)
        return -1;
    if(name == NULL || strlen(name) > 28)
        return -1;

    MSG_t req;
    MSG_t response;

    req.inum = pinum;
    req.req_type = 6;
    strcpy(req.name,name);
    
    int rc = UDP_Send(serverhostname,serverport,&req,&response);
    if(rc == -1)
        return -1;
    return response.inum;
}

// shutdown type : 7 
int MFS_Shutdown(){
    MSG_t req;
    MSG_t response;

    req.req_type = 7;
    int rc = UDP_Send(serverhostname,serverport,&req,&response);
    if(rc == -1)
        return -1;
    return 0;
}
