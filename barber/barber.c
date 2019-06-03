#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#define CHAIRS 5

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *arry;
};

//Semaphore Id;
static int customer = 0, barber = 0, mutex = 0, share = 0, share2 = 0;
static int systemControl = 0;
int* waiting, * over;
unsigned int CUSTOMERS = 10, c_time = 0, b_time = 0;

//function declare
static int sem_P(int id);
static int sem_V(int id);

int main(int argc, char** argv) {
    //Customers
    if(argc > 1) {
        CUSTOMERS = atoi(argv[1]);
    } else {
        fprintf(stdout, "可以添加第1个参数：设置客人总数。\n");
    }
    //Randoms
    srand(time(NULL));
    //Wait Times
    if(argc > 2) {
        c_time = atoi(argv[2]);
    } else {
        fprintf(stdout, "可以添加第2个参数：设置客人到来时间基数（实际到来时间为基数+随机数（1～3））。\n");
    }
    if(argc > 3) {
        b_time = atoi(argv[3]);
    } else {
        fprintf(stdout, "可以添加第3个参数：设置理发时间基数（实际理发时间为基数+随机数（1～3））。\n");
    }

	//Create Semaphore
	customer = semget((key_t)1, 1, 0666 | IPC_CREAT);
	barber = semget((key_t)2, 1, 0666 | IPC_CREAT);
	mutex = semget((key_t)3, 1, 0666 | IPC_CREAT);
	systemControl = semget((key_t)4, 1, 0666 | IPC_CREAT);
	//Initialize Semaphore
	union semun sem_union, sem_union2;
	sem_union.val = 1;
	sem_union2.val = 0;
	if(
			semctl(customer, 0, SETVAL, sem_union2) == -1 ||
			semctl(barber, 0, SETVAL, sem_union2) == -1 ||
			semctl(mutex, 0, SETVAL, sem_union) == -1 ||
			semctl(systemControl, 0, SETVAL, sem_union2) == -1
	) {
		fprintf(stderr, "Initialize Semaphore Failed!\n");
		exit(EXIT_FAILURE);
		return -1;
	}
	//Create Shared Memory
	share = shmget(IPC_PRIVATE, 64, 0666);
	if(share < 0) {
		if(
				semctl(customer, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(barber, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(mutex, 0, IPC_RMID, sem_union) == -1 ||
				semctl(systemControl, 0, IPC_RMID, sem_union2)
		) {
			fprintf(stderr, "Delete Semaphore Failed!");
		}
	    fprintf(stderr, "Create Shared Memory Failed!\n");
	    exit(EXIT_FAILURE);
	    return -1;
	}
	share2 = shmget(IPC_PRIVATE, 64, 0666);
	if(share2 < 0) {
		if(
				semctl(customer, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(barber, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(mutex, 0, IPC_RMID, sem_union) == -1 ||
				semctl(systemControl, 0, IPC_RMID, sem_union2)
		) {
			fprintf(stderr, "Delete Semaphore Failed!");
		}
	    fprintf(stderr, "Create Shared Memory 2 Failed!\n");
	    exit(EXIT_FAILURE);
	    return -1;
	}
    //Get Shared Memory
    waiting = shmat(share, 0, 0);
    if(waiting == (void*)-1) {
        fprintf(stderr, "Get Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    *waiting = 0;
    over = shmat(share2, 0, 0);
    if(over == (void*)-1) {
        fprintf(stderr, "Get Shared Memory 2 Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    *over = CUSTOMERS;
    //Release Shared Memory
    if(shmdt(waiting) < 0) {
        fprintf(stderr, "Release Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    if(shmdt(over) < 0) {
        fprintf(stderr, "Release Shared Memory 2 Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
	int pid = fork();
	if(pid == 0) {  //Child
	    for(int i = 0; i < CUSTOMERS; i++) {
            sleep(c_time + rand() % 3 + 1);
	        int pid = fork();
	        if(pid == 0) {  //Customer
	            //Get Shared Memory
	            waiting = shmat(share, 0, 0);
	            if(waiting == (void*)-1) {
	                fprintf(stderr, "Child: Get Shared Memory Failed!\n");
	                exit(EXIT_FAILURE);
	                return -1;
	            }
                over = shmat(share2, 0, 0);
                if(over == (void*)-1) {
                    fprintf(stderr, "Child: Get Shared Memory 2 Failed!\n");
                    exit(EXIT_FAILURE);
                    return -1;
                }
	            
	            //Customer
                sem_P(mutex);
                if(*waiting < CHAIRS) {
                    (*waiting)++;
                    fprintf(stdout, "Customer: New customer!%d\n", *waiting);
                    sem_V(customer);
                    sem_V(mutex);
                    sem_P(barber);
                } else {
                    fprintf(stdout, "\tCustomer: Customer Full!%d\n", *waiting);
                    (*over)--;
                    sem_V(mutex);
                }
	            
	            //Release Shared Memory
	            if(shmdt(waiting) < 0) {
	                fprintf(stderr, "Child: Release Shared Memory Failed!\n");
	                exit(EXIT_FAILURE);
	                return -1;
	            }
                if(shmdt(over) < 0) {
                    fprintf(stderr, "Child: Release Shared Memory 2 Failed!\n");
                    exit(EXIT_FAILURE);
                    return -1;
                }
	            //wait Parent
	            sem_P(systemControl);
	            break;
	        }
	    }
	} else {  //Parent
	
	    //Get Shared Memory
	    waiting = shmat(share, 0, 0);
	    if(waiting == (void*)-1) {
	        fprintf(stderr, "Parent: Get Shared Memory Failed!\n");
	        exit(EXIT_FAILURE);
	        return -1;
	    }
        over = shmat(share2, 0, 0);
        if(over == (void*)-1) {
            fprintf(stderr, "Child: Get Shared Memory 2 Failed!\n");
            exit(EXIT_FAILURE);
            return -1;
        }
	    
	    //Barber
	    while(1) {
	        if(*waiting == 0) {
	            fprintf(stdout, "\t\tBarber: I wanna sleep!!!%d\n", *waiting);
	        }
	        sem_P(customer);
	        sem_P(mutex);
	        (*waiting)--;
	        sem_V(barber);
	        sem_V(mutex);
	        //Barbering
	        fprintf(stdout, "\t\t\tBarber: Barbering......%d\n", *waiting);
	        sleep(b_time + rand() % 3 + 1);
	        fprintf(stdout, "\t\t\tBarber: Finished barber!%d\n", *waiting);
	        if(--(*over) <= 0) {
	            fprintf(stdout, "顾客全部服务完毕！\n");
	            break;
	        }
	    }
	    
		waitpid(pid, NULL, 0);
		//signal Children
		for(int i = 0; i < CUSTOMERS; i++) {
		    sem_V(systemControl);
		}
		//Delete Semaphore
		if(
				semctl(customer, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(barber, 0, IPC_RMID, sem_union2) == -1 ||
				semctl(mutex, 0, IPC_RMID, sem_union) == -1 ||
				semctl(systemControl, 0, IPC_RMID, sem_union) == -1
		) {
			fprintf(stderr, "Parent: Delete Semaphore Failed!");
	        exit(EXIT_FAILURE);
	        return -1;
		}
	    //Release Shared Memory
	    if(shmdt(waiting) < 0) {
	        fprintf(stderr, "Parent: Release Shared Memory Failed!\n");
	        exit(EXIT_FAILURE);
	        return -1;
	    }
        if(shmdt(over) < 0) {
            fprintf(stderr, "Child: Release Shared Memory 2 Failed!\n");
            exit(EXIT_FAILURE);
            return -1;
        }
		//Delete Shared Memory
		if(shmctl(share, IPC_RMID, NULL) == -1) {
		    fprintf(stderr, "Parent: Delete Shared Memory Failed!\n");
		    exit(EXIT_FAILURE);
		    return -1;
		}
		if(shmctl(share2, IPC_RMID, NULL) == -1) {
		    fprintf(stderr, "Parent: Delete Shared Memory 2 Failed!\n");
		    exit(EXIT_FAILURE);
		    return -1;
		}
	}
	exit(EXIT_SUCCESS);
	return 0;
}

static int sem_P(int id) {
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(id, &sem_b, 1) == -1) {
		fprintf(stderr, "Semaphore_P() Failed!\n");
		return 0;
	}
	return 1;
}

static int sem_V(int id) {
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = 1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(id, &sem_b, 1) == -1) {
		fprintf(stderr, "Semaphore_V() Failed!\n");
		return 0;
	}
	return 1;
}
