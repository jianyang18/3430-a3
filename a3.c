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
 * constants
 * --------------------------------- */
#define FAT32_MASK      0x0FFFFFFFu


/* -----------------------------------
 * global vals
 * --------------------------------- */
static int image_fd = -1;   // disk image file descriptor
static fat32BS image_bs;    // boot sector, (from fat32.h provided)
static uint32_t first_data_sec;  // first sector of data region
static uint32_t bytes_per_clus; // bytes in one cluster
static uint32_t fat_start_byte; // byte offset of FAT region


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

static void copy_name(char *dst, const char *src, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        dst[i] = src[i];
    }

    // strip trailing spaces by walking back and replacing with \0
    while (i > 0 && dst[i - 1] == ' ') {
        i--;
    }
    dst[i] = '\0';
}

/*
*   look up the FAT entry for cluster N (masked to 28 bits)
*/
static uint32_t fat_entry(uint32_t cluster)
{
    off_t offset = (off_t)fat_start_byte + (off_t)cluster * 4;  // each FAT entry is 4 bytes
    
    uint32_t entry  = 0;

    disk_read(offset, &entry, sizeof(entry));   // seek to that byte offset and read 4 bytes into entry
    
    return entry & FAT32_MASK;  // need to mask off top 4 bits -> FAT32 entries are 28 btis, should ignore
}



/* -----------------------------------
 * function: load boot sector
 *      read entire boot sector, store into struct image_bs
 * --------------------------------- */
static void load_boot_sector(void)
{
    // step 1: read the disk (from byte 0)
    disk_read(0, &image_bs, sizeof(image_bs));  

    // step 2: validate whether boot sector is read coccectly
        // BS_SigA: sector [510]    -> need to be 0x55
        // BS_SigB: sector [511]    -> need to be 0xAA 
    if (image_bs.BS_SigA != 0x55 || image_bs.BS_SigB != 0xAA)
    {
        fprintf(stderr, "Invalid boot sector signature: 0x%02X 0x%02X\n",image_bs.BS_SigA, image_bs.BS_SigB);

        exit (EXIT_FAILURE);
    }

    // extract info from bs, update globals
    uint32_t fat_sz = image_bs.BPB_FATSz32;     // # of sectors in FAT table

    first_data_sec = image_bs.BPB_RsvdSecCnt + (image_bs.BPB_NumFATs * fat_sz);     // skips reserved region and all FAT tables
    bytes_per_clus = (uint32_t)image_bs.BPB_SecPerClus * image_bs.BPB_BytesPerSec; 
    fat_start_byte = (uint32_t)image_bs.BPB_RsvdSecCnt * image_bs.BPB_BytesPerSec;  // skips reserved region
}

/* -----------------------------------
 * COMMAND 1: info
 *  prints drive name, free space, usable space, cluster size
 * --------------------------------- */
static void print_info()
{
    off_t fsinfo_offset = (off_t)image_bs.BPB_FSInfo * image_bs.BPB_BytesPerSec;

    // read FsInfo sector into the struct
    struct FSInfo fsinfo;
    disk_read(fsinfo_offset, &fsinfo, sizeof(fsinfo));

    // validate signature bits
    int fsinfo_valid = (fsinfo.lead_sig == 0x41615252u && fsinfo.signature == 0x61417272u);

    // 1. get Volume label from bs struct
    char volume_label[BS_VolLab_LENGTH + 1]; // vol_lab length is 11, add 1 for null terminator
    copy_name(volume_label, image_bs.BS_VolLab, BS_VolLab_LENGTH);

    // 2. get OEM name from bs struct
    char oem_name[BS_OEMName_LENGTH + 1];
    copy_name(oem_name, image_bs.BS_OEMName, BS_OEMName_LENGTH);

    // 3. get free space
    uint32_t free_clusters = fsinfo.free_count;

    if (free_clusters == 0xFFFFFFFFu)
    {
        // ===== in this case, free_clusters is unknown, need to traverse the fat table =====
        free_clusters = 0;

        // 1) calc total clusters
        // count of data clusters = (TotSec - FirstDataSec) / SecPerClus
        uint32_t data_clusters = (image_bs.BPB_TotSec32 - first_data_sec) / image_bs.BPB_SecPerClus;

        // 2) traverse (indexed at 2)
        for (uint32_t i = 2; i < data_clusters + 2; i++)
        {
            if (fat_entry(i) == 0)
            {
                free_clusters++;
            }
        }
    }

    uint64_t free_bytes = (uint64_t)free_clusters * bytes_per_clus;
    uint64_t free_kb = free_bytes / 1024;   // to kb

    // 4. get total space
    uint64_t total_bytes = (uint64_t)image_bs.BPB_TotSec32 * image_bs.BPB_BytesPerSec;
    uint64_t total_kb = total_bytes / 1024;

    // 5. get usable space (data region)
    uint64_t data_sectors = image_bs.BPB_TotSec32 - first_data_sec;
    uint64_t usable_kb = (data_sectors * image_bs.BPB_BytesPerSec) / 1024;

    // 6. get cluster size in number of sectors
    uint32_t clus_sectors = image_bs.BPB_SecPerClus;
    uint32_t clus_kb = bytes_per_clus / 1024;

    printf("Volume label: %s\n", volume_label);
    printf("OEM Name: %s\n", oem_name);
    printf("FSinfo is as %d\n", fsinfo_valid ? 1 : 0);
    printf("Free space is %llukB\n", (unsigned long long)free_kb);
    printf("Total space is %llukB\n", (unsigned long long)total_kb);
    printf("Total useable space %llukB\n", (unsigned long long)usable_kb);
    printf("Cluster size: %u sectors, %u kB\n", clus_sectors, clus_kb);


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

    printf("\n========== Reading disk image==========\n\n");

    load_boot_sector();

    // handle different command (3)
    if (strcmp(command, "info") == 0)
    {
        print_info();
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