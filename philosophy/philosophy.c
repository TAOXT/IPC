#include <stdlib.h>
#include <stdio.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define THINKING 0
#define HUNGRY 1
#define EATING 2
#define LEFT(i) (i + N - 1) % N
#define RIGHT(i) (i + 1) % N

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *arry;
};

//Semaphore Id;
static int philosophy = 0, mutex = 0, share = 0, share2 = 0;
int* state, *systemControl;
int N = 5;

//function declare
static int sem_P(int id, int index);
static int sem_V(int id, int index);
inline static void take_chopsticks(int i);
inline static void put_chopsticks(int i);
inline static void test(int i);
inline static void console_pause();

int main(int argc, char** argv) {
    //Philosophies
    if(argc > 1) {
        N = atoi(argv[1]);
    } else {
        fprintf(stdout, "可以添加第1个参数：设置哲学家人数。\n");
    }
    //Randoms
    srand(time(NULL));
    
	//Create Semaphore
	philosophy = semget((key_t)1, N, 0666 | IPC_CREAT);
	mutex = semget((key_t)2, 1, 0666 | IPC_CREAT);
	//Initialize Semaphore
	union semun sem_union, sem_union2;
	sem_union.val = 1;
	sem_union2.val = 0;
	for(int i = 0; i < N; i++) {
	    if(semctl(philosophy, i, SETVAL, sem_union) == -1) {
		    fprintf(stderr, "Initialize Semaphore Failed!\n");
		    exit(EXIT_FAILURE);
		    return -1;
	    }
	}
	if(semctl(mutex, 0, SETVAL, sem_union) == -1) {
		fprintf(stderr, "Initialize Semaphore Failed!\n");
		exit(EXIT_FAILURE);
		return -1;
	}
	//Create Shared Memory
	share = shmget(IPC_PRIVATE, sizeof(int) * N, 0666);
	if(share < 0) {
		if(
		    semctl(philosophy, 0, IPC_RMID, sem_union2) == -1 ||
		    semctl(mutex, 0, IPC_RMID, sem_union) == -1
		) {
			fprintf(stderr, "Delete Semaphore Failed!");
		}
	    fprintf(stderr, "Create Shared Memory Failed!\n");
	    exit(EXIT_FAILURE);
	    return -1;
	}
	share2 = shmget(IPC_PRIVATE, sizeof(int), 0666);
	if(share2 < 0) {
		if(
		    semctl(philosophy, 0, IPC_RMID, sem_union2) == -1 ||
		    semctl(mutex, 0, IPC_RMID, sem_union) == -1
		) {
			fprintf(stderr, "Delete Semaphore Failed!");
		}
	    fprintf(stderr, "Create Shared Memory Failed!\n");
	    exit(EXIT_FAILURE);
	    return -1;
	}
    //Get Shared Memory
    state = shmat(share, 0, 0);
    if(state == (void*)-1) {
        fprintf(stderr, "Get Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    {  //Block
        int _arr[N];
        memcpy(state, _arr, sizeof(int) * N);
    }
    for(int i = 0; i < N; i++) {
        state[i] = THINKING;
    }
    systemControl = shmat(share2, 0, 0);
    if(systemControl == (void*)-1) {
        fprintf(stderr, "Get Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    (*systemControl) = 0;
    //Release Shared Memory
    if(shmdt(state) < 0) {
        fprintf(stderr, "Release Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    if(shmdt(systemControl) < 0) {
        fprintf(stderr, "Release Shared Memory Failed!\n");
        exit(EXIT_FAILURE);
        return -1;
    }
    char tab[N];
	int pid = fork();
	if(pid == 0) {  //Child
	    int children[5];
	    for(int i = 0; i < N; i++) {
	        children[i] = fork();
	        if(children[i] == 0) {  //Philosophy
                //Get Shared Memory
                state = shmat(share, 0, 0);
                if(state == (void*)-1) {
                    fprintf(stderr, "Get Shared Memory Failed!\n");
                    exit(EXIT_FAILURE);
                    return -1;
                }
                systemControl = shmat(share2, 0, 0);
                if(systemControl == (void*)-1) {
                    fprintf(stderr, "Get Shared Memory Failed!\n");
                    exit(EXIT_FAILURE);
                    return -1;
                }
                //tables
                int j = 0;
                for(;j < i; j++) {
                    tab[j] = '\t';
                }
                tab[j] = '\0';
                //Philosophy
	            while(1) {
	                if((*systemControl) > 0) {
	                    break;
	                }
	                fprintf(stdout, "%sPhilosophy %d is thinking!\n", tab, i + 1);
	                sleep(rand() % 5 + 1);  //Think
	                fprintf(stdout, "%sPhilosophy %d is hungry!\n", tab, i + 1);
	                take_chopsticks(i);
	                fprintf(stdout, "%sPhilosophy %d is eating!\n", tab, i + 1);
	                sleep(rand() % 3 + 1);  //Eat
	                put_chopsticks(i);
	            }
	            fprintf(stdout, "Philosophy %d was left!\n", i + 1);
	            break;
	        } else if(i == N - 1) {  //Parent Last Loop
	            for(int i = 0; i < N; i++) {
	                waitpid(children[i], NULL, 0);
	            }
	        }
	    }
	} else {  //Parent
	    //Wait for key down
	    console_pause();
        //Get Shared Memory
        systemControl = shmat(share2, 0, 0);
        if(systemControl == (void*)-1) {
            fprintf(stderr, "Child: Get Shared Memory Failed!\n");
            exit(EXIT_FAILURE);
            return -1;
        }
		//signal Children
        (*systemControl) = 1;
	    //Release Shared Memory
	    if(shmdt(systemControl) < 0) {
	        fprintf(stderr, "Parent: Release Shared Memory Failed!\n");
	        exit(EXIT_FAILURE);
	        return -1;
	    }
		waitpid(pid, NULL, 0);
		//Delete Semaphore
		if(
		    semctl(philosophy, 0, IPC_RMID, sem_union2) == -1 ||
		    semctl(mutex, 0, IPC_RMID, sem_union) == -1
		) {
			fprintf(stderr, "Delete Semaphore Failed!");
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

inline static void take_chopsticks(int i) {
    sem_P(mutex, 0);
    state[i] = HUNGRY;
    test(i);
    sem_V(mutex, 0);
    sem_P(philosophy, i);
}

inline static void put_chopsticks(int i) {
    sem_P(mutex, 0);
    state[i] = THINKING;
    test(LEFT(i));
    test(RIGHT(i));
    sem_V(mutex, 0);
}

inline static void test(int i) {
    if(
        state[i] == HUNGRY &&
        state[LEFT(i)] != EATING &&
        state[RIGHT(i)] != EATING
    ) {
        state[i] = EATING;
        sem_V(philosophy, i);
    }
}

static int sem_P(int id, int index) {
	struct sembuf sem_b;
	sem_b.sem_num = index;
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(id, &sem_b, 1) == -1) {
		fprintf(stderr, "Semaphore_P() Failed!\n");
		return 0;
	}
	return 1;
}

static int sem_V(int id, int index) {
	struct sembuf sem_b;
	sem_b.sem_num = index;
	sem_b.sem_op = 1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(id, &sem_b, 1) == -1) {
		fprintf(stderr, "Semaphore_V() Failed!\n");
		return 0;
	}
	return 1;
}

inline static void console_pause() {
    fprintf(stdout, "按任意键退出！\n");
    struct termios te;
    int ch;
    tcgetattr( STDIN_FILENO,&te);
    te.c_lflag &=~( ICANON|ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&te);
    tcflush(STDIN_FILENO,TCIFLUSH);
    fgetc(stdin) ; 
    te.c_lflag |=( ICANON|ECHO);
    tcsetattr(STDIN_FILENO,TCSANOW,&te);
}

