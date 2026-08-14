#ifndef _TBL_H_
#define _TBL_H_
#include <stdbool.h>

#define VARCHAR 1
#define INT     2
#define LONG    3


//For Table Close
#define SCAN 0
#define INSERT 1

typedef char byte;

typedef struct {
    char *name;
    int  type;  // one of VARCHAR, INT, LONG
} ColumnDesc;

typedef struct {
    int numColumns;
    ColumnDesc **columns; // array of column descriptors
} Schema;

typedef struct {
    Schema *schema;
    int fd;
    bool overwrite;
    int currentPageNum;
    int freeSpceEndLocator;   //Pointer To End Of Free Space of Current Page 
    int slotEntryLocator;       //Pointer In Header Of Current Page Where new Slot Entry Should made 
    bool dirty;
    bool operation;
    char * bufferPointer;    // Page Buffer Pointer For Current Page 
} Table ;

typedef int RecId;

int
Table_Open(char *fname, Schema *schema, bool overwrite, Table **table);

int
Table_Insert(Table *t, byte *record, int len, RecId *rid);

int
Table_Get(Table *t, RecId rid, byte *record, int maxlen);

void
Table_Close(Table *);

typedef void (*ReadFunc)(void *callbackObj, RecId rid, byte *row, int len);

void
Table_Scan(Table *tbl, void *callbackObj, ReadFunc callbackfn);

#endif
