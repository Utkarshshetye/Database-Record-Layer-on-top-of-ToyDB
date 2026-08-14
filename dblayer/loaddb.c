#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include "codec.h"
#include "../pflayer/pf.h"
#include "../amlayer/am.h"
#include "tbl.h"
#include "util.h"

#define checkerr(err) {if (err < 0) {PF_PrintError(); exit(1);}}

#define MAX_PAGE_SIZE 4000


#define DB_NAME "data.db"
#define INDEX_NAME "data.db.0"
#define CSV_NAME "data.csv"


/*
Takes a schema, and an array of strings (fields), and uses the functionality
in codec.c to convert strings into compact binary representations
 */
int
encode(Schema *sch, char **fields, byte *record, int spaceLeft) {
    // UNIMPLEMENTED;
    // for each field
    //    switch corresponding schema type is
    //        VARCHAR : EncodeCString
    //        INT : EncodeInt
    //        LONG: EncodeLong
    // return the total number of bytes encoded into record
    
    int locator=0,len1;    //locator is pointing to current location of write in record
    int value;
    long value2;
    char * endptr;
    for(int i=0;i<sch->numColumns;i++){
        if(sch->columns[i]->type==VARCHAR){
            if(spaceLeft<strlen(fields[i])){
                printf("Records is too big\n");
                exit(1);
            }
            len1=EncodeCString(fields[i],record+locator,spaceLeft);
            locator+=len1;
            spaceLeft-=len1;
        }
        else if(sch->columns[i]->type==INT){
            if(spaceLeft<4){
                printf("Records is too big\n");
                exit(1);
            }
            value= atoi(fields[i]);                 // I assume that atoi works perfectly and no error occured
            EncodeInt(value,record+locator);
            spaceLeft-=4;
            locator+=4;
        }
        else if(sch->columns[i]->type==LONG){
            if(spaceLeft<8){
                printf("Records is too big\n");
                exit(1);
            }
            value2= strtol(fields[i],&endptr,10);              // I assume that strtol works perfectly and no error occured
            EncodeLong(value2,record+locator);
            spaceLeft-=8;
            locator+=8;
        }
        else{
            printf("Undefined type of field\n");
            exit(1);
        }
    }
    return locator;
}

Schema *
loadCSV() {
    // Open csv file, parse schema
    FILE *fp = fopen(CSV_NAME, "r");
    if (!fp) {
	perror("data.csv could not be opened");
        exit(EXIT_FAILURE);
    }

    char buf[MAX_LINE_LEN];
    char *line = fgets(buf, MAX_LINE_LEN, fp);
    if (line == NULL) {
	    fprintf(stderr, "Unable to read data.csv\n");
	    exit(EXIT_FAILURE);
    }

    // Open main db file
    Schema *sch = parseSchema(line);
    Table *tbl;

    if(Table_Open("data.db",sch,1,&tbl)!=0){
        printf("Some Error in creating Table\n");
        exit(EXIT_FAILURE);
    }

    char *tokens[MAX_TOKENS];
    char record[MAX_PAGE_SIZE];
    PF_DestroyFile("data.db.0");
    int status=AM_CreateIndex("data.db", 0, 'i', 4);      // Create Index
    if(status!=AME_OK){
        printf("Error in crearing index\n");
        exit(EXIT_FAILURE);
    }
   
    int indexFd=PF_OpenFile("data.db.0");               //Open Index File
    if(indexFd<0){
        printf("Error in opening index file\n");
        exit(EXIT_FAILURE);
    }
    printf("%-10s     %-40s\n","Record Id","Country name");
    char populationEntry[4];
    while ((line = fgets(buf, MAX_LINE_LEN, fp)) != NULL) {
        int n = split(line, ",", tokens);
        assert (n == sch->numColumns);
        int len = encode(sch, tokens, record, sizeof(record));
        RecId rid;
        Table_Insert(tbl,record,len, &rid);         // Insert the record in page of .db file

        printf("%10d     %-40s\n", rid, tokens[0]);          // Just for checking rid

        int population = atoi(tokens[2]);

        EncodeInt(population,populationEntry);
        status=AM_InsertEntry(indexFd , 'i',4, populationEntry, rid); //Insert record in index
        if(status!=AME_OK){
            printf("Error in crearing index\n");
            exit(EXIT_FAILURE);
        }
    }
    fclose(fp);
    tbl->operation=INSERT;  // To give Table_Close function hint that it should write current page to disk
    Table_Close(tbl);
    status=PF_CloseFile(indexFd);
    if(status!=PFE_OK){
        printf("Error in closing index file\n");
        exit(EXIT_FAILURE);
    }
    return sch;
}

int
main() {
    loadCSV();
}
