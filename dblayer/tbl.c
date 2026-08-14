#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "tbl.h"
#include "codec.h"
#include "../pflayer/pf.h"

#define SLOT_COUNT_OFFSET 2
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

// Macro For Some fixed Pointer in sloted page 
#define SLOT_COUNT_SIZE 2
#define FREE_SPACE_POINTER_SIZE 2
#define FREE_SPACE_ENTRY_LOCATOR 4
#define FREE_SPACE_END_LOCATOR 4095
#define SLOT_ENTRY_LOCATOR 6

#define MAX_PAGE_SIZE 4000

int  getLen(int slot, byte *pageBuf);
int  getNumSlots(byte *pageBuf);
void setNumSlots(byte *pageBuf, short nslots);
int  getNthSlotOffset(int slot, char* pageBuf);

int getLen(int slot, byte *pageBuf){
    char lenbuf[2];
    memcpy(lenbuf,pageBuf+SLOT_COUNT_OFFSET+SLOT_COUNT_SIZE+FREE_SPACE_POINTER_SIZE+((slot-1)*4)+2,2);
    short len=DecodeShort(lenbuf);
    return len;
}
int  getNthSlotOffset(int slot, char* pageBuf){
    char offbuf[2];
    memcpy(offbuf,pageBuf+SLOT_COUNT_OFFSET+SLOT_COUNT_SIZE+FREE_SPACE_POINTER_SIZE+((slot-1)*4),2);
    short offset=DecodeShort(offbuf);
    return offset;
}

int getNumSlots(byte *pageBuf){
    char slotCountBuf[2];
    memcpy(slotCountBuf,pageBuf+SLOT_COUNT_OFFSET,2);
    short slotCount=DecodeShort(slotCountBuf);
    return slotCount;
}

void setNumSlots(byte *pageBuf, short nslots){
    char encodedSlot[2];
    EncodeShort(nslots,encodedSlot);
    memcpy(pageBuf+SLOT_COUNT_OFFSET, encodedSlot, 2);
}

/**
   Opens a paged file, creating one if it doesn't exist, and optionally
   overwriting it.
   Returns 0 on success and a negative error code otherwise.
   If successful, it returns an initialized Table*.
 */
int
Table_Open(char *dbname, Schema *schema, bool overwrite, Table **ptable)
{

    // Initialize PF, create PF file,
    // allocate Table structure  and initialize and return via ptable
    // The Table structure only stores the schema. The current functionality
    // does not really need the schema, because we are only concentrating
    // on record storage. 

    PF_Init();
    int status,fd;
    Table* table=(Table *) malloc(sizeof(Table));

    if(overwrite){
        PF_DestroyFile(dbname);
        status=PF_CreateFile(dbname);
        if(status!=PFE_OK){
            printf("Error in Creating File.\n");
            PF_PrintError();
            exit(EXIT_FAILURE);
        }
        fd=PF_OpenFile(dbname);
        if(fd<0){
            printf("Error in Opening File.\n");
            PF_PrintError();
            exit(EXIT_FAILURE);
        }
        table->currentPageNum=-1;
        table->freeSpceEndLocator=FREE_SPACE_END_LOCATOR;
        table->slotEntryLocator=SLOT_ENTRY_LOCATOR; 
    }else{
        fd=PF_OpenFile(dbname);
        if(fd<PFE_OK){
            status=PF_CreateFile(dbname);
            printf("File Created Successfully\n");
            fd=PF_OpenFile(dbname);
            table->currentPageNum=-1;
            table->freeSpceEndLocator=FREE_SPACE_END_LOCATOR;
            table->slotEntryLocator=SLOT_ENTRY_LOCATOR;
        }
    }

    
    table->fd=fd;
    table->overwrite=overwrite;
    table->schema=schema;
    *ptable=table;
    table->dirty=0;
    return status;
}

void
Table_Close(Table *tbl) {
    if(tbl->operation==INSERT){
        PF_UnfixPage(tbl->fd,tbl->currentPageNum,tbl->dirty);   // Write Current Page to disk
    }
    PF_CloseFile(tbl->fd);
    free(tbl);
}


int
Table_Insert(Table *tbl, byte *record, int len, RecId *rid) {
    // Allocate a fresh page if len is not enough for remaining space
    // Get the next free slot on page, and copy record in the free
    // space
    // Update slot and free space index information on top of page.

    int fd=tbl->fd;
    int pagenum;
    char *buffer,*buffer2;
    short offset,slotEntry;
    char freeSpacePointer[2],offsetEntry[2],lengthEntry[2];
    int previousPage;
    if(tbl->currentPageNum==-1){            // Program goes in this if condition if first record is inserted in db file
        int status=PF_AllocPage(fd,&pagenum,&buffer);
        if(status!=PFE_OK){
            printf("Error in Allocating Page To File.\n");
            PF_PrintError();
            exit(EXIT_FAILURE);
        }
        tbl->currentPageNum=pagenum;
        tbl->bufferPointer=buffer;
        setNumSlots(tbl->bufferPointer,0);
    }
    else{
        if((tbl->freeSpceEndLocator-tbl->slotEntryLocator+1)<(len+4)){      // if current page has low space then memory need for new record
            previousPage=tbl->currentPageNum;
            int status=PF_AllocPage(fd,&pagenum,&buffer2);
            if(status!=PFE_OK){
                printf("Error in Allocating Page To File.\n");
                PF_PrintError();
                exit(EXIT_FAILURE);
            }
            tbl->bufferPointer=buffer2;
            tbl->currentPageNum=pagenum;
            tbl->freeSpceEndLocator=FREE_SPACE_END_LOCATOR;
            tbl->slotEntryLocator=SLOT_ENTRY_LOCATOR;
            setNumSlots(tbl->bufferPointer,0);
            PF_UnfixPage(tbl->fd,previousPage,tbl->dirty);      //Save previous page to disk.
        }
    }
    offset=tbl->freeSpceEndLocator-len+1;
    EncodeShort(offset,offsetEntry);
    EncodeShort(len,lengthEntry);
    memcpy(tbl->bufferPointer+offset,record,len);   // Enter Binary Record in page
    tbl->freeSpceEndLocator-=len;                   // Update free space pointer in table structure
    EncodeShort((short)tbl->freeSpceEndLocator,freeSpacePointer);
    memcpy(tbl->bufferPointer+FREE_SPACE_ENTRY_LOCATOR, freeSpacePointer, 2);   // Update Free Space Pointer in current page
    memcpy(tbl->bufferPointer+tbl->slotEntryLocator, offsetEntry, 2);           // Add Offset For currect record in current page
    memcpy(tbl->bufferPointer+tbl->slotEntryLocator+2, lengthEntry, 2);         // Add Length For currect record in current page
    tbl->slotEntryLocator+=4;                       // Update slot entry pointer in table structure
    short slots=getNumSlots(tbl->bufferPointer);
    slots++;
    setNumSlots(tbl->bufferPointer,slots);      // Increment Slots by 1 in current page
    *rid=tbl->currentPageNum<<16|slots;         // Calculate Record Id 
    tbl->dirty=1;                               // Set Dirty Bit for Current Page
    return 0;
}

#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(EXIT_FAILURE);}}

/*
  Given an rid, fill in the record (but at most maxlen bytes).
  Returns the number of bytes copied.
 */
int
Table_Get(Table *tbl, RecId rid, byte *record, int maxlen) {
    char *buffer2;
    short slot = rid & 0xFFFF;
    short pageNum = rid >> 16;
    int noBytes;
    int status=PF_GetThisPage(tbl->fd,pageNum,&buffer2);
    if(status!=PFE_OK){
        printf("%d\n",status);
        printf("Error in Getting This Page\n");
        exit(EXIT_FAILURE);
    }
    int slotOffset=getNthSlotOffset(slot,buffer2);
    int slotLength=getLen(slot,buffer2);

    if(slotLength<=maxlen){
        memcpy(record,buffer2+slotOffset,slotLength);
        noBytes=slotLength;

    }else{
        memcpy(record,buffer2+slotOffset,maxlen);
        noBytes=maxlen;

    }
    PF_UnfixPage(tbl->fd,pageNum,0);
    // PF_DisposePage(tbl->fd,pageNum);
    return noBytes;
    // UNIMPLEMENTED;
    // PF_GetThisPage(pageNum)
    // In the page get the slot offset of the record, and
    // m
    // retemcpy bytes into the record supplied.
    // Unfix the pageurn len; // return size of record
}

void
Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn) {

    // UNIMPLEMENTED;

    // For each page obtained using PF_GetFirstPage and PF_GetNextPage
    //    for each record in that page,
    //          callbackfn(callbackObj, rid, record, recordLen)
    int fd=tbl->fd;
    char *pageBuf,*actualAddress;
    RecId recId;
    int slots,pageNum;  //slots means number of slots
    short nthSlotOffset,nthSlotlength;
    byte * row;
    
    
    // For Printing Header Entry
    Schema *schema = (Schema *) callbackObj;
    ColumnDesc * col;
    printf("%10s ","Record Id");
    for(int i=0;i<schema->numColumns;i++){
        col=schema->columns[i];
        if(col->type==VARCHAR){
            printf("%-40s ",col->name);
        }
        else if(col->type==INT){
            printf("%10s ",col->name);
        }
        else if(col->type==LONG){
            printf("%20s",col->name);
        }
        else{
            printf("Invalid Schema\n");
            exit(EXIT_FAILURE);
        }
    }
    printf("\n");
    // For Printing Header Entry
    int status=PF_GetFirstPage(fd,&(tbl->currentPageNum),&pageBuf);
    if(status!=PFE_OK){
        printf("%d\n",status);
        printf("Error in getting First Page\n");
        exit(EXIT_FAILURE);
    }
    while(status!=PFE_EOF){
        slots=getNumSlots(pageBuf);
        for(int i=1;i<=slots;i++){
            nthSlotOffset=getNthSlotOffset(i,pageBuf);
            nthSlotlength=getLen(i,pageBuf);
            
            actualAddress=pageBuf+nthSlotOffset;

            // Stored nBytes starting from actualaddr to row
            row=(char *)malloc(nthSlotlength*sizeof(char));
            memcpy(row,actualAddress,nthSlotlength);
            
            // To calculate the pageNum and slot

            pageNum = tbl->currentPageNum << 16;
            recId=pageNum|i;
            callbackfn(callbackObj,recId,row,nthSlotlength);
            free(row);
            printf("\n");
        }
        PF_UnfixPage(tbl->fd,tbl->currentPageNum,0);
        // Go to the next page
        status=PF_GetNextPage(fd,&(tbl->currentPageNum),&pageBuf);
        if (status < 0 && status != PFE_EOF) {
            printf("Error getting next page.\n");
            PF_PrintError();
            exit(EXIT_FAILURE);
        }
    }
}


