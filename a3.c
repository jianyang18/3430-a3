/*
*   a2.c
*   
*   NAME:               Jian Yang
*   STUDENT NUMBER:     8000293
*   COURSE:             COMP 3430, SECTION: A01
*   INSTRUCTOR:         Dr. Saulo dos Santos
*   ASSIGNMENT:         assignment #2
*   REMARKS:            Implement a simulator to mimic a multi-threaded 
*                       CPU scheduler that follows the rules of MLFQ schedulling policy.
*   
*/

#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "fat32.h"



/* -----------------------------------
 * global vals
 * --------------------------------- */
static int image_fd = -1;   // disk image file descriptor
static fat32BS image_bs;    // boot sector, (from fat32.h provided)


/* -----------------------------------
 * helpers
 * --------------------------------- */
static void disk_read(off_t offset, void *buf, size_t size)
{
    // lseek to move the file reader to exact offset value
    if (lseek(image_fd, offset, SEEK_SET) == (off_t)-1)
    {
        fprintf(stderr, "Error seeking the offset, exisiting ...\n");

        exit(EXIT_FAILURE);
    }

    ssize_t result = read(image_fd, buf, size);

    // if result < 0 -> read on error, if != size -> got fewer bytes than requested
    if (result < 0 || (size_t)result != size)
    {
        fprintf(stderr, "Error reading the disk, exisiting ...\n");

        exit(EXIT_FAILURE);

    }

}



/* -----------------------------------
 * function: load boot sector
 * 
 * --------------------------------- */
static load_boot_sector(void)
{
    // step 1: read the disk
    disk_read(0, &image_bs, sizeof(image_bs));  

    // validate whether boot sector is read coccectly
    if (image_bs.BS_SigA != 0x55 || image_bs.BS_SigB != 0xAA)
    {
        fprintf(stderr, "Invalid boot sector signature: 0x%02X 0x%02X\n",image_bs.BS_SigA, image_bs.BS_SigB);

        return (EXIT_FAILURE);
    }
}

/* -----------------------------------
 * main()
 * --------------------------------- */
int main(int argc, char *argv[])
{
    // handle args
    if (argc < 3) {
        fprintf(stderr, "Error reading command, existing ...\n");

        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];
    const char *command = argv[2];

    // open disk image
    image_fd = open(image_path, O_RDONLY);
    
    if (image_fd < 0)
    {
        fprintf(stderr, "Error reading disk file, existing ...\n");

        return EXIT_FAILURE;
    }

    printf("\nReading disk image.\n");

    load_boot_sector();

    // handle different command (3)
    if (strcmp(command, "info") == 0)
    {
        cmd_print_info();
    }
    else if (strcmp(command, "list") == 0)
    {
        printf("list");
    }
    else if (strcmp(command, "get") == 0)
    {
        printf("get");
    }
    else {
        fprintf(stderr, "Unknown command: %s, existing ...\n", command);
        close(image_fd);
        
        return EXIT_FAILURE;
    }



    printf("\n=====================================\n");
    printf("   |Program completed normally.|\n");
    printf("=====================================\n");

    return EXIT_SUCCESS;
    
}