/*
All the main functions with respect to the MeMS are inplemented here
read the function discription for more details

NOTE: DO NOT CHANGE THE NAME OR SIGNATURE OF FUNCTIONS ALREADY PROVIDED
you are only allowed to implement the functions 
you can also make additional helper functions a you wish

REFER DOCUMENTATION FOR MORE DETAILS ON FUNSTIONS AND THEIR FUNCTIONALITY
*/
// add other headers as required
#include<stdio.h>
#include<stdlib.h>
#include <sys/mman.h>
#include<stdbool.h>



/*
Use this macro where ever you need PAGE_SIZE.
As PAGESIZE can differ system to system we should have flexibility to modify this 
macro to make the output of all system same and conduct a fair evaluation. 
*/
#define PAGE_SIZE 4096
#define PROCESS true
#define HOLE false


/*
Initializing the structure of the block of the Main Linked List and the Sub Linked List.
The Type in SubBlock.
false - Hole
true - proccess
*/
struct MemBlock {
    size_t parentSize;
    struct MemBlock* next;
    struct SubBlock* child;
    struct MemBlock* prev;
    void* PAD;
};

struct SubBlock {
    size_t size;
    bool type;
    struct SubBlock* next;
    struct SubBlock* prev;
};

#define blockSize sizeof(struct MemBlock)
#define subSize sizeof(struct SubBlock)

struct MemBlock* freeHead;
int virtualStart;
int* startHead;
int* current;

/*
Initializes all the required parameters for the MeMS system. The main parameters to be initialized are:
1. the head of the free list i.e. the pointer that points to the head of the free list
2. the starting MeMS virtual address from which the heap in our MeMS virtual address space will start.
3. any other global variable that you want for the MeMS implementation can be initialized here.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_init(){
    freeHead = NULL;
    virtualStart = 1000;
    startHead = (int *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    current = startHead;
}


/*
This function will be called at the end of the MeMS system and its main job is to unmap the 
allocated memory using the munmap system call.
Input Parameter: Nothing
Returns: Nothing
*/
void mems_finish(){
    struct MemBlock *head_main = freeHead;
    while(head_main != NULL)
    {
        struct SubBlock *head_sub = head_main->child;
        while(head_sub!=NULL)
        {
            int status = munmap((void*)head_sub,head_sub->size);
            head_sub=head_sub->next;
        }
        munmap((void*)head_main, head_main->parentSize);
        head_main=head_main->next;
    }
    
}


/*
Allocates memory of the specified size by reusing a segment from the free list if 
a sufficiently large segment is available. 

Else, uses the mmap system call to allocate more memory on the heap and updates 
the free list accordingly.

Note that while mapping using mmap do not forget to reuse the unused space from mapping
by adding it to the free list.
Parameter: The size of the memory the user program wants
Returns: MeMS Virtual address (that is created by MeMS)
*/ 

struct SubBlock* createSub(size_t size, bool type){
    if(size == 0) return NULL;
    struct SubBlock* tempSub;

    if(current + subSize > startHead + PAGE_SIZE){
        startHead = (int *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        current = startHead;
    }

    tempSub = (struct SubBlock*)current;
    tempSub -> size = size;
    tempSub -> type = type;
    tempSub -> prev = NULL; tempSub -> next = NULL;
    current += subSize;
    
    return tempSub;
}

struct MemBlock* createBlock(size_t size, size_t processSize){
    struct MemBlock* tempBlock;

    if(current + blockSize > startHead + PAGE_SIZE){
        startHead = (int *)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        current = startHead;
    }

    tempBlock = (struct MemBlock*)current;
    current += blockSize;
    
    struct SubBlock* tempProcess = createSub(processSize, PROCESS);
    struct SubBlock* tempHole = createSub(size - processSize, HOLE);
    
    if(tempHole) tempHole -> prev = tempProcess;
    if(tempProcess)tempProcess -> next = tempHole;

    tempBlock -> parentSize = size;
    tempBlock -> child = tempProcess;
    tempBlock -> PAD = NULL;
    tempBlock -> prev = NULL; tempBlock -> next = NULL;
    
    return tempBlock;
}

struct SubBlock* checkHole(struct MemBlock* blockPtr, size_t size, int *virtualAddress){
    struct SubBlock *subPtr = blockPtr -> child;

    while(subPtr){
        if(subPtr -> type == HOLE && subPtr -> size >= size) break;
        *virtualAddress += subPtr -> size;
        subPtr = subPtr -> next;
    }

    return subPtr;
}

void* mems_malloc(size_t size){
    int virtualAddress = virtualStart;
    struct MemBlock* blockPtr = freeHead;

    // TO check if any hole could fulfil the requirement.
    while(blockPtr){
        struct SubBlock* reqdHole = checkHole(blockPtr, size, &virtualAddress);
        if(reqdHole){
            struct SubBlock *prevSub = reqdHole -> prev;
            
            if(size == reqdHole -> size){
                reqdHole -> type = PROCESS;
                void *returType = (void *)virtualAddress;
                return returType;
            }

            struct SubBlock* tempSub = createSub(size, PROCESS);
            if(tempSub) 
            {
                tempSub -> next = reqdHole; 
                tempSub-> prev = prevSub;
                reqdHole -> prev = tempSub;
                if(prevSub) prevSub -> next = tempSub;
            }
            reqdHole->size = reqdHole->size - size;
            
            void *returType = (void *)virtualAddress;
            return returType;
        }
        if(blockPtr -> next == NULL) break;
        blockPtr = blockPtr -> next;
    }
    
    
    // In case there is no hole which could be used by the memory.
    size_t pageSize = size/PAGE_SIZE;
    pageSize = size % PAGE_SIZE == 0 ? pageSize : pageSize+1;
    pageSize = pageSize * PAGE_SIZE;
    
    struct MemBlock* tempBlock = createBlock(pageSize, size);
    if(blockPtr) blockPtr -> next = tempBlock;
    tempBlock -> prev = blockPtr;
    blockPtr = tempBlock;
    
    blockPtr -> PAD = mmap(NULL, pageSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if(freeHead == NULL){
        freeHead = blockPtr;
    }

    void *returType = (void *)virtualAddress;
    return returType;
}


/*
this function print the stats of the MeMS system like
1. How many pages are utilised by using the mems_malloc
2. how much memory is unused i.e. the memory that is in freelist and is not used.
3. It also prints details about each node in the main chain and each segment (PROCESS or HOLE) in the sub-chain.
Parameter: Nothing
Returns: Nothing but should print the necessary information on STDOUT
*/
void mems_print_stats() {
    printf("-----MeMs SYSTEM STATS -----\n");
    struct MemBlock* blockPtr = freeHead;
    int vAddress = virtualStart;
    int pages = 0;
    int unused = 0;
    int mainLength = 0;
    while(blockPtr){
        mainLength++;
        printf("MAIN[%d:%d]", vAddress, vAddress+blockPtr->parentSize-1);
        printf("-> ");
        pages += (blockPtr->parentSize)/ PAGE_SIZE;
        
        struct SubBlock *subPtr = blockPtr -> child;

        while(subPtr){
            char c = subPtr->type? 'P': 'H';
            unused += subPtr->type? 0:subPtr->size;
            printf("%c[%d:%d]", c, vAddress, vAddress+subPtr->size-1);
            printf(" <-> ");
            vAddress += subPtr -> size;
            subPtr = subPtr -> next;
        }
        printf("NULL\n");
        blockPtr = blockPtr -> next;
    }

    printf("Pages used: \t%d\n", pages);
    printf("Space unused: \t%d\n", unused);
    printf("Main chain Length: \t%d\n", mainLength);
    int subLength[mainLength];
    int count = 0;
    int i = 0;

    blockPtr = freeHead;
    while(blockPtr){
        struct SubBlock *subPtr = blockPtr -> child;
        count = 0;
        while(subPtr){
            count++;
            subPtr = subPtr -> next;
        }
        subLength[i++] = count;
        blockPtr = blockPtr -> next;
    }
    printf("Sub-chain Length array: [");

    for(int i = 0; i < mainLength; i++){
        printf("%d, ", subLength[i]);
    }

    printf("]\n");

    printf("----------------------------\n");
}


/*
Returns the MeMS physical address mapped to ptr ( ptr is MeMS virtual address).
Parameter: MeMS Virtual address (that is created by MeMS)
Returns: MeMS physical address mapped to the passed ptr (MeMS virtual address).
*/
void *mems_get(void* v_ptr){
    int target = (int) v_ptr;
    struct MemBlock* blockPtr = freeHead;
    int vAddress = virtualStart;

    while(blockPtr){
        if(target < vAddress + blockPtr->parentSize){
            target = target - vAddress;
            break;
        }
        vAddress += blockPtr->parentSize;
        blockPtr = blockPtr -> next;
    }

    void* ans = (void*)((int *)blockPtr -> PAD + target/4);
    return ans;
}


/*
this function free up the memory pointed by our virtual_address and add it to the free list
Parameter: MeMS Virtual address (that is created by MeMS) 
Returns: nothing
*/
void mems_free(void *v_ptr){
    int target = (int)v_ptr;
    struct MemBlock* blockPtr = freeHead;
    int vAdress = virtualStart;
    
    while(blockPtr){
        if(target < vAdress + blockPtr->parentSize){
            struct SubBlock* subPtr = blockPtr -> child;
            int size = 0;
            while(subPtr){
                if(vAdress == target){
                    break;
                }
                subPtr = subPtr -> next;
                vAdress += subPtr->size;
            }

            if(subPtr->type){
                size += subPtr -> size;
                subPtr -> type = PROCESS;
                struct SubBlock* prev = subPtr -> prev;
                if(prev->type == HOLE){
                    size += prev -> size;
                    struct SubBlock* pprev = prev-> prev;
                    if(pprev) pprev -> next = subPtr;
                    subPtr -> prev = pprev;
                }
                struct SubBlock* next = subPtr -> next;
                if(next -> type == HOLE){
                    size += next -> size;
                    struct SubBlock* nnext = next -> next;
                    if(nnext) nnext -> prev = subPtr;
                    subPtr -> next = nnext;
                }
                subPtr -> size = size;
                subPtr -> type = HOLE;
            }
            break;
        }
        vAdress += blockPtr->parentSize;
        blockPtr = blockPtr -> next;
    }
    
}
