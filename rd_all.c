#include "utils.h"

#define SWAP(a,b) { int tmp = *a; *a = *b; *b = tmp;}
//#define FLAGS                       O_RDONLY
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

bool do_open_read_close(struct share_it* my_state) {
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

   //     printf("3: fd is %d\n", new_fd);
        
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

    int blk_size = sb.st_size;

    char *buf = (char *)malloc(blk_size);

    int pages = sb.st_size / blk_size;

    // Prepare the state.
    state.fd         = fd;
    state.offsets    = NULL;
    state.buf        = buf;
    state.filename   = filename;
    state.size       = sb.st_size;
    state.block_size = blk_size;
    state.duration   = 0;

    timestamp duration  = 0;

    bool read           = true;

    int i = 0;

 //   printf("1: fd is %d\n", fd);

    for (i = 0; read && i < LOOP_COUNTER; i++) {
        if (lseek(fd, 0, SEEK_SET) == -1) {
            int err = errno;
            printf("2: fd is %d, errno is %d\n", fd, errno);
            printf("Could not seek to start  of file %s\n", filename);
            exit(1);
        }
        read = do_sequential(&state);
        //read = do_open_read_close(&state);
        duration += state.duration;
    }

    free(buf);
    close(fd);

    int number_of_blocks = pages * LOOP_COUNTER;

    printf("%ld\t%lf\n", (long int)sb.st_size, (double)(duration)/number_of_blocks);

}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: ./rd_all <filename>\n");
        exit(1);
    }
    
    test(argv[1]);
}
