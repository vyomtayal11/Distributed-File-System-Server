#include <stdio.h>
#include "udp.h"
#include "mfs.h"
#include <sys/stat.h>


#define BUFFER_SIZE (1000)

// global file descriptor 
int gfd = -1; 

MFS_CheckpointReg_t* checkpoint = NULL;

int Lookup(int pinum,char *name){

    // given valid pinum , now find tthe imap 
    // piece from checkpoint region
    int pimap = pinum/16;
    if(checkpoint->inodemap[pimap] == -1){
        printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    lseek(gfd,checkpoint->inodemap[pimap],SEEK_SET);
    //int imap_p[16];
    MFS_imap_t imap_p;
    read(gfd,&imap_p,sizeof(MFS_imap_t));
    // now find the index in imap piece 
    int index = pinum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now , read the parent inode 
    inode_t parent;
    lseek(gfd,imap_p.inodmap[index],SEEK_SET);
    read(gfd,&parent,sizeof(inode_t));
    // now you got the inode 
    // int parent_blocks = parent.size / MFS_BLOCK_SIZE + 1;

    if(parent.type != MFS_DIRECTORY){
        printf(" Parent is not a direcory ");
        return -1;
    } 

    // now we have to iterate through all parent datablocks
    // to check the mapping. 
    char tempbuff[MFS_BLOCK_SIZE];
    for(int i = 0;i<14 && parent.dpointer[i] != -1;i++){
        lseek(gfd,parent.dpointer[i],SEEK_SET);
        read(gfd,tempbuff,MFS_BLOCK_SIZE);

        directoryData_t *currDirContents = (directoryData_t*) tempbuff;
        // this will contain entries 
        // I have take max 14 directory contents only 
        for(int d = 0;d<14;d++){
            MFS_DirEnt_t* cdir = &currDirContents->dirfiles[d];
            if(strcmp(cdir->name,name) == 0){
                return cdir->inum;
            }
        }
    }
    return -1;
}


int Stat(int inum,MFS_Stat_t* m){

    // given valid pinum , now find tthe imap 
    // piece from checkpoint region
    int pimap = inum/16;
    if(checkpoint->inodemap[pimap] == -1){
        printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    lseek(gfd,checkpoint->inodemap[pimap],SEEK_SET);
    //int imap_p[16];
    MFS_imap_t imap_p;
    read(gfd,&imap_p,sizeof(MFS_imap_t));
    // now find the index in imap piece 
    int index = inum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now , read the parent inode 
    inode_t parent;
    lseek(gfd,imap_p.inodmap[index],SEEK_SET);
    read(gfd,&parent,sizeof(inode_t));
    // now you got the inode , read the contents and send 
    m->size = parent.size;
    m->type = parent.type;
    
    return 0; // on sucess 
}

int Read(int inum, char *buffer, int block){

    // both are valid 
    int pimap = inum/16;
    if(checkpoint->inodemap[pimap] == -1){
        printf(" Inavid pinum: imap piece not available");
        return -1;
    }

    // now read the inode piece
    lseek(gfd,checkpoint->inodemap[pimap],SEEK_SET);
    //int imap_p[16];
    MFS_imap_t imap_p;
    read(gfd,&imap_p,16*sizeof(int));
    // now find the index in imap piece 
    int index = inum % 16;
    // now yoou got the imap piece and index 
    // check in imap 
    if(imap_p.inodmap[index] == -1){
        printf(" Invalid pinum : imap index is -1 ");
        return -1;
    }

    // now you got the inode address
    // read the inode 
    inode_t curr;
    lseek(gfd,imap_p.inodmap[index],SEEK_SET);
    read(gfd,&curr,sizeof(inode_t));

    // you got the inode, now read the block in buffer
    lseek(gfd,curr.dpointer[block],SEEK_SET);
    read(gfd,buffer,MFS_BLOCK_SIZE);
    return 0;
}

int Write(int inum, char *buffer, int block){


    // this is the default point to add: end of log 
    int offset = checkpoint->disk_pointer;
    MFS_imap_t imap_p;

    int imap_piece = checkpoint->inodemap[inum/16];
    
    // here, we have started to check
    // if already exisiting conditions 
    int old_imap_piece = 0;
    int tinode;
    if(imap_piece != -1){
        // old imap piece exist 
        old_imap_piece = 1;

        // travel to inode 
        lseek(gfd,imap_piece,SEEK_SET);
        read(gfd,&imap_p,16*sizeof(int));
        tinode = imap_p.inodmap[inum%16];
    }

    // now, check for existing inode 
    // exisiting imap piece and exisitng inode 
    int old_inode = 0;
    inode_t curr;
    int data_block = -1;
    if(tinode != -1 && old_imap_piece == 1){
        old_inode = 1;
        lseek(gfd,tinode,SEEK_SET);
        read(gfd,&curr,sizeof(inode_t));

        // you cannot write to diretory ryt 
        if(curr.type != MFS_REGULAR_FILE){
            printf(" not a regular file");
            return -1;
        }
        // now check for data pointers block
        if(curr.dpointer[block] != -1){
            data_block = curr.dpointer[block];
        }
    }

    if(data_block != -1 && old_inode && old_imap_piece){
        // everything is pre-existing 
        offset = data_block;
    }

    // now we have checked the pre-existing conditions 
    // so, now we can write based on offset and 
    checkpoint->disk_pointer += MFS_BLOCK_SIZE;
    lseek(gfd,offset,SEEK_SET);

    // make the block  to write 
    char segment[4096];

    // CHANGE HERE : memcopy or something else for traversal 
    char* ib = buffer;
    for(int i = 0;i<4096;i++){
        if(ib != NULL){
            segment[i] = *ib;
            ib += 1;
        }
        else{
            segment[i] = '\0';
        }
    }

    write(gfd,segment,MFS_BLOCK_SIZE);
    inode_t newInode;
    // copy the old inode
    if(old_inode){
        newInode.type = curr.type;
        // change here too 
        // number of last byte
        if(curr.size > (block+1)*4096) 
            newInode.size = curr.size;
        else
            newInode.size = (block+1)*4096;

        // copy the pointers and block 
        for(int i  =0;i<14;i++){
            newInode.dpointer[i] = curr.dpointer[i];
            if(i == block)
                newInode.dpointer[i] = offset;
        }
    }
    else{
        newInode.type = MFS_REGULAR_FILE;
        newInode.size = 4096;
        // copy the pointers and block 
        for(int i  =0;i<14;i++){
            newInode.dpointer[i] = curr.dpointer[i];
            if(i == block)
                newInode.dpointer[i] = offset;
        }
    }

    // write the inode at the end of the log file 
    offset = checkpoint->disk_pointer;
    checkpoint->disk_pointer += sizeof(inode_t);
    lseek(gfd,offset,SEEK_SET);
    write(gfd,&newInode,sizeof(inode_t));

    // now we have to update the imap in checkpoint region 
    MFS_imap_t newimap;
    if(old_imap_piece){
        // copy the contents
        for(int i=0;i<16;i++){
            newimap.inodmap[i] = imap_p.inodmap[i];
            if(i == inum%16)
                newimap.inodmap[i] = offset; 
        }
    }
    else{
        for(int i=0;i<16;i++){
            newimap.inodmap[i] = -1;
            if(i == inum%16)
                newimap.inodmap[i] = offset; 
        }
    }

    // write the new imap 
    offset = checkpoint->disk_pointer;
    checkpoint->disk_pointer += sizeof(MFS_imap_t);
    lseek(gfd,offset,SEEK_SET);
    write(gfd,&newimap,sizeof(MFS_imap_t));

    // now update the checkpoint
    checkpoint->inodemap[inum/16] = offset;
    lseek(gfd,0,SEEK_SET);
    write(gfd,checkpoint,sizeof(checkpoint));

    fsync(gfd);
    return 0;
}

int create(int pinum, int type, char* name)
{

    // i,j , offset and oldmap initializations
    int i,j,k,l;

    //if(pinum < 0 || pinum >= TOTAL_NUM_INODES) {
    //perror("server_creat: invalid pinum_1");
    //return -1;

    int fp_mp = -1;

    //inode_t node;
    MFS_imap_t map;


    int len_name = 0;
    for(i=0; name[i] != '\0'; i++, len_name ++)
    {

    }
      /* if exists, creat is success */
    if(Lookup(pinum, name) != -1) 
    {
        return 0;
    }

    //control will come here if file doesn't already exist


    k = pinum / 16;
    fp_mp = checkpoint->inodemap[k];
    l = pinum % 16;

    MFS_imap_t imap_par;
    lseek(gfd, fp_mp, SEEK_SET);
    read(gfd, &imap_par, sizeof(MFS_imap_t));

    int fp_nd = imap_par.inodmap[l];
    
    inode_t node_par;
    lseek(gfd, fp_nd, SEEK_SET);
    read(gfd, &node_par, sizeof(inode_t));
  
    int free_inum = -1;
    int is_free_inum_found = 0;


    int offset;
    int step;
    for(i=0; i< ( 4096/16 ); i++) {
        
        fp_mp = checkpoint->inodemap[i];

        if(fp_mp != -1) 
        {
            MFS_imap_t imap_par;
            lseek(gfd, fp_mp, SEEK_SET);
            read(gfd, &imap_par, sizeof(MFS_imap_t));

            for(j=0; j<16; j++)
            {
                fp_nd = imap_par.inodmap[j];
                if(fp_nd == -1)
                {
                    free_inum = i*16 + j;
	                is_free_inum_found = 1;
	                break;
                }
            } 
        }
        else
        {
            MFS_imap_t map_new;

            for(j =0; j< 16;j++)
            {
                map_new.inodmap[j] = -1;
            }
            offset = checkpoint->disk_pointer;
            step = sizeof(MFS_imap_t);
            checkpoint->disk_pointer += step;
            lseek(gfd, offset, SEEK_SET);
            write(gfd, &map_new, step);

            // update cr, new imap and  inode are written, we update the imap table

            checkpoint->inodemap[i] = offset;
            lseek(gfd, 0, SEEK_SET);
            write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));

            fsync(gfd);

            for (j=0; j<16; j++)
            {
                fp_nd = map_new.inodmap[j];
                if(fp_nd == -1) 
                {
	                free_inum = i*16 + j;
	                is_free_inum_found = 1;
	                break;
	            }
            }
        }
        
        if (is_free_inum_found) 
        {
            break;
        }

    }


    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.
    // some commented code.


    //if(free_inum == -1 || free_inum > TOTAL_NUM_INODES - 1) 
    //{ 
        //gw: added free_inum upper limit check
        //perror("server_creat: cannot find free inode_5 ");
        //return -1;
    //}

    char data_buf[MFS_BLOCK_SIZE]; /* gw: use char buffer, (no need free) */
    directoryData_t* dir_buf = NULL;
    int flag_found_entry = 0;
    int block_par = 0;

    inode_t* par_nd = &node_par;

    int fp_block;
    for(i=0; i< 14; i++) 
    {   
        fp_block= par_nd->dpointer[i];
        block_par = i;

        if(fp_block == -1)
        {
            directoryData_t* par_dir = (directoryData_t*) data_buf;
            
            for(i=0; i< 64; i++)
            {
                strcpy(par_dir->dirfiles[i].name, "\0");
	            par_dir->dirfiles[i].inum = -1;
            }
        

            offset = checkpoint->disk_pointer;
            step = MFS_BLOCK_SIZE;
            checkpoint->disk_pointer += step;
            lseek(gfd, offset, SEEK_SET);
            write(gfd, par_dir, sizeof(directoryData_t)); 

            fp_block = offset;

            inode_t node_dir_new;
            node_dir_new.size = node_par.size;
            node_dir_new.type = MFS_DIRECTORY;

            for (int i = 0; i < 14; i++)
            {
                node_dir_new.dpointer[i] = node_par.dpointer[i];
            }

            node_dir_new.dpointer[block_par] = offset;
        
            par_nd = &node_dir_new;

            offset = checkpoint->disk_pointer;
            step = sizeof(inode_t);
            checkpoint->disk_pointer += step;
            lseek(gfd, offset, SEEK_SET);
            write(gfd, &node_dir_new, step);

            MFS_imap_t map_dir_new;	

            for(int i = 0; i< 16; i++) 
            {
                map_dir_new.inodmap[i] = imap_par.inodmap[i];
            }
            map_dir_new.inodmap[l] = offset; 

            offset = checkpoint->disk_pointer;
            step = sizeof(MFS_imap_t);
            checkpoint->disk_pointer += step;
            lseek(gfd, offset, SEEK_SET);
            write(gfd, &map_dir_new, step);

            //now the first imap and inode is written, we update imap table
            //this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic

            checkpoint->inodemap[k] = offset;
            lseek(gfd, 0, SEEK_SET);
            write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));

            fsync(gfd);
        }

        lseek(gfd, fp_block, SEEK_SET);
        read(gfd, data_buf, MFS_BLOCK_SIZE);

        dir_buf = (directoryData_t*)data_buf;
        for(j=0; j<64; j++)
        {
            MFS_DirEnt_t* p_de = &dir_buf->dirfiles[j];
            if(p_de->inum == -1) 
            {
	            p_de->inum = free_inum;
	            strcpy(p_de->name, name);
	            flag_found_entry = 1;
	            break;
            }
        }
        if(flag_found_entry)
        {
            break;
        }
    }

    //datablock dir_buf is ready now

    offset = checkpoint->disk_pointer;
    step = MFS_BLOCK_SIZE;
    checkpoint->disk_pointer += step;
    lseek(gfd, offset, SEEK_SET);
    write(gfd, dir_buf, sizeof(directoryData_t));

    inode_t node_par_new;
    node_par_new.size = par_nd->size;
    node_par_new.type = MFS_DIRECTORY;
    for (i = 0; i < 14; i++)
    {
        node_par_new.dpointer[i] = par_nd->dpointer[i];
    }  
    node_par_new.dpointer[block_par] = offset; // absolute offset.

    offset = checkpoint->disk_pointer;
    step = sizeof(inode_t);
    checkpoint->disk_pointer += step;
    lseek(gfd, offset, SEEK_SET);
    write(gfd, &node_par_new, step );

    MFS_imap_t map_par_new;
    for(i = 0; i< 16; i++) 
    {
        map_par_new.inodmap[i] = imap_par.inodmap[i]; //gw: dubious about num inodes per imap
    }
    map_par_new.inodmap[l] = offset;

    offset = checkpoint->disk_pointer;
    step = sizeof(MFS_imap_t);
    checkpoint->disk_pointer += step;
    lseek(gfd, offset, SEEK_SET);
    write(gfd, &map_par_new, step );

    //now the first imap and inode is written, we update imap table
    //this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic

    checkpoint->inodemap[k] = offset;
    lseek(gfd, 0, SEEK_SET);
    write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));	



    fsync(gfd);



    //now the parent file operation completes
    //using child inum = free_inum


    //make an empty block
    //generate clean buffer
    //char* ip = NULL;
    char wr_buffer[ MFS_BLOCK_SIZE ];
    for(i=0; i<MFS_BLOCK_SIZE; i++) 
    {
        wr_buffer[i] = '\0';
    }

    // pick an inode. use the first -1 nd
    // use free_inum found earlier

    int inum = free_inum;
    int is_old_map = 0;
    //int is_old_node = 0;
    //int is_old_block = 0;

    fp_mp= -1;
    fp_nd = -1;
    fp_block = -1;

    if(type == MFS_DIRECTORY)
    {
        directoryData_t* par_dir = (directoryData_t*) wr_buffer;

        for(i=0; i< 64; i++)
        {
            strcpy(par_dir->dirfiles[i].name, "\0");

            par_dir->dirfiles[i].inum = -1;
        }

        strcpy(par_dir->dirfiles[0].name, ".\0");
        par_dir->dirfiles[0].inum = inum; 
        strcpy(par_dir->dirfiles[1].name, "..\0");
        par_dir->dirfiles[1].inum = pinum; 


        // write the directory block
        offset = checkpoint->disk_pointer;
        step = MFS_BLOCK_SIZE;
        checkpoint->disk_pointer += step;
        lseek(gfd, offset, SEEK_SET);
        write(gfd, wr_buffer, step );
        
    }

    // set default offset if not old map



    //fs operations starts
    k = inum / 16; 
    fp_mp =  checkpoint->inodemap[k];
    if(fp_mp != -1)
    {
        is_old_map =1;

        l = inum % 16;

        lseek(gfd, fp_mp, SEEK_SET);
        read(gfd, &map, sizeof(MFS_imap_t));
        fp_nd = map.inodmap[l];
    }

    //gw: 0512: no init block0 if REG_FILE (note that directory block is handaled previously) */
    /* step = MFS_BLOCK_SIZE; /\* DirDataBlock should be rounded to 4096 *\/ */
    /* p_cr->end_log += step; */
    /* lseek(fd, offset, SEEK_SET); */
    /* write(fd, wr_buffer, MFS_BLOCK_SIZE); /\* write whole block *\/ */

    //new node

    inode_t node_new;
    node_new.size = 0;
    node_new.type = type;
    for (i = 0; i < 14; i++)
    {
        node_new.dpointer[i] = -1;
    }

    if(type == MFS_DIRECTORY)
    {
        node_new.dpointer[0] = offset;
    }

    offset = checkpoint->disk_pointer;
    step = sizeof(inode_t);
    checkpoint->disk_pointer += step;
    lseek(gfd, offset, SEEK_SET);
    write(gfd, &node_new, step );

    //make new map
    //update imap

    MFS_imap_t map_new;
    if(is_old_map) 
    {
        for(i = 0; i< 16; i++) 
        {
            map_new.inodmap[i] = map.inodmap[i] ;
        }
        map_new.inodmap[l] = offset; 	
    }
    else 
    {
        for(i = 0; i< 16; i++) 
        {
            map_new.inodmap[i] = -1 ; 
        }
        map_new.inodmap[l] = offset; 	/* fp_nd_new */
    }

    offset = checkpoint->disk_pointer;
    step = sizeof(MFS_imap_t);
    checkpoint->disk_pointer += step;
    lseek(gfd, offset, SEEK_SET);
    write(gfd, &map_new, step );

    // udpate cr
    // update imap table

    checkpoint->inodemap[k] = offset;
    lseek(gfd, 0, SEEK_SET);
    write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t) );

    fsync(gfd);
    return 0;


}

int Unlink(int pinum, char *name){

    // check for the directory using lookup function 
    int dir_inum = Lookup(pinum,name);
    if(dir_inum == -1){
        // directory name not present 
        // still return Success as per description
        return 0;
    }

    // checkpoint -> imap_piece
    int imap_p_loc = checkpoint->inodemap[dir_inum/16];
    MFS_imap_t imap_p;
    lseek(gfd,imap_p_loc,SEEK_SET);
    read(gfd,&imap_p,sizeof(MFS_imap_t));
    // check if it the only entry also
    int only_entry_in_imap_piece = 1;
    for(int i = 0;i<16;i++){
        if(i != (dir_inum%16) && imap_p.inodmap[i] != -1){
            only_entry_in_imap_piece = 0;
            break;
        }
    }

    // now we have the imap, now get the inode 
    inode_t fdir_inode;
    int fdir_inode_loc = imap_p.inodmap[dir_inum%16];
    
    lseek(gfd,fdir_inode_loc,SEEK_SET);
    read(gfd,&fdir_inode,sizeof(inode_t));

    // If we have to delete directory 
    // check whether it is empty directory or not 
    int empty_dir_check = 1;
    if(fdir_inode.type == MFS_DIRECTORY){
        directoryData_t* dir_content;
        for(int i =0;i<14;i++){
            if(fdir_inode.dpointer[i] != -1){
                // read each non-empty data block 
                // in one of the directoris, it will containd "." and ".."
                char data_block[4096];
                lseek(gfd,fdir_inode.dpointer[i],SEEK_SET);
                read(gfd,data_block,MFS_BLOCK_SIZE);

                dir_content = (directoryData_t*) data_block;
                // now we got the directory content
                for(int d  = 0;d<14;d++){
                    MFS_DirEnt_t* temp = &dir_content->dirfiles[d];
                    if(temp->inum == -1 || temp->inum == pinum || temp->inum == dir_inum)
                        continue;
                    else{
                        // not an empty directoey
                        empty_dir_check = 0;
                        break;
                    }
                }
                if(empty_dir_check == 0)
                    break;
            }    
        }
    }
    if(empty_dir_check == 0){
        printf(" non - Empty directory ");
        return -1;
    }

    // nnow , since it is non-empty
    // we have to remove all th e data pointers in it 
    for(int i =0;i<14;i++){
        fdir_inode.dpointer[i] = -1;
    }
    // now, we have to remove/unlink the imap
    MFS_imap_t new_imap_piece;
    if(only_entry_in_imap_piece == 0){
        // we may need not update the imap
        // just update the checkpoint region to -1
        // this should unlink the inode 
        checkpoint->inodemap[dir_inum/16] = -1;
    }
    else{
        //here , we have other directories/files in too imap piece
        // just make a new imap with file/dir component as -1
        for(int i = 0;i<16;i++){
            if(i == dir_inum%16){
                new_imap_piece.inodmap[i] = -1;
            }
            else{
                new_imap_piece.inodmap[i] = imap_p.inodmap[i];
            }
        }
        // copy the new imap piece at log end 
        int log_end = checkpoint->disk_pointer;
        lseek(gfd,log_end,SEEK_SET);
        write(gfd,&new_imap_piece,sizeof(MFS_imap_t));
        // update the checkpoint diskpointer
        checkpoint->disk_pointer += (int)sizeof(MFS_imap_t);
    }
    // write the updated checkpoint
    lseek(gfd,0,SEEK_SET);
    write(gfd,checkpoint,sizeof(MFS_CheckpointReg_t));
    fsync(gfd);

    // we also have to make some small update in parent node
    // parent data block update , make data entry as -1
    int imap_par_loc = checkpoint->inodemap[pinum/16];
    MFS_imap_t imap_par;
    lseek(gfd,imap_par_loc,SEEK_SET);
    read(gfd,&imap_par,sizeof(MFS_imap_t));

    // now we have the imap, now get the inode 
    inode_t pardir_inode;
    int pardir_inode_loc = imap_par.inodmap[pinum%16];
    
    lseek(gfd,pardir_inode_loc,SEEK_SET);
    read(gfd,&pardir_inode,sizeof(inode_t));

    //char par_data_block[4096];
    directoryData_t* par_dir_content;
    int found = 0;
    int dci = 0;
    while(dci < 14 && found == 0){
        if(pardir_inode.dpointer[dci] != -1){
            // read each non-empty data block 
            // in one of the directoris, it will containd "." and ".."
            char data_block[4096];
            lseek(gfd,pardir_inode.dpointer[dci],SEEK_SET);
            read(gfd,data_block,MFS_BLOCK_SIZE);
            par_dir_content = (directoryData_t*) data_block;
            // now we got the directory content
            for(int d  = 0;d<14;d++){
                MFS_DirEnt_t* temp = &par_dir_content->dirfiles[d];
                if(temp->inum == dir_inum){
                    strcpy(temp->name,"nullfile");
                    temp->inum = -1; 
                    found = 1;
                    break;
                }
            }
        }
        dci += 1;
    }
    // copy the data block to the end of log 
    int db_eol = checkpoint->disk_pointer;
    lseek(gfd,db_eol,SEEK_SET);
    write(gfd,par_dir_content,sizeof(directoryData_t));
    checkpoint->disk_pointer += MFS_BLOCK_SIZE;

    // now update the inode of the parent
    inode_t new_par_inode;
    new_par_inode.type = pardir_inode.type;
    // reduction of one block
    new_par_inode.size -= MFS_BLOCK_SIZE;
    if(new_par_inode.size < 0)
        new_par_inode.size = 0;
    for(int i =0;i<14;i++){
        new_par_inode.dpointer[i] = pardir_inode.dpointer[i];
        if(i == dci-1)
            new_par_inode.dpointer[i] = db_eol;
    }

    // write the new parent inode
    int new_par_inode_eol = checkpoint->disk_pointer;
    lseek(gfd,checkpoint->disk_pointer,SEEK_SET);
    write(gfd,&new_par_inode,sizeof(inode_t));
    checkpoint->disk_pointer += (int)sizeof(inode_t);

    // make a new imappiece containing updated 
    MFS_imap_t new_par_imap;
    for(int i=0;i<16;i++){
        if(i == pinum%16){
            new_par_imap.inodmap[i] = new_par_inode_eol;
        }
        else{
            new_par_imap.inodmap[i] = imap_par.inodmap[i];
        }
    }

    // copy to end 
    int new_par_imap_eol = checkpoint->disk_pointer;
    lseek(gfd,checkpoint->disk_pointer,SEEK_SET);
    write(gfd,&new_par_imap,sizeof(MFS_imap_t));
    checkpoint->disk_pointer += (int)sizeof(MFS_imap_t);

    checkpoint->inodemap[pinum/16] = new_par_imap_eol;
    lseek(gfd,0,SEEK_SET);
    write(gfd,checkpoint,sizeof(MFS_CheckpointReg_t));
    fsync(gfd);
    return 0;
}

int Shutdown(){
    fsync(gfd);
    exit(0);
}






int server_fs_initialize(int port, char *filesystem){

    gfd = open(filesystem, O_RDWR|O_CREAT, S_IRWXU);

    if(gfd < 0){
        perror("server_init: Cannot open file");
    }

    // Make a copy in memory
    struct stat fs;
    if(fstat(gfd,&fs) < 0) {
        perror("server_init: Cannot open file");
    }

    //Put image in memory
    int i;

    checkpoint = (MFS_CheckpointReg_t *)malloc(sizeof(MFS_CheckpointReg_t));
    //int sz = 0;
    int offset = 0, step = 0;

    if(fs.st_size < sizeof(MFS_CheckpointReg_t)){

        /* new file */
        gfd = open(filesystem, O_RDWR|O_CREAT|O_TRUNC, S_IRWXU);
        if(gfd <0)
            return -1;

        // Make a copy in memory
        if(fstat(gfd,&fs) < 0) {
            perror("server_init: fstat error");
            return -1;
        }
        
        //p_cr->inode_count = 0;
        checkpoint->disk_pointer = sizeof(MFS_CheckpointReg_t);
        for(i=0; i<256; i++)
        checkpoint->inodemap[i] = -1;

        /* write content on the file using lseek and write */
        lseek(gfd, 0, SEEK_SET);
        write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));

        /* create and write to block 0, directory */
        directoryData_t db;
        for(i=0; i< 14; i++){
        strcpy(db.dirfiles[i].name, "\0");
        db.dirfiles[i].inum = -1;
        }
        strcpy(db.dirfiles[0].name, ".\0");
        db.dirfiles[0].inum = 0; /* gw: correct */
        strcpy(db.dirfiles[1].name, "..\0");
        db.dirfiles[1].inum = 0;

        /* GW: how??? */
        offset = checkpoint->disk_pointer;
        step = MFS_BLOCK_SIZE; /* DirDataBlock should be rounded to 4096 */
        checkpoint->disk_pointer += step;
        lseek(gfd, offset, SEEK_SET);
        write(gfd, &db, sizeof(directoryData_t));

        inode_t nd;
        nd.size = 0; /* gw: tbc, use 0 for dir (prf) */
        nd.type = MFS_DIRECTORY;
        for (i = 0; i < 14; i++) nd.dpointer[i] = -1; /* 14 pointers per inode */
        nd.dpointer[0] = offset;			    /* gw: right, inode contains the actual address of datablock */

        offset = checkpoint->disk_pointer;
        step = sizeof(inode_t); /* inode size */
        checkpoint->disk_pointer += step;
        lseek(gfd, offset, SEEK_SET);
        write(gfd, &nd, step);


        MFS_imap_t mp;
        for(i = 0; i< 16; i++) mp.inodmap[i] = -1;
        mp.inodmap[0] = offset; /* gw: right, imap translate the inode number "0" to inode's address */

        offset = checkpoint->disk_pointer;
        step = sizeof(MFS_imap_t); /* inode size */
        checkpoint->disk_pointer += step;
        lseek(gfd, offset, SEEK_SET);
        write(gfd, &mp, step);

        /* now the first imap and inode is written, we update imap table */
        /* this update is done after the (imap, inode, datablock creation is donea altogether ) -> atomic */
        checkpoint->inodemap[0] = offset; /* gw: right, contains the address of imap piece "0" */
        lseek(gfd, 0, SEEK_SET);
        write(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));

        fsync(gfd);
    }
    else{
        lseek(gfd,0, SEEK_SET);
        read(gfd, checkpoint, sizeof(MFS_CheckpointReg_t));
    }

    int sd=-1;
    if((sd =   UDP_Open(port))< 0){
        perror("server_init: port open fail");
        return -1;
    }


    struct sockaddr_in s;
    MSG_t buf_pk,  rx_pk;

    while (1) {
        if( UDP_Read(sd, &s, (char *)&buf_pk, sizeof(MSG_t)) < 1)
        continue;

        // lookup
        if(buf_pk.req_type == 1){
        rx_pk.inum = Lookup(buf_pk.inum, buf_pk.name);
        rx_pk.req_type = 1;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 2){
        // stat
        rx_pk.inum = Stat(buf_pk.inum, &(rx_pk.statinfo));
        rx_pk.req_type = 2;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 3){
        // write 
        rx_pk.inum = Write(buf_pk.inum, buf_pk.buffer, buf_pk.block);
        rx_pk.req_type = 3;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 4){
        // read
        rx_pk.inum = Read(buf_pk.inum, rx_pk.buffer, buf_pk.block);
        rx_pk.req_type = 4;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 5){
        // creat
        rx_pk.inum = create(buf_pk.inum, buf_pk.type, buf_pk.name);
        rx_pk.req_type = 5;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 6){
        // unlink
        rx_pk.inum = Unlink(buf_pk.inum, buf_pk.name);
        rx_pk.req_type = 6;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));

        }
        else if(buf_pk.req_type == 7) {
        rx_pk.req_type = 7;
        UDP_Write(sd, &s, (char*)&rx_pk, sizeof(MSG_t));
        Shutdown();
        }
        else {
        perror("server_init: unknown request");
        return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]){
    if(argc != 3){
        perror("Usage: server <portnum> <image>\n");
        exit(1);
    }

    server_fs_initialize(atoi ( argv[1] ) ,argv[2] );
    return 0;
}

    


