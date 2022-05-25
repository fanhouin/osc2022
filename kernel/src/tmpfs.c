#include <tmpfs.h>
#include <malloc.h>
#include <string.h>
#include <uart.h>
#include <string.h>

struct file_operations* tmpfs_file_ops;
struct vnode_operations* tmpfs_vnode_ops;

int tmpfs_setup_mount(FileSystem *fs, Mount *mount){
    mount->fs = fs;
    mount->root_dentry = tmpfs_create_dentry("/", NULL, D_DIR); 
    return 0;
}

Dentry *tmpfs_create_dentry(const char *name, Dentry *parent, enum dentry_type type){
    Dentry *new_dentry = (Dentry *)kmalloc(sizeof(Dentry));
    new_dentry->name = (char *)kmalloc(sizeof(char) * strlen(name));
    INIT_LIST_HEAD(&new_dentry->list);
    INIT_LIST_HEAD(&new_dentry->childs);
    strcpy(new_dentry->name, name);
    new_dentry->parent = parent;
    if(new_dentry->parent != NULL){
        /* add to parent's child list */
        list_add_tail(&new_dentry->list, &parent->childs); 
        // uart_puts("[*] Added to parent's child list\n");
    }
    new_dentry->type = type;
    new_dentry->mount = NULL;
    new_dentry->vnode = tmpfs_create_vnode(new_dentry);
    return new_dentry;
}

VNode *tmpfs_create_vnode(Dentry *dentry){
    VNode *new_vnode = (VNode *)kmalloc(sizeof(VNode));
    new_vnode->dentry = dentry;
    new_vnode->v_ops = tmpfs_vnode_ops;
    new_vnode->f_ops = tmpfs_file_ops;
    new_vnode->internal = NULL;
    return new_vnode;
}

void tmpfs_set_ops(){
    tmpfs_file_ops = (struct file_operations *)kmalloc(sizeof(struct file_operations));
    tmpfs_vnode_ops = (struct vnode_operations *)kmalloc(sizeof(struct vnode_operations));

    tmpfs_file_ops->write = tmpfs_write;
    tmpfs_file_ops->read = tmpfs_read;
    tmpfs_file_ops->open = tmpfs_open;
    tmpfs_file_ops->close = tmpfs_close;
    tmpfs_file_ops->lseek64 = tmpfs_lseek64;

    tmpfs_vnode_ops->lookup = tmpfs_lookup;
    tmpfs_vnode_ops->create = tmpfs_create;
    tmpfs_vnode_ops->mkdir = tmpfs_mkdir;
}

int tmpfs_write(struct file* file, const void* buf, size_t len){
    TmpfsInode *inode_head = (TmpfsInode *)file->vnode->internal;
    char *src = (char *)buf;
    size_t write_len = len;
    size_t write_idx = 0;


    struct list_head *pos;
    list_for_each(pos, &inode_head->list){
        TmpfsInode *block = (TmpfsInode *)pos;
        if(file->f_pos >= block->idx * MAX_DATA_LEN){
            continue;
        }

        while(1){
            /* f_pos is in the block of some offset */
            size_t offset = MAX_DATA_LEN - (block->idx * MAX_DATA_LEN - file->f_pos);
            if(block->data[offset] == (char)EOF) return -1;

            if(write_len <= MAX_DATA_LEN - offset){
                memcpy(block->data + offset, src + write_idx, write_len);
                file->f_pos += write_len;
                write_idx += write_len;
                block->data[write_idx] = EOF;

                block->size += write_len;
                goto DONE;
            }
            else if(write_len > MAX_DATA_LEN - offset){
                memcpy(block->data + offset, src + write_idx, MAX_DATA_LEN - offset);
                file->f_pos += MAX_DATA_LEN - offset;
                write_idx += MAX_DATA_LEN - offset;
                write_len -= MAX_DATA_LEN - offset;
                block->size = MAX_DATA_LEN;

                /* add a new block */
                TmpfsInode *new_block = (TmpfsInode *)kmalloc(sizeof(TmpfsInode));
                new_block->data = (char *)kmalloc(sizeof(char) * MAX_DATA_LEN);
                new_block->idx = block->idx + 1;
                new_block->size = 0;
                new_block->vnode = inode_head->vnode;
                list_add_tail(&new_block->list, &inode_head->list);

                uart_puts("[*] Tmpfs Write: Create new block\n");
                /* next round will write the data into the new block */
                block = new_block;
            }
        }
    }

DONE:
    return write_idx;
}
int tmpfs_read(struct file* file, void* buf, size_t len){
    TmpfsInode *inode_head = (TmpfsInode *)file->vnode->internal;
    char *dest = (char *)buf;
    size_t read_len = len;
    size_t read_idx = 0;

    /* iterate through all data inodes */
    struct list_head *pos;
    list_for_each(pos, &inode_head->list){
        TmpfsInode *block = (TmpfsInode *)pos;
        /* first need to iterate to the right block beacuse of the f_pos */
        if(file->f_pos >= block->idx * MAX_DATA_LEN){
            continue;
        }
        /* f_pos is in the block of the offset */
        size_t offset = MAX_DATA_LEN - (block->idx * MAX_DATA_LEN - file->f_pos);
        if(block->data[offset] == (char)EOF) return -1;

        if(read_len <= block->size - offset){
            /* len < block size, read len is ok */
            for(size_t i = 0; i < read_len && block->data[offset + i] != (char)EOF; i++){
                dest[read_idx + i] = block->data[offset + i];
            }
            file->f_pos += read_len;
            goto DONE;
        }
        else if(read_len > block->size - offset){
            /* len >= block size, read MAX_DATA_LEN is ok */
            for(size_t i = 0; (i < (block->size - offset)) && (block->data[offset + i] != (char)EOF); i++){
                dest[read_idx + i] = block->data[offset + i];
            }
            file->f_pos += block->size - offset;
            read_idx += block->size - offset;
            read_len -= block->size - offset;
        }
    }

DONE:
    return read_idx;
}
int tmpfs_open(struct vnode* file_node, struct file** target){
    if(file_node == NULL){
        return -1;
    }
    File *new_file = (File *)kmalloc(sizeof(File));
    new_file->vnode = file_node;
    new_file->f_pos = 0;
    new_file->f_ops = file_node->f_ops;
    new_file->flags = 0;
    *target = new_file;
    return 0;
}
int tmpfs_close(struct file* file){
    uart_puts("[*] Closed file: ");
    uart_puts(file->vnode->dentry->name);
    uart_puts("\n");

    kfree(file);
    file = NULL;
    return 0;
}
long tmpfs_lseek64(struct file* file, long offset, int whence){

    return 0;
}


int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name){
    // if(*component_name == '\0'){
    //     /* if the target name is empty, it is end of the directory */
    //     return 0;
    // } 
    if(strcmp(component_name, ".") == 0){
        /* if the target name is ".", it is the same directory */
        *target = dir_node;
        return 0;
    }
    else if(strcmp(component_name, "..") == 0){
        /* if dentry is root path, return it */
        if(dir_node->dentry->parent == NULL) return 0;
        /* if dentry is not root path, find its parent */
        *target = dir_node->dentry->parent->vnode;
        return 0;
    }
    else{
        /* need to find the child dentry */
        struct list_head *pos;
        list_for_each(pos, &dir_node->dentry->childs){
            Dentry *tmp = (Dentry *)pos;
            if(strcmp(tmp->name, component_name) == 0){
                // TODO: need to check the dir is other filesystem

                /* if the target is a file/dir, return it */
                *target = tmp->vnode;
                return 0; 
            }
        }
    }
    /* if the target is not found, return error code */
    *target = NULL;
    return -1;
}

int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name){
    // uart_puts(component_name);
    // uart_puts(((Dentry *)(dir_node->dentry->childs.next))->name);
    /* create the dict info */
    Dentry *new_dentry = tmpfs_create_dentry(component_name, dir_node->dentry, D_FILE);
    /* create the inode list head */
    TmpfsInode *inode_head = (TmpfsInode *)kmalloc(sizeof(TmpfsInode));
    inode_head->idx = 0;
    inode_head->size = 0;
    inode_head->vnode = new_dentry->vnode;
    /* create the real data block */
    TmpfsInode *inode = (TmpfsInode *)kmalloc(sizeof(TmpfsInode));
    inode->data = (char *)kmalloc(sizeof(char) * MAX_DATA_LEN);
    inode->idx = 1;
    inode->size = 0;
    inode->vnode = new_dentry->vnode;

    new_dentry->vnode->internal = inode_head;

    /* add the new data block to inode_head */
    list_add_tail(&inode->list, &inode_head->list);

    *target = new_dentry->vnode;

    // print_string(UITOA, "idx = ", ((TmpfsInode *)(((TmpfsInode *)((*target)->internal))->list.next))->idx, 1);

    return 0;
}

int tmpfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name){
    /* create the dict info */
    Dentry *new_dentry = tmpfs_create_dentry(component_name, dir_node->dentry, D_DIR);
    *target = new_dentry->vnode;

    return 0;
}