#include <threads/malloc.h>
#include <stdio.h>
#include <string.h>
#include "filesys/buffer_cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"

/*buffer cache entry의 개수 (32kb*/
#define BUFFER_CACHE_ENTRY_NB 64
/*buffer cache 메모리 영역을 가리킴*/
void *p_buffer_cache;
/*buffer head 배열*/
struct buffer_head head_buffer[BUFFER_CACHE_ENTRY_NB];
/*victim entry 선정 시 clock 알고리즘을 위한 변수*/
int clock_hand;

/*Buffer cache에서 데이터를 읽어 유저 buffer에 저장*/
bool bc_read (block_sector_t sector_idx, void *buffer, off_t bytes_read, int chunk_size, int sector_ofs)
{
    /* sector_idx를 buffer_head에서 검색 (bc_lookup 함수 이용) */
    struct buffer_head *sector_head = bc_lookup(sector_idx);
    /* 검색 결과가 없을 경우, 디스크 블록을 캐싱 할 buffer entry의
    buffer_head를 구함 (bc_select_victim 함수 이용) */
    if(sector_head == NULL){
		sector_head = bc_select_victim();
		sector_head->sector = sector_idx;
		sector_head->is_used = true;
        /* block_read 함수를 이용해, 디스크 블록 데이터를 buffer cache
        로 read */
		block_read(fs_device, sector_idx, sector_head->data);
	}
    lock_acquire(&sector_head->buffer_lock);
    /* buffer_head의 clock bit을 setting */
    sector_head->clock_bit = true;
    /* memcpy 함수를 통해, buffer에 디스크 블록 데이터를 복사 */
    memcpy(buffer + bytes_read, sector_head->data + sector_ofs, chunk_size);
	lock_release(&sector_head->buffer_lock);
    return true;
}

bool bc_write (block_sector_t sector_idx, void *buffer, off_t bytes_written, int chunk_size, int sector_ofs)
{
    bool success = false;
    /* sector_idx를 buffer_head에서 검색하여 buffer에 복사(구현)*/
    struct buffer_head *sector_head = bc_lookup(sector_idx);
	if(sector_head == NULL){
		sector_head = bc_select_victim();
		sector_head->sector  = sector_idx;
		sector_head->is_used = true;
		block_read(fs_device, sector_idx, sector_head->data);
	}
    lock_acquire(&sector_head->buffer_lock);
    /* update buffer head (구현) */
	sector_head->clock_bit = true;
	sector_head->dirty = true;
    memcpy(sector_head->data + sector_ofs, buffer + bytes_written, chunk_size);
	lock_release(&sector_head->buffer_lock);
    success = true;
    return success;
}

void bc_init(void) {
    /* Allocation buffer cache in Memory */
    /* p_buffer_cache가 buffer cache 영역 포인팅 */
    p_buffer_cache = malloc(SECTOR_SIZE * BUFFER_CACHE_ENTRY_NB);
    /* 전역변수 buffer_head 자료구조 초기화 */
    int i = 0;
	for(i = 0; i < BUFFER_CACHE_ENTRY_NB; i++){
		head_buffer[i].dirty = false;
		head_buffer[i].is_used = false;
		head_buffer[i].clock_bit = false;
		head_buffer[i].data = p_buffer_cache + SECTOR_SIZE * i;
		lock_init(&head_buffer[i].buffer_lock);
	}
    return;
}

void bc_term(void)
{
    /* bc_flush_all_entries 함수를 호출하여 모든 buffer cache
    entry를 디스크로 flush */
    bc_flush_all_entries();
    /* buffer cache 영역 할당 해제 */
    free(p_buffer_cache);
    return;
} 

struct buffer_head* bc_select_victim (void) {
    /* clock 알고리즘을 사용하여 victim entry를 선택 */
    /* buffer_head 전역변수를 순회하며 clock_bit 변수를 검사 */
    while(true)
	{
		clock_hand = (clock_hand + 1) % BUFFER_CACHE_ENTRY_NB;
		/* select the victim */
		if(head_buffer[clock_hand].is_used == false ||
           head_buffer[clock_hand].clock_bit == false)
		{
			/* 선택된 victim entry가 dirty일 경우, 디스크로 flush */
			if(head_buffer[clock_hand].dirty == true)
				bc_flush_entry(&head_buffer[clock_hand]);

			/* victim entry에 해당하는 buffer_head 값 update */
            head_buffer[clock_hand].is_used = false;
			head_buffer[clock_hand].sector = -1;
			head_buffer[clock_hand].clock_bit = false;

			/* victim entry를 return */
			return &head_buffer[clock_hand];
		}
		head_buffer[clock_hand].clock_bit = false;
	}
}

struct buffer_head* bc_lookup (block_sector_t sector){
    /* buffe_head를 순회하며, 전달받은 sector 값과 동일한
    sector 값을 갖는 buffer cache entry가 있는지 확인 */
    int i = 0;
    for(i = 0; i < BUFFER_CACHE_ENTRY_NB; i++){
        if(head_buffer[i].sector == sector && head_buffer[i].is_used)
            return &head_buffer[i];
    }
    /* 성공 : 찾은 buffer_head 반환, 실패 : NULL */
    return NULL;
} 

void bc_flush_entry (struct buffer_head *p_flush_entry)
{
    /* block_write을 호출하여, 인자로 전달받은 buffer cache entry
    의 데이터를 디스크로 flush */
    block_write(fs_device, p_flush_entry->sector, p_flush_entry->data);
    /* buffer_head의 dirty 값 update */
    p_flush_entry->dirty = false;
    return;
}

void bc_flush_all_entries (void){
    /* 전역변수 buffer_head를 순회하며, dirty인 entry는
    block_write 함수를 호출하여 디스크로 flush */
    int i = 0;
    for(i = 0; i < BUFFER_CACHE_ENTRY_NB; i++){
        if(head_buffer[i].dirty && head_buffer[i].is_used)
            /* 디스크로 flush한 후, buffer_head의 dirty 값 update */
            bc_flush_entry(&head_buffer[i]);
    }
    return;
}