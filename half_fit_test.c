/*****************************************************************************
* This is the test file for MTE 241 Project 2 - Half Fit
* Modifications to this file should not be necessary
* Last Revised: September 28, 2016
****************************************************************************/

#include "half_fit.h"
#include "lpc17xx.h"
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include "uart.h"

#define smlst_blk         5
#define	smlst_blk_sz     (1 << smlst_blk)
#define	lrgst_blk        15
#define	lrgst_blk_sz     (1 << lrgst_blk)

// How many random blocks are allocated and removed. This random blocks are at most 2*RNDM_TESTS
#define RNDM_TESTS	100

//DO_PRINT allows or prevents the printf statements from being called. The elapsed time printf is the only statement that is run. 
#define DO_PRINT

/**-----------------------------------------------------------------Timer part--------------------------------------------------------*/
// How many ticks are passed
volatile uint32_t msTicks;
volatile uint8_t run_timer;

// SysTick interrupt Handler.
void SysTick_Handler( void )  {                               
	if ( run_timer ) {
		++msTicks;  
	}
}

__inline TimerInit(){

	msTicks = 0;
	run_timer = 0;
	/* Configure SysTick to generate an interrupt every millisecond */
	while ( SysTick_Config(SystemCoreClock / 1000) )  {                                   
		/* Check return code for errors */
	}
}

__inline TimerStart(){
	run_timer = 1;
}

__inline TimerStop(){
	run_timer = 0;
}

__inline current_elapsed_time(){
		return msTicks;
}

// The timer can be reset if it is already stopped
__inline TimerReset(){
	if ( run_timer == 0 ) {
		msTicks = 0;
	}
}

/**--------------------------------------------------------------End of Timer part--------------------------------------------------------*/

/*
take all allocated blocks and find any violations.
The violations are not revelaed on demand. all the allocated blocks are
stored in an array, then sorted. The sorted array is traversed
to find any block started before the previous one finished.
Such a simple algorithm does not need any complicated data structure and
is implemented using quick sort.
*/

typedef struct block {
	void *ptr;		// The actual pointer
	size_t len;		// The requested size
} block_t;

typedef struct pair {
	block_t first;
	block_t second;
} pair_t;


/*	Find the maximum possible allocating block
*/

size_t find_max_block( void ) {
	size_t i;
	void *p = NULL;

	for ( i = lrgst_blk_sz; i > 0; --i ) {
		p = half_alloc( i );

		if ( p != NULL ) {
			half_free( p );
			return i;
		}
	}

	return 0;
}

int cmpr_blks( const void * a, const void * b ) {

	return (char *)( ((block_t *) a)->ptr ) - (char *)( ((block_t *) b)->ptr );
}

bool is_violated( pair_t pair ) {
	bool rslt = !( pair.first.ptr == NULL
	           && pair.first.len == 0
	           && pair.second.ptr == NULL
	           && pair.second.len == 0 );

	if ( rslt ) {
		#ifdef DO_PRINT
			printf( "The %d Byte block starts at %d overlaps with the %d Byte block starts at %d\n",
			        pair.first.len, pair.first.ptr, pair.second.len, pair.second.ptr );
		#endif
	}

	return rslt;
}

pair_t find_violation( block_t * blk_arr, size_t len ) {
	pair_t result;
	uint32_t i;
	block_t empty_blk;
	
	qsort( blk_arr, len, sizeof(block_t), cmpr_blks );

	empty_blk.ptr = NULL;
	empty_blk.len = 0;

	result.first = empty_blk;
	result.second = empty_blk;

	for ( i = 0; i < len - 1; ++i) {
		if ( ((char *)blk_arr[i + 1].ptr - (char *)blk_arr[i].ptr) < blk_arr[i].len ) {
			// Violation happened
			result.first = blk_arr[i];
			result.second = blk_arr[i + 1];
			break;
		}
	}

	return result;
}

uint32_t log_2( uint32_t n ) {
	uint32_t b, m;
	
	if ( n <= 0 ) {
		#ifdef DO_PRINT
			printf( "log_2(0) is wrong\n" );
		#endif
	}

	b = sizeof( unsigned int ) * 8 - 1;

	m = 1 << b;

	while ( !(n & m) && (m != 0) ) {
		m >>= 1;
		b--;
	}

	return b;
}

// This function initializes the array
// through the half_init function
// then it starts by finding the maximum block size

bool test_max_alc( void ) {
	bool rslt = true;
	uint32_t blk_sz, max_blk_sz;
	
	half_init();

	blk_sz = find_max_block();

	#ifdef DO_PRINT
		printf( "blk_sz = %d", blk_sz );
	#endif

	max_blk_sz = 0x01 << lrgst_blk;

	if ( ((int)max_blk_sz - (int)blk_sz) / max_blk_sz > 1 ) {
		// The algorithm wasted more than 1% of memory.
		rslt = false;
	}
	
	return rslt;
}

// This test case tests the possibility of allocating the maximum
// block size
// This function uses the half_alloc function for the first time for the
// maximum block size
// It works for the second test case
bool test_alc_free_max( void ) {
	bool rslt = true;
	uint32_t blk_sz;
	void* ptr;

	half_init();
	blk_sz = find_max_block();
	ptr = half_alloc( blk_sz );

	if ( ptr == NULL ) {
		rslt = false;
	}
	
	return rslt;
}

// This function allocates several memry blocks of different sizes and then it
// it deallocates these blocks without committing to the same order
// by which they were allocated
bool test_static_alc_free( void ) {
	bool rslt = true;
	uint32_t max_sz;
	void *ptr_1, *ptr_2, *ptr_3, *ptr_4, *ptr_5, *ptr_6;
	
	half_init();

	max_sz = find_max_block();

	ptr_1 = half_alloc( (1 << 5) + 1 );

	if ( ptr_1 == NULL ) {
		return false;
	}

	ptr_2 = half_alloc( (1 << 9) - 1 );

	if ( ptr_2 == NULL ) {
		return false;
	}

	ptr_3 = half_alloc( (1 << 5) + 1 );

	if ( ptr_3 == NULL ) {
		return false;
	}

	ptr_4 = half_alloc( 1 << 10 );

	if ( ptr_4 == NULL ) {
		return false;
	}

	ptr_5 = half_alloc( 12345 );

	if ( ptr_5 == NULL ) {
		return false;
	}

	half_free(ptr_1);

	ptr_6 = half_alloc( 1 );

	if ( ptr_6 == NULL ) {
		return false;
	}

	half_free( ptr_3 );

	half_free( ptr_4 );

	ptr_1 = half_alloc( 1 << 9 );

	if ( ptr_1 == NULL ) {
		return false;
	}

	half_free( ptr_6 );
	half_free( ptr_1 );
	half_free( ptr_2 );
	half_free( ptr_5 );

	// Check wether all allocated memory blocks are freed.
	ptr_1 = half_alloc( max_sz );

	if ( ptr_1 == NULL ) {
		rslt = false;

		#ifdef DO_PRINT
			printf( "Memory is defraged.\n" );
		#endif
	} else {
		half_free( ptr_1 );
	}

	return rslt;
}
//
void alloc_blk_in_arr( block_t* blks, size_t *blks_sz, size_t len ) {

	blks[*blks_sz].ptr = half_alloc( len );
	blks[*blks_sz].len = len;

	if ( blks[*blks_sz].ptr != NULL ) {
		(*blks_sz)++;
	}
}

// This function makes sure the allocation process and the deallocation process are not violated through the algorithm
// it changes the block size back and forth
// This test case is related to the previous one and usually when one of them fails the other fails as well
bool test_static_alc_free_violation( void ) {
	bool rslt = true;
	size_t max_sz, blks_sz;
	block_t blks[5];
	void* ptr_1;
	
	half_init();
	max_sz = find_max_block();

	blks_sz = 0;

	alloc_blk_in_arr( blks, &blks_sz, (1 << 5) + 1 );
	alloc_blk_in_arr( blks, &blks_sz, (1 << 9) - 1 );
	alloc_blk_in_arr( blks, &blks_sz, (1 << 5) + 1 );
	alloc_blk_in_arr( blks, &blks_sz, (1 << 10)    );
	alloc_blk_in_arr( blks, &blks_sz, 12345        );

	if ( blks_sz == 0 ) {
		#ifdef DO_PRINT
			printf( "Failure on allocating any memory block. The memory access violation is irrelevant.\n" );
		#endif

		return false;
	}

	// Checking any violation
	if ( is_violated( find_violation( blks, blks_sz ) ) ) {
		return false;
	}

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	alloc_blk_in_arr(blks, &blks_sz, (1 << 9));

	// Checking any violation
	if ( is_violated(find_violation(blks, blks_sz)) ) {
		return false;
	}

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	--blks_sz;
	half_free(blks[blks_sz].ptr);

	// Check wether all allocated memory blocks are freed.
	ptr_1 = half_alloc(max_sz);

	if ( ptr_1 == NULL ) {
		rslt = false;

		#ifdef DO_PRINT
			printf( "Memory is defraged.\n" );
		#endif
	} else {
		half_free( ptr_1 );
	}

	return rslt;
}


size_t get_random_block_size( void ) {
	// Generate the random numbe in the range of [2^0, 2^(lrgst_blk-smlst_blk) )
	size_t r_num = rand() % ((1 << (lrgst_blk - smlst_blk)) - 1) + 1;

	r_num = (lrgst_blk - 1) - log_2(r_num);
	r_num = (rand() % (1 << r_num)) + (1 << r_num) + 1;
	return r_num;
}

// Previously we tested a static memory allocation and deallocation
// This test case generates random blovk sizes using the get_random_block_size( void ) function
// and proceed with the random memory allocation
// Furthermore, this function checks for the deallocation process as well as it checks for the
// existance of any violation in the memory allocation process
// It is noram that if the previous two cases fail, this one would fail as well
// Therefore, it is best to make sure that the simple static allocation works before debugging the fifth test case
bool test_rndm_alc_free( void ) {
	bool rslt = true;
	size_t line = 0, max_sz, blks_sz, alc_rec, tbf, blk_sz;
	int i;
	block_t blks[RNDM_TESTS << 1];
	block_t blk;
	void *ptr_1;

	blks_sz = 0;

	half_init();

	max_sz = find_max_block();
	
	// 'alc_rec' stores how many times 'half_alloc' successfully returns a requested block.
	alc_rec = 0;

	// Allocating random memory blocks
	for ( i = 0; i < RNDM_TESTS; ++i ) {

		// Making a new memory block and storing its pointer in the array 
		size_t blk_sz = get_random_block_size();
		block_t blk;
		blk.ptr = half_alloc(blk_sz);
		blk.len = blk_sz;

		if ( blk.ptr != 0 ) {
			blks[blks_sz] = blk;
			++blks_sz;
			alc_rec++;

			#ifdef DO_PRINT
				printf( "%i)The allocated %d Byte block starts from %d \n", ++line, blk.len, blk.ptr );
			#endif
		}
	}

	// Checking any violation
	if ( is_violated(find_violation(blks, blks_sz)) ) {
		return false;
	}
	
	// Free almost half of the allocation blocks
	for ( i = 0; i < RNDM_TESTS >> 1 ; ++i ) {
		if ( (rand() % 2) && (blks_sz > 0) ) {
			// Free a random block
			tbf = rand() % blks_sz;	// To be freed idex
			half_free(blks[tbf].ptr);

			#ifdef DO_PRINT
				printf( "%i)The %d Byte block starting from %d is free1\n", ++line, blks[tbf].len, blks[tbf].ptr );
			#endif

			--blks_sz;
			blks[tbf] = blks[blks_sz];

		} else {
			blk_sz = get_random_block_size();
			blk.ptr = half_alloc(blk_sz);
			blk.len = blk_sz;

			if ( blk.ptr != 0 ) {
				blks[blks_sz] = blk;
				++blks_sz;
				alc_rec++;

				#ifdef DO_PRINT
					printf( "%i)The allocated %d Byte block starts from %d \n", ++line, blk.len, blk.ptr );
				#endif
			}
		}
	}
	
	// Checking any violation
	if ( is_violated( find_violation( blks, blks_sz ) ) ) {
		return false;
	}


	for ( i = blks_sz - 1; i >= 0; --i ) {
		half_free(blks[i].ptr);
		--blks_sz;

		#ifdef DO_PRINT
			printf( "%i)The %d Byte block starting from %d is free2\n", ++line, blks[i].len, blks[i].ptr );
		#endif
	}

	// All allocated memories have to be freed now.

	#ifdef DO_PRINT
		printf("%d random blocks are allocated and freed without any violation.\n", alc_rec);
	#endif

	ptr_1 = half_alloc(max_sz);

	if ( ptr_1 == NULL ) {
		rslt = false;

		#ifdef DO_PRINT
			printf( "Memory is defraged.\n" );
		#endif
	} else {
		half_free( ptr_1 );
	}

	return rslt;
}
// This is the last test case and the goal is to allocation 1 byte at at time
bool test_max_alc_1_byte( void ) {
	bool rslt = true;
	uint32_t c = 0;
	size_t max_sz;
	
	half_init();

	max_sz = find_max_block();

	// Allocate 1 bytes until no half_aloc returns NULL

	while ( half_alloc( 1 ) != NULL ) {
		c++;
	}

	#ifdef DO_PRINT
		printf( "Only %d 1-Byte block can be allocated within %d addressable Bytes.\n", c, max_sz );
	#endif

	if ( c == 0 || !(max_sz >= smlst_blk_sz * c  || smlst_blk_sz * c >= lrgst_blk_sz) ) {
		#ifdef DO_PRINT
			printf( "32 * %d = %d is not equal to the maximum allocable block which is %d\n", c, c*32 , max_sz );
		#endif

		rslt = false;
	}

	return rslt;
}

bool test_max_alc_rand_byte( void ) {

	return false;
}

// The main function which contains the six test cases
// The timer starts at the first test case and continues until the last
// test case.
int main( void ) {
	SystemInit();
	SystemCoreClockUpdate();
	TimerInit();

	TimerStart(); {
		printf( "max_alc: %i\n",                   test_max_alc() );
		printf( "alc_free_max: %i\n",              test_alc_free_max() );
		printf( "static_alc_free: %i\n",           test_static_alc_free() );
		printf( "static_alc_free_violation: %i\n", test_static_alc_free_violation() );
		printf( "rndm_alc_free: %i\n",             test_rndm_alc_free() );
		printf( "max_alc_1_byte: %i\n",            test_max_alc_1_byte() );
	} TimerStop();
	
	printf( "The elappsed time:              %d ms\n", current_elapsed_time());
	
	while( 1 ) {
		// Infinite loop
	}
}
