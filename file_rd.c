#include "utils.h"

#define SWAP(a,b) { int tmp = *a; *a = *b; *b = tmp;}
#define BLOCK_SIZE                  (32 * 1024)
//#define BLOCK_SIZE                  (64)
///#define FLAGS                       O_RDONLY
#define FLAGS                       (O_RDONLY | O_SYNC  | O_DIRECT) 

struct share_it {
    int         fd;
    int*        offsets;
    char*       buf;
    char*       filename;
    size_t      size;
    size_t      block_size;
    timestamp   duration;   
};

int dummy_call(char* buf) {
    return (buf[0] == '0');
}

/*
 * Randomize an array.
 */
void randomize(int *array, int size) {
    int i;
    for( i = 0 ; i < size; i++) {
        array[i] = i;
    }
    for( i = size - 1 ; i > 0; i--) {
        int index = rand() % i;
        SWAP((array + index) , (array + i));
    }
    return;
}
 
bool do_sequential(struct share_it* my_state) {
    size_t size         = my_state->size;
    size_t block_size   = my_state->block_size;
    timestamp start     = 0;
    timestamp end       = 0;
    my_state->duration  = 0;
    int bytes           = 0;

    while (size > 0) {
        if (size < block_size)
            return true;
        RDTSCP(start);
        bytes = read(my_state->fd, my_state->buf, block_size);
        RDTSCP(end);
        dummy_call(my_state->buf);
        my_state->duration  += (end - start);
        if (bytes <= 0)
            return false;
        size -= block_size;
    }
    return true;    
}

bool do_random(struct share_it* my_state) {
    int fd              = my_state->fd;
    size_t size         = my_state->size;
    size_t block_size   = my_state->block_size;
    timestamp start     = 0;
    timestamp end       = 0;
    my_state->duration  = 0;
    int bytes           = 0;

    int i = 0;
    while (size > 0) {
        if (size < block_size)
            return true;
        if (lseek(fd, my_state->offsets[i] * block_size, SEEK_SET) == -1) {
            printf("Could not seek to start  of file %s\n", my_state->filename);
            exit(1);
        }
        RDTSCP(start);
        bytes = read(my_state->fd, my_state->buf, block_size);
        RDTSCP(end);
        my_state->duration  += (end - start);
        if (bytes <= 0)
            return false;
        size -= block_size;
        i++;
    }
    return true;    
}

bool do_open_seek_read(struct share_it* my_state) {
    int fd              = my_state->fd;
    size_t size         = my_state->size;
    size_t block_size   = my_state->block_size;
    timestamp start     = 0;
    timestamp end       = 0;
    my_state->duration  = 0;
    int bytes           = 0;

    int i = 0;
    while (size > 0) {
        if (size < block_size)
            return true;
        RDTSCP(start);
        int new_fd = open(my_state->filename, FLAGS);
        if(new_fd == -1) {
            int err = errno;
            printf("2: Could not open file descriptor for file %s with error %d\n",
                    my_state->filename, err);
            exit(1);
        }
        if (lseek(new_fd, my_state->offsets[i] * block_size, SEEK_SET) == -1) {
            printf("Could not seek to start  of file %s\n", my_state->filename);
            exit(1);
        }
        bytes = read(new_fd, my_state->buf, block_size);
        close(fd);
        RDTSCP(end);
        my_state->duration  += (end - start);
        if (bytes <= 0)
            return false;
        size -= block_size;
        i++;
    }
    return true;    
}

void test(char *filename) {
    int fd;
    struct stat sb;
    struct share_it state;

#ifdef _GNU_SOURCE    
    //Set cpu affinity
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &set) == -1) {
        printf("Affinity Failed!\n");
        exit(1);
    }
#endif

    fd = open(filename, FLAGS);
    if(fd == -1) {
        printf("1 : Could not open file descriptor for file %s\n", filename);
        exit(1);
    }

    if (fstat(fd,&sb) == -1) {
        printf("File stat failed for file %s\n", filename);
        exit(1);
    }

    char *buf = (char *)malloc(BLOCK_SIZE);

    int pages = sb.st_size / BLOCK_SIZE;
    int *index = (int *)malloc(sizeof(int) * pages);
    randomize(index, pages);

    // Prepare the state.
    state.fd         = fd;
    state.offsets    = index;
    state.buf        = buf;
    state.filename   = filename;
    state.size       = sb.st_size;
    state.block_size = BLOCK_SIZE;
    state.duration   = 0;

    timestamp duration  = 0;

    bool read           = true;

    int i = 0;

    for (i = 0; read && i < LOOP_COUNTER; i++) {
         if (lseek(fd, 0, SEEK_SET) == -1) {
            printf("Could not seek to start  of file %s\n", filename);
            exit(1);
        }
        //read = do_sequential(&state);
        read = do_random(&state);
        //read = do_open_seek_read(&state);
        duration += state.duration;
    }

    free(index);
    free(buf);
    close(fd);

    int number_of_blocks = pages * LOOP_COUNTER;

    printf("%ld\t%lf\n", (long int)sb.st_size, (double)(duration)/number_of_blocks);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./file_rd.o <filename>\n");
        exit(1);
    }

    test(argv[1]);
}
