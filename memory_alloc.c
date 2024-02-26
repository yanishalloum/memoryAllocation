#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <setjmp.h>
#include "cmocka.h"
#include "memory_alloc.h"

struct memory_alloc_t m;
#define A_B INT32_MIN

/* Initialize the memory allocator */
void memory_init() {
  /* Initialize the blocks with values equal to index of the next available block */
  for (int i = 0; i < DEFAULT_SIZE - 1; i++) {
    m.blocks[i] = (int64_t)(i + 1);
  }
  m.blocks[DEFAULT_SIZE - 1] = NULL_BLOCK;

  /* Set the number of available blocks to default size */
  m.available_blocks = DEFAULT_SIZE;

  /* Set the index of the first available block to 0 */
  m.first_block = 0;

  /* Set the error number to E_SUCCESS */
  m.error_no = E_SUCCESS;
}

/* Return the number of consecutive blocks starting from {first} index */
int nb_consecutive_blocks(int first) {
  /* Check if {first} index is valid and if the {first} block value is valid */
  if (first < 0 || first > DEFAULT_SIZE) {
    return -1;
  }
  if (m.blocks[first] == A_B) {
    return 0;
  }
  
  int nbConsecutiveBlocks = 1;
  int currentBlock = first;

  /* Calculate the number of consecutive blocks */
  while(m.blocks[currentBlock] == currentBlock + 1) {
    nbConsecutiveBlocks += 1;
    currentBlock = m.blocks[currentBlock];
  }

  return nbConsecutiveBlocks;
}


/* Used in memory_reorder function to swap two elements */
void swap(int* a, int* b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/* Used to reorder blocks */
void memory_reorder() {
  int nbAvailableBlocks = m.available_blocks;
  int availableBlocks[nbAvailableBlocks - 1];

  int currentBlock = m.first_block;
  availableBlocks[0] = currentBlock;

  int idx = 1;
  while(m.blocks[currentBlock] != NULL_BLOCK && idx < nbAvailableBlocks) {
    availableBlocks[idx] = m.blocks[currentBlock];
    currentBlock = m.blocks[currentBlock];

    idx += 1;
  }

  /* Apply bubble sort to the available blocks */
  int i, j;
  int sorted;
  for (i = 0; i < nbAvailableBlocks - 1; i++) {
    sorted = 0;
    for (j = 0; j < nbAvailableBlocks - 1 -  i; j++) {
      if (availableBlocks[j] > availableBlocks[j + 1]) {
        swap(&availableBlocks[j], &availableBlocks[j + 1]);
        sorted = 1;
      }
    }
  }

  /* Redefine the order of the chained list */
  m.first_block = availableBlocks[0];
  currentBlock = m.first_block;
  int nextBlock = availableBlocks[1];

  idx = 0;
  while (idx < nbAvailableBlocks - 1) {
    m.blocks[currentBlock] = nextBlock;
    
    idx++;

    currentBlock = availableBlocks[idx];
    nextBlock = availableBlocks[idx + 1];
  }
  
  int lastBlock = availableBlocks[nbAvailableBlocks - 1];
  m.blocks[lastBlock] = NULL_BLOCK;
  
}


/* Allocate size bytes
 * return -1 in case of an error
 */
int memory_allocate(size_t size) {
  int startIndex = -1;

  /* Calculate the necessary number of blocks to stock {size} bytes */
  size_t blockSize = 8;
  int nbNecessaryBlocks = (size + blockSize - 1) / blockSize;
  printf("Necessary blocks: %d\n", nbNecessaryBlocks);

  if (nbNecessaryBlocks > DEFAULT_SIZE) {
    m.error_no = E_NOMEM;
    return -1; 
  }

  int currentBlock = m.first_block;
  int prevIndex = currentBlock;

  /* Iterate through the available blocks chained list */
  while (currentBlock != NULL_BLOCK) {

    /* Check if there are enough consecutive blocks starting at currentBlock to stock nbNecessaryBlocks */
    if (nb_consecutive_blocks(currentBlock) >= nbNecessaryBlocks) {
      startIndex = currentBlock;
      int endIndex = currentBlock + nbNecessaryBlocks - 1;

      /* Redefine the 1st block */
      if (((m.first_block >= startIndex) && (m.first_block <= endIndex)) ||
          ((m.first_block > endIndex) && (m.blocks[endIndex] < m.first_block))) {
            m.first_block = m.blocks[endIndex];
        }

      /* Substract the allocated number of blocks from available blocks */
      m.available_blocks -= nbNecessaryBlocks;
      
      m.error_no = E_SUCCESS;

      /* Bridge the gap left by the allocated blocks */
      m.blocks[prevIndex] = m.blocks[endIndex];
      initialize_buffer(startIndex, size);

      return startIndex;  

    } else {
      prevIndex = currentBlock;
      currentBlock = m.blocks[currentBlock];
    }
  }

  /* Not enoug consecutive blocks found, reorder and retry */
  memory_reorder();
  
  /* Try again after reordering */
    currentBlock = m.first_block;
    prevIndex = currentBlock;

    while (currentBlock != NULL_BLOCK) {
        if (nb_consecutive_blocks(currentBlock) >= nbNecessaryBlocks) {
            startIndex = currentBlock;
            int endIndex = currentBlock + nbNecessaryBlocks - 1;

            if (((m.first_block >= startIndex) && (m.first_block <= endIndex)) ||
                ((m.first_block > endIndex) && (m.blocks[endIndex] < m.first_block))) {
                m.first_block = m.blocks[endIndex];
            }

            m.available_blocks -= nbNecessaryBlocks;
            m.error_no = E_SUCCESS;

            m.blocks[prevIndex] = m.blocks[endIndex];
            initialize_buffer(startIndex, size);

            return startIndex;
        } else {
            prevIndex = currentBlock;
            currentBlock = m.blocks[currentBlock];
        }
    }

  m.error_no = E_SHOULD_PACK;  
  return startIndex;
}

/* Free the block of data starting at address */
void memory_free(int address, size_t size) {
  /* Calculate the number of blocks to free */
  size_t blockSize = 8;
  int nbBlocksToFree = (size + blockSize - 1) / blockSize;

  int startIndex = address;
  int endIndex = startIndex + nbBlocksToFree - 1;

  /* Add the freed blocks to the available blocks */
  for (int i = startIndex; i < endIndex; i++) {
    m.blocks[i] = i + 1;
  }
  m.blocks[endIndex] = m.first_block;

  m.first_block = startIndex;
  m.available_blocks += nbBlocksToFree;
}

/* Print information on the available blocks of the memory allocator */
void memory_print() {
  printf("---------------------------------\n");
  printf("\tBlock size: %lu\n", sizeof(m.blocks[0]));
  printf("\tAvailable blocks: %lu\n", m.available_blocks);
  printf("\tFirst free: %d\n", m.first_block);
  printf("\tStatus: "); memory_error_print(m.error_no);
  printf("\tContent:  ");

  int availableBlock = m.first_block;
  while (availableBlock != NULL_BLOCK) {
    printf("%d -> ", availableBlock);
    availableBlock = m.blocks[availableBlock];
  }
  printf("NULL_BLOCK\n");
  printf("---------------------------------\n");
}


/* print the message corresponding to error_number */
void memory_error_print(enum memory_errno error_number) {
  switch(error_number) {
  case E_SUCCESS:
    printf("Success\n");
    break;
  case E_NOMEM:
    printf("Not enough memory\n");
    break;
  case  E_SHOULD_PACK:
    printf("Not enough contiguous blocks\n");
    break;
  default:
    break;
  }
}

/* Initialize an allocated buffer with zeros */
void initialize_buffer(int start_index, size_t size) {
  char* ptr = (char*)&m.blocks[start_index];
  for(int i=0; i<size; i++) {
    ptr[i]=0;
  }
}






/*************************************************/
/*             Test functions                    */
/*************************************************/

// We define a constant to be stored in a block which is supposed to be allocated:
// The value of this constant is such as it is different form NULL_BLOCK *and*
// it guarantees a segmentation vioaltion in case the code does something like
// m.blocks[A_B]
#define A_B INT32_MIN

/* Initialize m with all allocated blocks. So there is no available block */
void init_m_with_all_allocated_blocks() {
  struct memory_alloc_t m_init = {
    // 0    1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
    {A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B, A_B},
    0,
    NULL_BLOCK,
    INT32_MIN // We initialize error_no with a value which we are sure that it cannot be set by the different memory_...() functions
  };
  m = m_init;
}

/* Test memory_init() */
void test_exo1_memory_init(){
  init_m_with_all_allocated_blocks();

  memory_init();

  // Check that m contains [0]->[1]->[2]->[3]->[4]->[5]->[6]->[7]->[8]->[9]->[10]->[11]->[12]->[13]->[14]->[15]->NULL_BLOCK
  assert_int_equal(m.first_block, 0);

  assert_int_equal(m.blocks[0], 1);
  assert_int_equal(m.blocks[1], 2);
  assert_int_equal(m.blocks[2], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 6);
  assert_int_equal(m.blocks[6], 7);
  assert_int_equal(m.blocks[7], 8);
  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 10);
  assert_int_equal(m.blocks[10], 11);
  assert_int_equal(m.blocks[11], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], 15);
  assert_int_equal(m.blocks[15], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, DEFAULT_SIZE);

  // We do not care about value of m.error_no
}

/* Initialize m with some allocated blocks. The 10 available blocks are: [8]->[9]->[3]->[4]->[5]->[12]->[13]->[14]->[11]->[1]->NULL_BLOCK */
void init_m_with_some_allocated_blocks() {
  struct memory_alloc_t m_init = {
    // 0           1    2    3    4    5    6    7    8    9   10   11   12   13   14   15
    {A_B, NULL_BLOCK, A_B,   4,   5,  12, A_B, A_B,   9,   3, A_B,   1,  13,  14,  11, A_B},
    10,
    8,
    INT32_MIN // We initialize error_no with a value which we are sure that it cannot be set by the different memory_...() functions
  };
  m = m_init;
}

/* Test nb_consecutive_block() at the beginning of the available blocks list */
void test_exo1_nb_consecutive_blocks_at_beginning_linked_list(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(nb_consecutive_blocks(8), 2);
}

/* Test nb_consecutive_block() at the middle of the available blocks list */
void test_exo1_nb_consecutive_blocks_at_middle_linked_list(){
  init_m_with_some_allocated_blocks();
  memory_print(); // Ligne ajoutee
  assert_int_equal(nb_consecutive_blocks(3), 3);
}

/* Test nb_consecutive_block() at the end of the available blocks list */
void test_exo1_nb_consecutive_blocks_at_end_linked_list(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(nb_consecutive_blocks(1), 1);
}

/* Test memory_allocate() when the blocks allocated are at the beginning of the linked list */
void test_exo1_memory_allocate_beginning_linked_list(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(memory_allocate(16), 8);
  
  // We check that m contains [3]->[4]->[5]->[12]->[13]->[14]->[11]->[1]->NULL_BLOCK 
  assert_int_equal(m.first_block, 3);

  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], 11);
  assert_int_equal(m.blocks[11], 1);
  assert_int_equal(m.blocks[1], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, 8);

  assert_int_equal(m.error_no, E_SUCCESS);
}

/* Test memory_allocate() when the blocks allocated are at in the middle of the linked list */
void test_exo1_memory_allocate_middle_linked_list(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(memory_allocate(17), 3);
  
  // We check that m contains [8]->[9]->[12]->[13]->[14]->[11]->[1]->NULL_BLOCK 
  assert_int_equal(m.first_block, 8);

  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], 11);
  assert_int_equal(m.blocks[11], 1);
  assert_int_equal(m.blocks[1], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, 7);

  assert_int_equal(m.error_no, E_SUCCESS);
}

/* Test memory_allocate() when we ask for too many blocks ==> We get -1 return code and m.error_no is M_NOMEM */
void test_exo1_memory_allocate_too_many_blocks(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(memory_allocate(256), -1);
  
  // We check that m does not change and still contains: [8]->[9]->[3]->[4]->[5]->[12]->[13]->[14]->[11]->[1]->NULL_BLOCK
  assert_int_equal(m.first_block, 8);

  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], 11);
  assert_int_equal(m.blocks[11], 1);
  assert_int_equal(m.blocks[1], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, 10);

  assert_int_equal(m.error_no, E_NOMEM);
}

/* Test memory_free() */
void test_exo1_memory_free(){
  init_m_with_some_allocated_blocks();

  memory_free(6, 9);
  
  // We check that m contains: [6]->[7]->[8]->[9]->[3]->[4]->[5]->[12]->[13]->[14]->[11]->[1]->NULL_BLOCK
  assert_int_equal(m.first_block, 6);

  assert_int_equal(m.blocks[6], 7);
  assert_int_equal(m.blocks[7], 8);
  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], 11);
  assert_int_equal(m.blocks[11], 1);
  assert_int_equal(m.blocks[1], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, 12);

  // We do not care about value of m.error_no
}

/* Test memory_reorder() */
void test_exo2_memory_reorder(){
  init_m_with_some_allocated_blocks();

  memory_reorder();
  
  // We check that m contains: [1]->[3]->[4]->[5]->[8]->[9]->[11]->[12]->[13]->[14]->NULL_BLOCK
  assert_int_equal(m.first_block, 1);

  assert_int_equal(m.blocks[1], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 8);
  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 11);
  assert_int_equal(m.blocks[11], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], NULL_BLOCK);  
  assert_int_equal(m.available_blocks, 10);

  // We do not care about value of m.error_no
}

/* Test memory_reorder() leading to successful memory_allocate() because we find enough consecutive bytes */
void test_exo2_memory_reorder_leading_to_successful_memory_allocate(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(memory_allocate(32), 11);
  // We check that m contains: [1]->[3]->[4]->[5]->[8]->[9]->NULL_BLOCK
  assert_int_equal(m.first_block, 1);

  assert_int_equal(m.blocks[1], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 8);
  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], NULL_BLOCK);
  
  assert_int_equal(m.available_blocks, 6);

  assert_int_equal(m.error_no, E_SUCCESS);
}

/* Test memory_reorder() leading to failed memory_allocate() because not enough consecutive bytes */
void test_exo2_memory_reorder_leading_to_failed_memory_allocate(){
  init_m_with_some_allocated_blocks();

  assert_int_equal(memory_allocate(56), -1);

  // We check that m contains: [1]->[3]->[4]->[5]->[8]->[9]->[11]->[12]->[13]->[14]->NULL_BLOCK
  assert_int_equal(m.first_block, 1);

  assert_int_equal(m.blocks[1], 3);
  assert_int_equal(m.blocks[3], 4);
  assert_int_equal(m.blocks[4], 5);
  assert_int_equal(m.blocks[5], 8);
  assert_int_equal(m.blocks[8], 9);
  assert_int_equal(m.blocks[9], 11);
  assert_int_equal(m.blocks[11], 12);
  assert_int_equal(m.blocks[12], 13);
  assert_int_equal(m.blocks[13], 14);
  assert_int_equal(m.blocks[14], NULL_BLOCK);  
  assert_int_equal(m.available_blocks, 10);

  assert_int_equal(m.available_blocks, 10);

  assert_int_equal(m.error_no, E_SHOULD_PACK);
}

int main(int argc, char**argv) {

  const struct CMUnitTest tests[] = {
    
    /* a few tests for exercise 1.
     *
     * If you implemented correctly the functions, all these tests should be successfull
     * Of course this test suite may not cover all the tricky cases, and you are free to add
     * your own tests.
     */
    cmocka_unit_test(test_exo1_memory_init),
    cmocka_unit_test(test_exo1_nb_consecutive_blocks_at_beginning_linked_list),
    cmocka_unit_test(test_exo1_nb_consecutive_blocks_at_middle_linked_list),
    cmocka_unit_test(test_exo1_nb_consecutive_blocks_at_end_linked_list),
    cmocka_unit_test(test_exo1_memory_allocate_beginning_linked_list),
    cmocka_unit_test(test_exo1_memory_allocate_middle_linked_list),
    cmocka_unit_test(test_exo1_memory_allocate_too_many_blocks),
    cmocka_unit_test(test_exo1_memory_free),

    /* Run a few tests for exercise 2.
     *
     * If you implemented correctly the functions, all these tests should be successfull
     * Of course this test suite may not cover all the tricky cases, and you are free to add
     * your own tests.
     */

    cmocka_unit_test(test_exo2_memory_reorder),
    cmocka_unit_test(test_exo2_memory_reorder_leading_to_successful_memory_allocate),
    cmocka_unit_test(test_exo2_memory_reorder_leading_to_failed_memory_allocate)
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
