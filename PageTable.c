// PageTable.c ... implementation of Page Table operations
// COMP1521 17s2 Assignment 2
// Written by John Shepherd, September 2017

#include <stdlib.h>
#include <stdio.h>
#include "Memory.h"
#include "Stats.h"
#include "PageTable.h"
// Symbolic constants

#define NOT_USED 0
#define IN_MEMORY 1
#define ON_DISK 2

// PTE = Page Table Entry

typedef struct PTE{
   char status;      // NOT_USED, IN_MEMORY, ON_DISK
   char modified;    // boolean: changed since loaded
   int  frame;       // memory frame holding this page
   int  accessTime;  // clock tick for last access
   int  loadTime;    // clock tick for last time loaded
   int  nPeeks;      // total number times this page read
   int  nPokes;      // total number times this page modified
   int pageNo;          // pagenumber
   struct PTE* prev; //Setup for linked list
   struct PTE* next;
} PTE;

// The virtual address space of the process is managed
//  by an array of Page Table Entries (PTEs)
// The Page Table is not directly accessible outside
//  this file (hence the static declaration)

static PTE *PageTable;      // array of page table entries

static PTE *fifoQueue;      // Linked list  
static int  nPages;         // # entries in page table
static int  replacePolicy;  // how to do page replacement
static int  fifoLast;       // index of last PTE in FIFO list


// initPageTable: create/initialise Page Table data structures

void initPageTable(int policy, int np)
{
   PageTable = malloc(np * sizeof(PTE));
   fifoQueue = NULL;

   if (PageTable == NULL) {
      fprintf(stderr, "Can't initialise Memory\n");
      exit(EXIT_FAILURE);
   }
   replacePolicy = policy;
   nPages = np;
   fifoLast = -1;
   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      p->status = NOT_USED;
      p->modified = 0;
      p->frame = NONE;
      p->accessTime = NONE;
      p->loadTime = NONE;
      p->nPeeks = p->nPokes = 0;
      p->prev = NULL;
      p->next = NULL;
      p->pageNo = i; //position in pagetable  
   }
}


// requestPage: request access to page pno in mode
// returns memory frame holding this page
// page may have to be loaded
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

int requestPage(int pno, char mode, int time)
{
   if (pno < 0 || pno >= nPages) {
      fprintf(stderr,"Invalid page reference\n");
      exit(EXIT_FAILURE);
   }
   PTE *p = &PageTable[pno];
   int fno; // frame number
   switch (p->status) {
   case NOT_USED:
   case ON_DISK:
      fno = findFreeFrame();

      if (fno == NONE) {
#ifdef DBUG
         printf("Evict page %d\n", fifoQueue->pageNo);
#endif
         PTE* victim = fifoQueue;//victim will always be at beginning of linked list
         if(victim->modified) {
            int frameNo = victim->frame; //save the victim if it is modified
            saveFrame(frameNo);
         }
         fno = victim->frame; 

         if(victim->next != NULL) { //remove the victim from the fifo queue
            (victim->next)->prev = NULL; 
         }

         fifoQueue = victim->next; // reset list to next item in fifo/lru queue

         victim->next = NULL; //remove victim from queue

         victim->status = ON_DISK; //reset victim
         victim->modified = 0;
         victim->frame = NONE;
         victim->accessTime = NONE;
         victim->loadTime = NONE;
      }  
      printf("Page %d given frame %d\n",pno,fno);

      loadFrame(fno, pno, time);
      
      p->next = NULL;

      if(fifoLast >= 0) { //If fifolast < 0 the list is empty
         PageTable[fifoLast].next = p; //add it into fifo queue
         p->prev = &PageTable[fifoLast];
      } else  {
         fifoQueue = p; //create a new list
      }

      p->status = IN_MEMORY; //initialise the page
      p->modified = 0;
      p->frame = fno;
      p->accessTime = time;
      p->loadTime = time;

      fifoLast = pno; //set pointer to last element in fifoQueue
      countPageFault();
      break;
   case IN_MEMORY:
      fno = p->frame; 
      countPageHit();

      if(replacePolicy == REPL_LRU) { //if LRU move frame to end of list
         if(p->next != NULL) { //not already last element in list
            //do nothing
            if(p->prev != NULL) { //remove it from its current location
               p->prev->next = p->next;
            } 

            if(p->next != NULL) {
               p->next->prev = p->prev;
            }

            PageTable[fifoLast].next = p; //move it onto end of fifo queue
            p->prev = &PageTable[fifoLast];
            fifoLast = pno; 
            if(fifoQueue == p)
               fifoQueue = p->next;
            p->next = NULL;
         }  
         
      }
      break;
   default:
      fprintf(stderr,"Invalid page status\n");
      exit(EXIT_FAILURE);
   }
   if (mode == 'r')
      p->nPeeks++;
   else if (mode == 'w') {
      p->nPokes++;
      p->modified = 1;
   }
   

   p->accessTime = time;
   return p->frame;
}


// showPageTableStatus: dump page table
// PTE(status,modified,frame,accessTime,nextPage,nPeeks,nWrites)

void showPageTableStatus(void)
{
   char *s;
   printf("%4s %6s %4s %6s %7s %7s %7s %7s\n",
          "Page","Status","Mod?","Frame","Acc(t)","Load(t)","#Peeks","#Pokes");
   for (int i = 0; i < nPages; i++) {
      PTE *p = &PageTable[i];
      printf("[%02d]", i);
      switch (p->status) {
      case NOT_USED:  s = "-"; break;
      case IN_MEMORY: s = "mem"; break;
      case ON_DISK:   s = "disk"; break;
      }
      printf(" %6s", s);
      printf(" %4s", p->modified ? "yes" : "no");
      if (p->frame == NONE)
         printf(" %6s", "-");
      else
         printf(" %6d", p->frame);
      if (p->accessTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->accessTime);
      if (p->loadTime == NONE)
         printf(" %7s", "-");
      else
         printf(" %7d", p->loadTime);
      printf(" %7d", p->nPeeks);
      printf(" %7d", p->nPokes);
      printf("\n");
   }
}


