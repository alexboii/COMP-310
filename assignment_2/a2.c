/**
 * TODO: Get input from file: something about reusing the getCmd function but instead of stdin use the pointer to the filestream
 **/

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <signal.h>

#define SEM_LOC "/eharfoSem"
#define SEM_COUNT_LOC SEM_LOC "count"

///////////////////////////////////////////////////////
//////////////// SHARED MEM DEFS //////////////////////
//////////////////////////////////////////////////////
#define NUM_TAB 10
#define NUM_SEC 2
#define MEM_LOC "/eharfo"
#define NAME_LENGTH 50
struct table
{
  char name[NAME_LENGTH];
  int  status; //0: available 1: reserved


}table;

int   SEM_MODE = O_RDWR;
int   SEM_FLAG = S_IRUSR | S_IWUSR;

int   TAB_SIZE = sizeof(table);
int   MEM_SIZE = NUM_TAB*NUM_SEC*sizeof(table);

//Shared memory flags 
int   SHM_MODE = O_RDWR;
int   SHM_FLAG = S_IRUSR | S_IWUSR;

//Memory mapping flags
int   MAP_MODE = PROT_READ | PROT_WRITE;
int   MAP_FLAG = MAP_SHARED;

//Shared mem ptr
struct table * ptr;
int fd;

sem_t * countSem;
sem_t * ctrl;
char* memPtrArg;

//Setup for shared memory and semaphores
int Initialize();
int Terminate();

//Reservation system commands, require memory safety
void PrintReservationStatus();
int ReserveTable(int section, int tableNum, char* name);
int GetFirstTableInSection(int section);
int ResetTables();

//Helper functions
char *getcmd      (char *args[], int maxArgc, int * argc);
int  toSectionNum (char * c);
int  toTableNum   (char * c, int section);

//Signal handling 
void signHndl(int sig);

int main()
{
  //Override signals
  if (signal(SIGINT, signHndl) == SIG_ERR )
  {
      printf("signals couldn't be set, critical error\n");
      return -1;
  }

  if(!Initialize())
  {
    return -1;
  }


  char *args[4];
  int argc;
  memPtrArg = NULL;

  while(1)
  {
    if(memPtrArg !=NULL)
    {
      free(memPtrArg);
    }
    argc =0;
    memPtrArg = getcmd(args,4,&argc);
   
    //Somehow no arguments passed?
    if(argc == 0)
    {
      continue;      
    }

    if(!strcmp(args[0],"reserve"))
    {
      if(argc<3)
      {
        //Invalid number
        printf("Not enough arguments for reserve command\nreserve <person_name> <section> <table_number(optional)\n");
        continue;
      }

      int sectionNum = toSectionNum(args[2]);
      if(sectionNum==-1)
      {
        printf("invalid input for section, please enter a letter from A-B/a-b\n");
        continue;
      }

      //Critical section begin
      sem_wait(ctrl);      
      int table = -1;
      if(argc==3)
      {
        table = GetFirstTableInSection(sectionNum);
        if(table==-1)
        {
          printf("No free tables available in selected section, please choose another section\n");
        }
        else if(table == -2)
        {
          printf("Section does not exist, please select a valid section (A-B)\n");
        }
      }
      else 
      {
        table = toTableNum(args[3],sectionNum);
        if(table==-1)
        {
          printf("Invalid entry for table number, please enter a positive number \n");
        }
        else if (table == -2)
        {
          printf("Invalid entry for table, table does number does not exist for this given section\n");
        }
      }

      //If invalid table number, we've already printed the error, post the semaphore, and go on with your day
      if(table <0)
      {
        sem_post(ctrl);                          
        continue;        
      }
      //Otherwise try to reserve a table
      int status = ReserveTable(sectionNum,table,args[1]);
      sleep(5);
      sem_post(ctrl);   
      //Critical section end

      if(status == 1)
      {
        printf("Reservation completed\n");
      }
      else if(status == -1)
      {
        printf("Section does not exist, please select a valid section (A-B)\n");
      }
      else if(status == -2)
      {
        printf("Table does not exist, please select a valid table (0-%d)\n",NUM_TAB);
      }
      else if(status == -3)
      {
        printf("Sorry, this table was already reserved, please try another table\n");
      }
      else if(status == -4)
      {
        printf("You must enter a non empty name for the reservation\n");
      }
    }
    else if(!strcmp(args[0],"status"))
    {
      sem_wait(ctrl);
      PrintReservationStatus();
      sem_post(ctrl);
    }
    else if(!strcmp(args[0],"init"))
    {
      sem_wait(ctrl);
      if(ResetTables())
      {
        printf("Successfully reset all reservations\n");
      }
      sem_post(ctrl);

    }
    else if(!strcmp(args[0],"exit"))
    {
      break;
    }
    else 
    {
      printf("Command not recognized\nValid commands are reserve,status,init,exit\n");
    }
  }

  if(Terminate())
  {
    return -2;
  }

  return 1;
}

void PrintReservationStatus()
{
  if(ptr != NULL)
  {
    struct table* currentT = ptr;
    for(int section=0; section<NUM_SEC;section++){
      printf("SECTION %c\n",(char)(section+65));
      for(int t =0; t<NUM_TAB; t++){
        currentT = ptr + t + section*NUM_TAB;
        printf("    Table %d: ",t + (section+1)*100);
        if(currentT->status)
        {
          printf("Reserved by %s",currentT->name);
        }
        else
        {
          printf("Available");
        }
        printf("\n");
      }
    }
  }
  else 
  {
    printf("Error, shared memory pointer was not initialized\n");
  }
}

int ResetTables()
{
  if(ptr != NULL)
  {
    int sizeTable = sizeof(table);
    struct table* currentT = ptr;
    for(int section=0; section<NUM_SEC;section++){
      for(int t =0; t<NUM_TAB; t++){
        currentT = ptr + t + section*NUM_TAB;
        currentT->status = 0;
        for(int c =0; c<NAME_LENGTH;c++){
          currentT->name[c] = '\0';
        }
      }
    }
    return 1;
  }
  else 
  {
    return 0;
  }
}

int GetFirstTableInSection(int section)
{
  if(ptr !=NULL)
  {
    if(section>=NUM_SEC || section<0)
    {
      //Invalid section number
      return -2;
    }

    for(int t=0; t<NUM_TAB;t++)
    {
        if(!((ptr + t + section*NUM_TAB)->status))
        {
          return t;
        }
    }

    //No available tables found
    return -1;
  }
  else 
  {
    //printf("Error, shared memory pointer was not initialized\n"); 
    return -3;   
  }

}

int ReserveTable(int section, int tNum, char* name)
{
  if(ptr!=NULL)
  {
    if(section>=NUM_SEC || section<0)
    {
      //Invalid section number
      return -1;
    }
    if(tNum<0 || tNum>= NUM_TAB)
    {
      //Invalid table number
      return -2;
    }

    struct table *selected = ptr + tNum + section*NUM_TAB;
    if(selected->status)
    {
      //Table not available
      return -3;
    }
   
    if(*name == '\0')
    {
      //Empty name string
      return -4;
    }

    selected->status = 1;
    int index =0;
    //Fill up name and put end line at the end of array
    while(*(name+index)!='\0')
    {
      selected->name[index] = *(name+index);
      index++;
    }
    selected->name[index]='\0';
    //Success
    return 1;
  }
  else
  {
    //Shared mem pointer bad
    return -5;
  }
}

char *getcmd(char *args[], int maxArgc, int * argc)
{
    char *token;

    char *line = NULL;
    //startline is used to keep track of the allocated memory block by get line in order to free it and avoid a memory leak
    char *startLine;

    size_t linecap = 0;

    printf(">");
    getline(&line, &linecap, stdin);
    startLine = line;
    
    (*argc) =0;
    while ((token = strsep(&line, " \t\n")) != NULL && (*argc)<maxArgc)
    {
      //Extra check for nullity because apparently first one isnt working?
      if(*token  == 0)
        break;
      
       args[*argc] = token;
       (*argc)++;
    }

    return startLine;
}

int Initialize()
{
  //For error checking
  ptr = NULL;

  //Create sempahore with an initial value of 1 or retrieve it, this sempahore is used to count the number of processes alive and currently using the shared memory
  countSem = sem_open(SEM_COUNT_LOC, SEM_MODE | O_CREAT , SEM_FLAG,0);
  //Post in the semaphore in order to notify that a new process exists
  sem_post(countSem);

  //create the access semaphore
  ctrl = sem_open(SEM_LOC, SEM_MODE | O_CREAT, SEM_FLAG,1);
  
  //Try to create the shared memory, if you are unable to, that means someone else created it and set it up
  if((fd = shm_open(MEM_LOC, SHM_MODE | O_CREAT | O_EXCL, SHM_FLAG))>=0)
  {
    //Error checking
    if(fd < 0)
    {
      printf("error opening shm %s\n", strerror(errno));
    }
    
    sem_wait(ctrl);
    
    // configure shared mem size 
    ftruncate(fd,MEM_SIZE);  
    // map shared memory segment to local address space
    ptr = mmap(0, MEM_SIZE, MAP_MODE, MAP_FLAG, fd, 0);
    //Error checking
    if(ptr == MAP_FAILED)
    {
      printf("MAP FAILED\n");
      return -1;
    }
    
    ResetTables();
    sem_post(ctrl);
  }
  else
  {
    fd = shm_open(MEM_LOC, SHM_MODE, SHM_FLAG);
    // configure shared mem size 
    ftruncate(fd,MEM_SIZE);  
    // map shared memory segment to local address space
    ptr = mmap(0, MEM_SIZE, MAP_MODE, MAP_FLAG, fd, 0);
    //Error checking
    if(ptr == MAP_FAILED)
    {
      printf("MAP FAILED\n");
      return 0;
    }
  }

  return 1;
}

int Terminate()
{
  //Clean up, unmap the memory and close the file descriptor we opened to it
  munmap(ptr, MEM_SIZE);
  close(fd);

  //Close the control (binary) sempahore we opened
  sem_close(ctrl);

  //Decrement our process count sempahore to indicate that this process is about to end
  sem_wait(countSem);
  //get the value of the semaphore to know whether there are any other processes running which might care about shared memory
  int pCount = 1;
  sem_getvalue(countSem,&pCount); 
  //Close the process counter semaphore to indicate we're also done with it
  sem_close(countSem);
  //If there are no other processes still using shared memory, we clean can unlink it safely (as well as our semaphores)
  if(pCount<=0)
  {
    shm_unlink(MEM_LOC);
    sem_unlink(SEM_LOC);
    sem_unlink(SEM_COUNT_LOC);
  }

  return 1;
}

int toSectionNum(char * sec)
{
  if(*(sec+1) != '\0')
  {
    return -1;
  }
  int value = (int)*sec;
  if(value>='A' && value<='Z')
  {
    //upercase
    return value - 'A';
  }
  else if(value >= 'a' && value <= 'z')
  {
    //lowercase
    return value - 'a';
  }
}

int toTableNum (char * c, int section)
{
  if(*c == '\0')
  {
    return -1;
  }
  int val = atoi(c);  
  if(val==0  && *c != '0')
  {
    return -1;
  }
  val = val - (section+1)*100;
  if(val<0 || val >=NUM_TAB)
  {
    return -2;
  }

  return val;
}

void signHndl(int sig)
{
    Terminate();
    exit(1);
}
