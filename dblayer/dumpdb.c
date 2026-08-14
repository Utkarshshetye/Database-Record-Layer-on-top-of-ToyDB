#include <stdio.h>
#include <stdlib.h>
#include<string.h>
#include "codec.h"
#include "tbl.h"
#include "util.h"
#include "../pflayer/pf.h"
#include "../amlayer/am.h"
#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(1);}}

#define MAX_PAGE_SIZE 4000
char buffer[MAX_PAGE_SIZE];
long longData;
int intData;

void
printRow(void *callbackObj, RecId rid, byte *row, int len) {
    int bufferReadPointer=0;        // Pointer to reading head of current record
    Schema *schema = (Schema *) callbackObj;
    byte *cursor = row;
    ColumnDesc * col;
    printf("%10d ",(int)rid);
    
    for(int i=0;i<schema->numColumns;i++){
        col=schema->columns[i];
        if(col->type==VARCHAR){
            bufferReadPointer+=DecodeCString(row+bufferReadPointer,buffer,MAX_PAGE_SIZE);
            bufferReadPointer+=2;
            printf("%-40s ",buffer);
        }
        else if(col->type==INT){
            intData=DecodeInt(row+bufferReadPointer);
            printf("%10d ",intData);
            bufferReadPointer+=4;
        }
        else if(col->type==LONG){
            longData=DecodeLong(row+bufferReadPointer);
            printf("%20ld ",longData);
            bufferReadPointer+=8;
        }
        else{
            printf("Invalid Schema\n");
            exit(EXIT_FAILURE);
        }

    }
}

#define DB_NAME "data.db"
#define INDEX_NAME "data.db.0"
	 
void
index_scan(Table *tbl, Schema *schema, int indexFD, int op, int value) {
    // UNIMPLEMENTED;
    /*
    Open index ...
    while (true) {
	find next entry in index
	fetch rid from table
        printRow(...)
    }
    close index ...
    */
    char valueBuffer[4];
    EncodeInt(value,valueBuffer);
    int scanDesc=AM_OpenIndexScan(indexFD,'i',4,op,valueBuffer);
    int recId,length;
    short pageNum,slot;
    char buffer2[4095];
    while(true){
        recId=AM_FindNextEntry(scanDesc);
        if(recId==AME_EOF)
            break;
        length=Table_Get(tbl, recId, buffer2, MAX_PAGE_SIZE); 
        printRow(schema,recId,buffer2,length);
        printf("\n");
    }
    AM_CloseIndexScan(scanDesc);
}

int
main(int argc, char **argv) {
    char *schemaTxt = "Country:varchar,Capital:varchar,Population:int";
    Schema *schema = parseSchema(schemaTxt);
    Table *tbl;
    PF_Init();
    Table_Open("data.db",schema,0,&tbl);
    
    // UNIMPLEMENTED;
    if (argc == 2 ){
        if(strcmp(argv[1],"s")==0) {
            // UNIMPLEMENTED;
            // invoke Table_Scan with printRow, which will be invoked for each row in the table.
            Table_Scan(tbl,schema,printRow);
        } else if(strcmp(argv[1],"i")==0) {
            // index scan by default
            int indexFD = PF_OpenFile(INDEX_NAME);
            checkerr(indexFD);

            // Ask for populations less than 100000, then more than 100000. Together they should
            // yield the complete database.
            printf("Index Search-1: LESS_THAN_EQUAL 100000\n");
            index_scan(tbl, schema, indexFD, LESS_THAN_EQUAL, 100000);
            printf("\n\n\nIndex Search-2: GREATER_THAN 100000\n");
            index_scan(tbl, schema, indexFD, GREATER_THAN, 100000);
            PF_CloseFile(indexFD);
        }
        else{
            printf("Invalid Argument\n");
            exit(EXIT_FAILURE);
        }
    }
    else{
        printf("Incorrect Command\n");
        exit(EXIT_FAILURE);
    }

    tbl->operation=SCAN;
    Table_Close(tbl);
    
}
