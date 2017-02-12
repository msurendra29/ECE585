/*

DDR3 DRAM Memory Controller Simulation.
Policy : In order no access scheduling open page scheduling policy.

Author: Surendra Maddula



*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>


#define EMPTY 									0
#define QUEUE_SIZE 							16
#define TRUE 										1
#define FALSE 									0
#define ZERO 										0
#define CPU_TO_DRAM_CLOCK_RATIO 4
#define BYTE_SELECT 						0
#define COLUMN 									1
#define BANK 										2
#define ROW 										3
#define BYTE_MASK								0x7
#define ROW_MASK								0x7FFD0000
#define COL_MASK								0x3FF8
#define BANK_MASK								0x1C000
#define TRC 										50
#define TRCD 										14
#define TCAS  									14
#define TRP  										14
#define TBURST 									4
#define TRAS 										36
#define TWR											16
#define TRTP 										8
#define TCWL 										10
#define TWTR										8
#define TCCD										4
#define A 											0
#define R												1
#define W												2
#define P												3
#define MAX_INPUT_FILENAME_SIZE 100
#define BANK_SIZE               8
#define NUMBER_OF_COMMANDS      4

struct data {										//Data structure used to store fields of file, temporarily and then
	char hex_address[30];							// used it to enqueue.
	char operation[30];
	int time;
};

struct queue {									// Linked list used as FIFO.
	unsigned int hex_address;
	char operation[30];
	int time;
	int capacity;
	int current_queue_size;
	int bank;
	int row;
	int column;
	int byte_select;
	struct queue *next;
};

//Function Declarations

struct queue * initialize_q();							// Used for queue Initialization, and set the capacity and current queue size.
void enqueue(struct queue *, struct data );	//Adding a request to the FIFO.
void dequeue(struct queue *);							//Removing a request from the FIFO, after successful completion of RDAP/WRAP command.
void display(struct queue *);							//Used to display the element in the FIFO.
int hex_address_splitter(int,int);						//This helps to divide the 32 bits to corresponding row, bank, col, and byte select.

int cpu_clock=ZERO;
int dram_clock =ZERO;
int queue_empty_flag = FALSE;
int new_request_time_less_than_prev = FALSE;
int take_cpu_requests     = TRUE;
int queue_FULL_flag = FALSE;
struct data D;
struct queue *q = NULL;
struct queue *front= NULL;
struct queue *rear= NULL;

int hex_address_splitter(int hex, int required_bits)
{
	int select;
	switch(required_bits)
	{
		case BYTE_SELECT:
			return select = hex & BYTE_MASK;

		case COLUMN:
		return select = (hex & COL_MASK) >> 3 ;

		case BANK:
			return select = (hex & BANK_MASK )>> 14;

		case ROW:
			return select = (hex & ROW_MASK) >> 17;

	}
}

struct queue * initialize_q()
{
	struct queue *i = NULL;
	i= (struct queue *)malloc(sizeof(struct queue));
	i->capacity = QUEUE_SIZE;
	i->current_queue_size = ZERO;
	return i;

}

void enqueue(struct queue *p, struct data d)
{
	struct queue *temp = NULL;
		//printf("ENQUEUE: %u\t%u",rear,front);
	if(p->current_queue_size == p->capacity)
	{
			 perror("Queue is Full.\n");
			 queue_FULL_flag = TRUE;

	}
	else if(p->current_queue_size == EMPTY)
	{
		queue_empty_flag = FALSE;
		temp = (struct queue *)malloc(sizeof(struct queue));
		temp->hex_address = (unsigned int)strtol(d.hex_address, NULL, ZERO);
		temp->byte_select = hex_address_splitter(temp->hex_address, BYTE_SELECT);
		temp->column = hex_address_splitter(temp->hex_address, COLUMN);
		temp->bank = hex_address_splitter(temp->hex_address, BANK);
		temp->row = hex_address_splitter(temp->hex_address, ROW);
		strcpy(temp->operation,d.operation);
		temp->time = d.time;
		temp->next = NULL;
		front = temp;
		rear = temp;
		//printf("ENQUEUE1: %u\t%u",rear,front);
		p->current_queue_size++;
	}
	else
	{
		if(d.time < front->time)
		{
			new_request_time_less_than_prev = TRUE;
			printf("WARNING! \n\aNew request time cannot be less than previous request time, aborting .......\n");
		}
		else
		{
			 if(d.time ==front->time)
			 {
			 	printf("WARNING! \n\aNew request time is equal to previous request, can be used in multi core processors\n");
			 }
			temp = (struct queue *)malloc(sizeof(struct queue));
			temp->hex_address = (unsigned int)strtol(d.hex_address, NULL, 0);
			temp->byte_select = hex_address_splitter(temp->hex_address, BYTE_SELECT);
			temp->column = hex_address_splitter(temp->hex_address, COLUMN);
			temp->bank = hex_address_splitter(temp->hex_address, BANK);
			temp->row = hex_address_splitter(temp->hex_address, ROW);
			strcpy(temp->operation,d.operation);
			temp->time = d.time;
			temp->next = NULL;
			front->next=temp;
			front = temp;
			p->current_queue_size++;
		}
	}
}

void display(struct queue *d)
{
	d = rear;
	while(d!=NULL)
	{
		//printf("%d\t\t0x%x\t\t%s\t\t%d\n",cpu_clock,d->hex_address,d->operation,d->time);
		d = d->next;
	}
}

void dequeue(struct queue * d)
{
	struct queue *temp;
	//printf("DEQUEUE: %u\t%u",rear,front);
	if(rear!=front)
	{
		temp = rear;
		rear = rear->next;
		free(temp);
		queue_empty_flag = FALSE;
		d->current_queue_size--;
		if(d->current_queue_size < d->capacity)
		{
			queue_FULL_flag = FALSE;
		}
	}
	else
	{
		printf("Queue is empty \n");
		queue_empty_flag = TRUE;
	}
}

int main()
{
	int current_bank, current_row, column;
	int active_row_in_bank[BANK_SIZE] = {-1,-1,-1,-1,-1,-1,-1,-1};
	int command_issue_time[BANK_SIZE][NUMBER_OF_COMMANDS]={0};
	int activate_command_flag = TRUE;
	int same_row_flag = TRUE;
	int same_bank_diff_row = TRUE;
	int start_tRCD_flag = FALSE;
	int start_tRP_flag = FALSE;
	int start_tCWL_flag = FALSE;
	int start_tCL_flag = FALSE;
	int tCCD  = TCCD;
	int tRCD 	= TRCD;
	int tRP		= TRP;
	int tWR 	= TWR;
	int tRTP = TRTP;
	int tBURST= TBURST;
	int tRAS 	= TRAS;
	int tCWL = TCWL;
	int tWTR = TWTR;
	int tCAS = TCAS;
	int counter_for_trcd = ZERO;
	int counter_for_write_delay = ZERO;
	int counter_for_read_delay = ZERO;
	int counter_for_PRE = ZERO;
	float wait_time;
	float dram_wait_controller_time=ZERO;
	char s[2]= " ";
	char *token;
	int flag = 3;
	const char input_file[MAX_INPUT_FILENAME_SIZE];
	const char *output_file = "output_dram_commands.txt";
	struct queue *dramqueue;
	//printf("Clock\t\tHex_address\t      Operation\t       Time\n");
  printf("Enter the input file name\n");
  scanf("%s",&input_file);
	FILE *file_cpu = fopen ( input_file, "r" );
	FILE *file_dram = fopen ( output_file, "w" );
	q = initialize_q();
	if ( file_cpu != NULL  && file_dram!= NULL)
	{
		char line [ 128 ]; /* or other suitable maximum line size */
		while ( 1 ) /* read a line */
		{
			if(q->current_queue_size == q->capacity) //check this condition before reading a line from file.
			{
         printf("Queue is Full.\n");
         queue_FULL_flag = TRUE;
         goto READING;
			}
      if(queue_FULL_flag == FALSE)
      {
        if((fgets (line, sizeof(line) , file_cpu ) != NULL ) && new_request_time_less_than_prev == FALSE)
        {
          flag = 3;
          token = strtok(line, s);
          while(token != NULL)
          {
            if(flag == 3)
            {
              strcpy(D.hex_address,token);
            }
            if(flag == 2)
            {
              strcpy(D.operation ,token);
            }
            if(flag == 1)
            {
               D.time = atoi(token);
               enqueue(q,D);
            }
            token = strtok(NULL,s);
            flag--;
          }
        }
      }

      READING:
				if(cpu_clock % CPU_TO_DRAM_CLOCK_RATIO == ZERO  && q->current_queue_size > ZERO )
				{
					dramqueue = rear;
					wait_time = (dramqueue->time)/(float)CPU_TO_DRAM_CLOCK_RATIO;
					dram_wait_controller_time = ceil(wait_time);
					//printf("DRAM time and dram wait time  %d   %.1f		%.1f \n", dram_clock, wait_time,dram_wait_controller_time);
					if(dram_clock >= dram_wait_controller_time )
					{
						current_bank = dramqueue->bank;
						current_row = dramqueue->row;
						column = dramqueue->column;
						if(active_row_in_bank[current_bank] == -1)// Different bank.
						{
							if(activate_command_flag)
							{
								fprintf(file_dram,"%d\t\tACT\t%d\t0x%X\t\n",cpu_clock,current_bank,current_row);
								command_issue_time[current_bank][A] = cpu_clock;
								activate_command_flag = FALSE;
								counter_for_trcd = ZERO;
							}
							if(tRCD <= counter_for_trcd)
							{
								if(tRCD == counter_for_trcd)
								{
									if(!(strcmp(dramqueue->operation,"WRITE") ))//Enters this on WRITE
									{
										fprintf(file_dram,"%d\t\tWR\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
										command_issue_time[current_bank][W] = cpu_clock;
										counter_for_write_delay= ZERO;
										counter_for_read_delay = ZERO;
									}

									else // Enters this on READ
									{
										fprintf(file_dram,"%d\t\tRD\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
										command_issue_time[current_bank][R] = cpu_clock;
										counter_for_write_delay= ZERO;
										counter_for_read_delay = ZERO;
									}
								}
                if(!(strcmp(dramqueue->operation,"WRITE") ))
                {
                  //printf("WRITE\n");
                  if((tCWL+tBURST) <= counter_for_write_delay )
                  {
                    //printf("READ/WRITE completed\n");
                    counter_for_write_delay = -1;
                    counter_for_read_delay = -1;
                    counter_for_PRE = -1;
                    counter_for_trcd = -1;
                    active_row_in_bank[current_bank] = current_row;// present active row in the bank.
                    dequeue(q);
                    cpu_clock--;
                    dram_clock--;
                    activate_command_flag = TRUE;
                  }
                }
                else
                {
                  //printf("READ\n");
                  if(tCAS+tBURST <= counter_for_read_delay)
                  {
                    //printf("READ/WRITE completed\n");
                    counter_for_write_delay = -1;
                    counter_for_read_delay = -1;
                    counter_for_PRE = -1;
                    counter_for_trcd = -1;
                    active_row_in_bank[current_bank] = current_row;
                    dequeue(q);
                    cpu_clock--;
                    dram_clock--;
                    activate_command_flag = TRUE;
                  }
                }
							}
						}
						else if( active_row_in_bank[current_bank] != current_row)//Same bank , different row.
						{
							if(same_bank_diff_row)
							{
								if((cpu_clock - command_issue_time[current_bank][A] >= (tRAS *4))//Read to precharge
									&& (cpu_clock - command_issue_time[current_bank][R] >= (tRTP * 4))//read to precharge
									&& (cpu_clock - command_issue_time[current_bank][W] >= (tCWL+tBURST+tWR) * 4))//Write to Precharge
								{
									fprintf(file_dram,"%d\t\tPRE\t%d\n",cpu_clock,current_bank);
									command_issue_time[current_bank][P] = cpu_clock;
									//printf("PRECHARGING\n");
									//printf("start_tRCD_flag = %d\n", start_tRCD_flag);
									same_bank_diff_row = FALSE;
									counter_for_PRE = ZERO;
									counter_for_trcd = ZERO;
									counter_for_write_delay= ZERO;
									counter_for_read_delay = ZERO;
									start_tRP_flag = TRUE;
								}
							}

							if(tRP <= counter_for_PRE && start_tRP_flag == TRUE)
							{
								if(tRP == counter_for_PRE)
								{
									fprintf(file_dram,"%d\t\tACT\t%d\t0x%X\t\n",cpu_clock,current_bank,current_row);
									//printf("ACTIVATING .\n");
									command_issue_time[current_bank][A] = cpu_clock;
									counter_for_trcd = ZERO;
									start_tRCD_flag = TRUE;
								}
							}
							//printf("tRCD = %d, counter_for_trcd: %d  start_tRCD_flag = %d \n",tRCD,counter_for_trcd, start_tRCD_flag);

              if((tRCD <= counter_for_trcd) && (start_tRCD_flag == TRUE))
              {
                //printf("tRCD completed\n");
                if(tRCD == counter_for_trcd)
                {
                  if(!(strcmp(dramqueue->operation,"WRITE") ))//Enters this on WRITE
                  {
                    fprintf(file_dram,"%d\t\tWR\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
                    command_issue_time[current_bank][W] = cpu_clock;
                    counter_for_write_delay= ZERO;
                    counter_for_read_delay = ZERO;
                    start_tCWL_flag = TRUE;
                  }

                  else // Enters this on READ
                  {
                    fprintf(file_dram,"%d\t\tRD\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
                    command_issue_time[current_bank][R] = cpu_clock;
                    counter_for_write_delay= ZERO;
                    counter_for_read_delay = ZERO;
                    start_tCWL_flag = TRUE;
                  }
                }

                if(!(strcmp(dramqueue->operation,"WRITE") ))
                {

                  //printf("WRITE\n");
                  if((tCWL+tBURST) <= counter_for_write_delay && start_tCWL_flag == TRUE )
                  {
                    //printf("READ/WRITE completed\n");
                    counter_for_write_delay = -1;
                    counter_for_read_delay = -1;
                    counter_for_PRE = -1;
                    counter_for_trcd = -1;
                    active_row_in_bank[current_bank] = current_row;// present active row in the bank.
                    dequeue(q);
                    cpu_clock--;
                    dram_clock--;
                    start_tRCD_flag = FALSE;
                    start_tRP_flag = FALSE;
                    same_bank_diff_row = TRUE;
                    same_row_flag = TRUE;
                    activate_command_flag = TRUE;
                    start_tCWL_flag = FALSE;
                  }
                }
                else
                {
                  //printf("READ\n");
                  if((tCAS+tBURST) <= counter_for_read_delay && start_tCWL_flag == TRUE)
                  {
                    //printf("READ/WRITE completed\n");
                    counter_for_write_delay = -1;
                    counter_for_read_delay = -1;
                    counter_for_PRE = -1;
                    counter_for_trcd = -1;
                    active_row_in_bank[current_bank] = current_row;
                    dequeue(q);
                    cpu_clock--;
                    dram_clock--;
                    start_tRP_flag = FALSE;
                    start_tRCD_flag = FALSE;
                    same_bank_diff_row = TRUE;
                    activate_command_flag = TRUE;
                    same_row_flag = TRUE;
                    start_tCWL_flag = FALSE;
                  }
                }

              }
						}
						else if( active_row_in_bank[current_bank] == current_row)//Same bank, same row.
						{
							if(same_row_flag)
							{
								if(!(strcmp(dramqueue->operation,"WRITE")))
								{
									if((cpu_clock - command_issue_time[current_bank][W] >= (tCCD *4 ))
											&& (cpu_clock - command_issue_time[current_bank][R] >= ((tCAS+tBURST+2-tCWL) *4)))
									{
										fprintf(file_dram,"%d\t\tWR\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
										command_issue_time[current_bank][W] = cpu_clock;
										counter_for_write_delay= ZERO;
										counter_for_read_delay = ZERO;
										same_row_flag = FALSE;
										start_tCWL_flag = TRUE;
									}
								}
								else
								{
									if((cpu_clock - command_issue_time[current_bank][R] >= (tCCD * 4))
										&& (cpu_clock - command_issue_time[current_bank][W] >= ((tCWL+tBURST+tWTR)*4)))
									{
										fprintf(file_dram,"%d\t\tRD\t%d\t0x%X\t\n",cpu_clock,current_bank,column);
										command_issue_time[current_bank][R] = cpu_clock;
										counter_for_write_delay= ZERO;
										counter_for_read_delay = ZERO;
										same_row_flag = FALSE;
										start_tCL_flag = TRUE;
									}
								}
							}
							if(!(strcmp(dramqueue->operation,"WRITE") ))
							{
								if((tCWL+tBURST) <= counter_for_write_delay && start_tCWL_flag == TRUE)
								{
									//printf("READ/WRITE completed\n");
									counter_for_write_delay = -1;
									counter_for_read_delay = -1;
									counter_for_PRE = -1;
									counter_for_trcd = -1;
									active_row_in_bank[current_bank] = current_row;// present active row in the bank.
									dequeue(q);
									cpu_clock--;
									dram_clock--;
									same_row_flag = TRUE;
									activate_command_flag = TRUE;
									start_tCWL_flag = FALSE;
								}
							}
							else
							{
								//printf("READ\n");
								if((tCAS+tBURST) <= counter_for_read_delay && start_tCL_flag == TRUE)
								{
									//printf("READ/WRITE completed\n");
									counter_for_read_delay = -1;
									counter_for_write_delay = -1;
									counter_for_PRE = -1;
									counter_for_trcd = -1;
									active_row_in_bank[current_bank] = current_row;
									dequeue(q);
									//printf("queue size = %d\n",q->current_queue_size);
									cpu_clock--;
									dram_clock--;
									same_row_flag = TRUE;
									activate_command_flag = TRUE;
									start_tCL_flag = FALSE;
								}
							}
						}
						else
            {
								//printf("Doing nothing here\n");
            }
					}
					dram_clock++;
					counter_for_PRE++;
					counter_for_trcd++;
					counter_for_write_delay++;
					counter_for_read_delay++;
					if(queue_empty_flag == TRUE) break;
				}
      cpu_clock++;
			if(queue_empty_flag == TRUE) break;
		}
	}
	else
	{
		printf( "\aSomething Wrong happened :( Input file not found \n" ); /* why didn't the file_cpu open? */
	}

		fclose(file_cpu);
		fclose(file_dram);
	return 0;
	}

