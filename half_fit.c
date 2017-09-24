#include "half_fit.h"
#include <lpc17xx.h>
#include <stdio.h>
#include <stdbool.h>
#include <limits.h>
#include "uart.h"
#include "type.h"
#include <string.h>
#define MAX_SIZE 32768


unsigned char array[MAX_SIZE] __attribute__ ((section(".ARM.__at_0x10000000"), zero_init));
int bit_vector[11];
//consider removing the +1 to size when checking, this doesnt really matter, and may lead to many corner cases
//Block Header
typedef struct memory_header{ 
	unsigned int prev:10;
	unsigned int next:10; 
	unsigned int size:10; 
	unsigned int alloc: 1;
	unsigned int unused: 1;
} mem_header_t; 

//Bucket Header
typedef struct bucket_header { 
	
	unsigned int memory_header:32;
	unsigned int prev:10; 
//	unsigned int unused1:6;
	unsigned int next:10;
//	unsigned int unused2:6;
} bkt_header_t;
//*CHANGE* this array is now an address array which holds the address of the first bucket element
mem_header_t* bucket_addresses[11];  
mem_header_t* base_address = (mem_header_t *) (array);

//Bit shifting function
U16 get_index(void *ptr) {
	return ((U32) ptr >> 5);
}

//Bit shifting function
mem_header_t* get_address_mem(U16 address) { 
	return (mem_header_t *)(address << 5);
}
bkt_header_t* get_address_bkt(U16 address) { 
	return (bkt_header_t *)(address << 5);
}

void remove_from_bucket(mem_header_t* block) { 
	//head
	bkt_header_t* next_temp_bkt_header;
	bkt_header_t* curr_bkt_header = (bkt_header_t*) (&block);
	bkt_header_t* prev_temp_bkt_header;
	int index = -1;
	//if the previous current bucket points to itself
	if (curr_bkt_header->prev == get_index(&curr_bkt_header)) { 
		while(2<<index <= block->size+1) { 
			index++;
		}
		index++;
		//check if next current bucket points to something not itself, then replace the bucket address
		if(curr_bkt_header->next != get_index(&curr_bkt_header)) { 
			next_temp_bkt_header = get_address_bkt(curr_bkt_header->next);
			next_temp_bkt_header->prev = get_index(&next_temp_bkt_header);
			bucket_addresses[index] = (mem_header_t*) (next_temp_bkt_header);
		}
		else { 
			bit_vector[index] = 0;
		}
		 
	}
	//tail
	//if the next current bucket header poinst to itself
	else if(curr_bkt_header->next == get_index(&block)) { 
		prev_temp_bkt_header = get_address_bkt(curr_bkt_header->prev) ;
		prev_temp_bkt_header->next = get_index(&prev_temp_bkt_header); 
	}
	//general case
	else { 
		prev_temp_bkt_header = get_address_bkt(curr_bkt_header->prev) ;
		next_temp_bkt_header = get_address_bkt(curr_bkt_header->next) ;
		prev_temp_bkt_header->next = get_index(&next_temp_bkt_header);
		next_temp_bkt_header->prev = get_index(&prev_temp_bkt_header); 
	}
}
//remove the blocks that are to be coalesced from their respective buckets 
//return a larger block 
mem_header_t* merge_blocks(mem_header_t* middle_block, mem_header_t* left_block,mem_header_t* right_block) { 
	remove_from_bucket(middle_block);
	
	//if both left and right blocks are unallocated, remove blocks
	if (left_block != NULL && !left_block->alloc && right_block != NULL && !right_block->alloc) { 		
		remove_from_bucket(right_block); 
		remove_from_bucket(left_block); 
		
		left_block->size += right_block->size + middle_block->size +2;
		
		//check if merging block on right is tail
		if(right_block->next == get_index(&right_block)) { 
			left_block->next = get_index(&left_block);
		}
		else {
			left_block->next = right_block->next;
		}			
		return left_block;
	}
	//if left block is unallocated, remove blcok
	else if (left_block != NULL && !left_block->alloc) { 	
		remove_from_bucket(left_block);
		
		//check if merging block on right is tail
		if(middle_block->next == get_index(&middle_block)) { 
			left_block->next = get_index(&left_block);
		}
		else {
			left_block->next = middle_block->next; 
		}
		left_block->size +=  middle_block->size+1;
		return left_block; 
	}
	//if right block is unallocated, remove block
	else if(right_block != NULL && !right_block->alloc) { 
		remove_from_bucket(right_block);
		
		//check if merging block on right is tail
		if(right_block->next == get_index(&right_block)) { 
			middle_block->next = get_index(&middle_block);
		}
		else {
			middle_block->next = right_block->next; 
		}
		middle_block->size +=  right_block->size+1;
		return middle_block;
	}
	return middle_block;
} 


void  half_init(void){
	int i;
	mem_header_t* first_block = (mem_header_t*) (array); 	
	bkt_header_t* first_bucket_element = (bkt_header_t*) (array);
	first_block->prev = get_index((array));  
	first_block->next = get_index((array));  
	first_block->size = 1023;  
	first_block->alloc = 0;
	first_bucket_element->next = get_index(array) ; 
	first_bucket_element->prev = get_index(array);
	bucket_addresses[10] = (mem_header_t*) (array);
	for(i =0;i<10;i++) { 
		bit_vector[i] = 0;
	}
	bit_vector[10] = 1;
}
mem_header_t* dummyfunction1(mem_header_t* block) { 
	return block; 
}	
bkt_header_t* dummyfunction2(bkt_header_t* block) { 
	return block; 
}	
void *half_alloc(unsigned int size){
	
	mem_header_t* block_header= NULL;
	bkt_header_t* bucket_header = NULL; 
	mem_header_t* new_block;
	bkt_header_t* temp_bkt_element; 
	bkt_header_t* new_bkt_element;
	int chunk_size = (size+4)/32 + 1; //added a plus one here for interger conv issues 
	int index = -1;
	
	if(chunk_size > 1024) { 
			return NULL;
	}
	//compare the lower bound of the bucket with the requested chunk size  
	while(2<<index <= chunk_size && !bit_vector[index+1] && index+1 <= 10) { 
		index++;
	}
	index++;
	block_header = bucket_addresses[index];
	bucket_header = (bkt_header_t*) (bucket_addresses[index]);
	dummyfunction1(block_header);
	dummyfunction2(bucket_header);
	if((block_header->size+1 - chunk_size) == 0) { 
			//case I: no need for block seperation && only one block in the bucket
			if(bucket_header->next == get_index(&bucket_header) && bucket_header->prev == get_index(&bucket_header)) {
				block_header->alloc = 1;
				bit_vector[index] = 0;
				return(void*)(bucket_addresses[index] +4);
			}
			//case II: no need for block seperation &&  more than one block in the bucket
			else { 
				block_header->alloc = 1;
				temp_bkt_element = get_address_bkt(bucket_header->next); 
				temp_bkt_element->prev = bucket_header->next; //new head
				bucket_addresses[index] = (mem_header_t*)(temp_bkt_element);
				return (void*)(bucket_addresses[index] +4);
			}
	}
	else { 
		//change the next/prev pointers and add the appropraite block header to the new block
		new_block = (mem_header_t*)(&block_header + chunk_size*32); 
		new_bkt_element = (bkt_header_t*)((&block_header + chunk_size*32)); 			
		new_block->size = (block_header->size - chunk_size);
		new_block->next = block_header->next;
		new_block->prev = get_index(&block_header); 
		block_header->next = get_index(&block_header + chunk_size*32);
		block_header->size = chunk_size-1;
		block_header->alloc = 1;
		//locate which bucket to place the new block
		index--;
		while(new_block->size+1 < 2<<index ) { 
			index--;
		}
		index++;
		//check if the new block is the first block in the bucket
		if(!bit_vector[index]) { 
			bit_vector[index] = 1; 
			new_bkt_element->next = get_index(&new_bkt_element); 
			new_bkt_element->prev = get_index(&new_bkt_element);
			bucket_addresses[index] = (mem_header_t*) (new_bkt_element); 
		}
		else { 
			//if not the first block in the bucket simply add it as the first element in the linkedlist
				temp_bkt_element =  (bkt_header_t*)(bucket_addresses[index]); 
				new_bkt_element->next = get_index(&temp_bkt_element);
				temp_bkt_element->prev = get_index(&new_bkt_element);
				new_bkt_element->prev = get_index(&new_bkt_element);
				bucket_addresses[index] = (mem_header_t*) (new_bkt_element);
		}
		return (void*)(bucket_addresses[index] +4);
	}	
}
void  half_free(void * address) {
	mem_header_t* temp_left_block_header = NULL;
	mem_header_t* temp_right_block_header = NULL;
	bkt_header_t* new_bkt_element;
	bkt_header_t* temp_bkt_element;
	int index =-1;
	mem_header_t* block_header = (mem_header_t*) (&address - 4); 
	//case I if the user is trying to deallocate a max sized block
	if(block_header->next == get_index(&block_header) && block_header->prev == get_index(&block_header)) { 
		block_header->size = MAX_SIZE/32-1;
		bucket_addresses[10] = block_header;
		block_header->alloc = 0;
		block_header->next = get_index(&block_header); 
		block_header->prev = get_index(&block_header);
		bit_vector[10] = 1;
		return;
	}	
	//if the user is trying to deallocate the tail block
	else if(block_header->next == get_index(&block_header)) {
		temp_left_block_header = get_address_mem(block_header->prev); 
	}
	//if the user is trying to deallocate the head block
	else if(block_header->prev == get_index(&block_header)) { 
		temp_right_block_header = get_address_mem(block_header->next); 
	}
	//middle block dealloc
	else { 
			temp_left_block_header = get_address_mem(block_header->prev); 
			temp_right_block_header = get_address_mem(block_header->next); 
	}
	
	//determining what pointers to pass to merge_blocks func
	block_header = merge_blocks(block_header,temp_left_block_header,temp_right_block_header);
	
	while(block_header->size+1 >= 2<<index) { 
			index++;
	}
	index++;
	new_bkt_element = (bkt_header_t*)(&block_header+4);
	if(!bit_vector[index]) { 
			bit_vector[index] = 1; 
			new_bkt_element->next = get_index(&new_bkt_element); 
			new_bkt_element->prev = get_index(&new_bkt_element);
			bucket_addresses[index] = (mem_header_t*) (new_bkt_element); 
		}
		else { 
			//if not the first block in the bucket simply add it as the first element in the linkedlist
				new_bkt_element->prev = get_index(&new_bkt_element); 
				temp_bkt_element =  (bkt_header_t*)(bucket_addresses[index]); 
				new_bkt_element->next = get_index(&temp_bkt_element);
				temp_bkt_element->prev = get_index(&new_bkt_element); 
				bucket_addresses[index] = (mem_header_t*) (new_bkt_element);
		}
}



