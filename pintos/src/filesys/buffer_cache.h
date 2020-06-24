#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include "filesys/off_t.h"
#include "devices/block.h"
#include "threads/synch.h"
#include "filesys/inode.h"

#define SECTOR_SIZE 512
#define INDIRECT_BLOCK_ENTRIES 128
#define DIRECT_BLOCK_ENTRIES 123

struct buffer_head
{
    // 해당 entry가 dirty인지를 나타내는 flag
    bool dirty;
    // 해당 entry의 사용 여부를 나타내는 flag
    bool is_used; 
    // 해당 entry의 disk sector 주소
    block_sector_t sector;
    // clock algorithm을 위한 clock bit
    bool clock_bit;
    // lock 변수 (struct lock)
    struct lock buffer_lock;
    // buffer cache entry를 가리키기 위한 데이터 포인터
    void* data;
};

//void free_inode_sectors(struct inode_disk *inode_disk);
//block_sector_t byte_to_sector(const struct inode_disk *inode_disk, off_t pos);
//bool get_disk_inode(const struct inode *inode, struct inode_disk *inode_disk);
//bool inode_update_file_length(struct inode_disk *disk, off_t start_pos, off_t end_pos);
void bc_init (void);
void bc_flush_entry (struct buffer_head *p_flush_entry);
void bc_flush_all_entries(void);
struct buffer_head* bc_lookup(block_sector_t sector);
struct buffer_head* bc_select_victim(void);
void bc_term(void);
bool bc_read(block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunck_size, int sector_ofs);
bool bc_write(block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunck_size, int sector_ofs);
#endif