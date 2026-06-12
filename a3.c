/*
*   a3.c
*   
*   NAME:               Jian Yang
*   STUDENT NUMBER:     8000293
*   COURSE:             COMP 3430, SECTION: A01
*   INSTRUCTOR:         Dr. Saulo dos Santos
*   ASSIGNMENT:         assignment #3
*   REMARKS:            Reads a FAT32 formatted disk image and supports
*                       three commands: info (drive metadata), list 
*                       (directory tree), and get (extract a file).
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
#define FAT32_MASK 0x0FFFFFFFu
#define FAT32_BAD 0x0FFFFFF7u
#define FAT32_EOC_MIN 0x0FFFFFF8u
#define DIR_ENTRY_SIZE 32



/* -----------------------------------
 * global vals
 * --------------------------------- */
static int image_fd = -1;   // disk image file descriptor
static fat32BS image_bs;    // boot sector, (from fat32.h provided)
static uint32_t first_data_sec;  // first sector of data region
static uint32_t bytes_per_clus; // bytes in one cluster
static uint32_t fat_start_byte; // byte offset of FAT region

/* -----------------------------------
 * struct
 * --------------------------------- */
struct list_state {
    int depth;
};


/* -----------------------------------
 * helpers
 * --------------------------------- */
static void disk_read(off_t offset, void *buf, size_t size)
{
    // lseek to move the file reader to exact offset value
    if (lseek(image_fd, offset, SEEK_SET) == (off_t)-1)
    {
        fprintf(stderr, "Error seeking the offset, exiting ...\n");

        exit(EXIT_FAILURE);
    }

    ssize_t result = read(image_fd, buf, size);

    // if result < 0 -> read on error, if != size -> got fewer bytes than requested
    if (result < 0 || (size_t)result != size)
    {
        fprintf(stderr, "Error reading the disk, exiting ...\n");

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

/*
*   builds a printable 8.3 name for the 11-byte dir_name field
*   called in list_cb(for printing dirs and files)
*/

static void format_83(char *dst, const char *raw)
{
    char base[9] = {0};
    char ext[4] = {0};  // "dot" and file ext

    copy_name(base, raw, 8);
    copy_name(ext, raw+8, 3);   // assemble both

    if (ext[0] != '\0')
        snprintf(dst, 13, "%s.%s", base, ext);  // if there's a file exetension, attach it
    else
        snprintf(dst, 13, "%s", base);
}

// ======== recursively listing from the root cluster  =========
/* Forward declaration for recursion */
static void list_dir(uint32_t cluster, int depth);



/*
* test if cluster value signals eoc (end of chain)
*/
static int is_eoc(uint32_t fat_val)
{
    return fat_val >= FAT32_EOC_MIN;
}

/* 
* Return byte offset of the first byte of cluster N 
*/
static off_t cluster_to_offset(uint32_t cluster)
{
    uint32_t sector = ((cluster - 2) * image_bs.BPB_SecPerClus) + first_data_sec;

    return (off_t)sector * image_bs.BPB_BytesPerSec;
}


typedef void (*dir_callback)(const struct DirInfo *entry, void *userdata);

static void list_cb(const struct DirInfo *e, void *user_data)
{
    struct list_state *st = (struct list_state *)user_data; // cast back to list_state
    int depth = st->depth;  // get depth

    char name83[13];    // to store short names (12 for 8.3 name + NUL) (for spec doc)
    format_83(name83, e->dir_name);

    // print leading dashes to represents depth
    for (int i = 0; i < depth; i++)
    {
        printf("-");
    }

    // check if it's directory
    if (e->dir_attr & ATTR_DIRECTORY)   // bitwise AND to check if the ATTR_DIRECTORY bit is set in the attribute byte
    {
        printf("[DIRECTORY]: %s\n", name83);

        // recurse
        uint32_t sub_cluster =
            ((uint32_t)e->dir_first_cluster_hi << 16) |
             (uint32_t)e->dir_first_cluster_lo;
        list_dir(sub_cluster, depth + 1);
    }
    else{
        // not a directory -> file
        printf("File: %s\n", name83);
    }

}

/*
*
*/
static void walk_dir(uint32_t root_cluster, dir_callback cb, void *userdata)
{
    uint32_t cluster = root_cluster;

    while(!is_eoc(cluster) && cluster != 0 && cluster != FAT32_BAD)
    {
        off_t base = cluster_to_offset(cluster);    // convert cluster num to byte
        uint32_t entries_per_cluster = bytes_per_clus / DIR_ENTRY_SIZE;
    
        // for each cluster, loop through it
        for (uint32_t i = 0; i < entries_per_cluster; i++)  
        {
            uint8_t raw[DIR_ENTRY_SIZE];    // 32-byte buffer to hold one raw directory entry

            disk_read(base + (off_t)(i * DIR_ENTRY_SIZE), raw, DIR_ENTRY_SIZE);

            // first byte 0x00 = no more entries in this directory, done
            if (raw[0] == 0x00)   
            {    
                return;
            }

            // first byte 0xE5 = deleted entry, skip it
            if ((uint8_t)raw[0] == 0xE5)
            {
                continue;
            }

            // get attribute bit (11)
            uint8_t attr = raw[11];

            // validate
                // if lower 4 bits are all set = long name entry, skip
            if ((attr & 0x0F) == 0x0F) {
                continue;
            }

                // volume label entry, skip
            if (attr & ATTR_VOLUME_ID)
            {
                continue;
            }

            struct DirInfo entry;
            memcpy(&entry, raw, DIR_ENTRY_SIZE);

            // skip  dot and dot dot
            if (entry.dir_name[0] == '.')
            {
                continue;
            }

            cb(&entry, userdata);
        }

        // move to next cluster
        cluster = fat_entry(cluster);
    }

}


/*
*  wrapper around walk_dir
*/
static void list_dir(uint32_t cluster, int depth)
{
    struct list_state st = { depth };   // curr depth value
    walk_dir(cluster, list_cb, &st);
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
 * COMMAND 2: list 
 *  list all files and directories on the drive
 * --------------------------------- */
static void cmd_list()
{
    // get basic info about image
    uint32_t bps = image_bs.BPB_BytesPerSec;
    uint32_t spc = image_bs.BPB_SecPerClus;

    // validate fat[0] and fat[1]
    uint32_t fat0 = fat_entry(0);
    uint32_t fat1 = fat_entry(1);

    // find root cluster
    uint32_t root = image_bs.BPB_RootClus;  // root directory, always 2 on FAT32

    // get root cluster address
    uint32_t root_sector = ((root - 2) * spc) + first_data_sec;

    // volume label
    char vol_label[BS_VolLab_LENGTH + 1];
    copy_name(vol_label, image_bs.BS_VolLab, BS_VolLab_LENGTH);


    printf("Drive has %u Bytes per sector and %u sectors per cluster\n", bps, spc);
    
    printf("0x%08x should be 0x%08x\n", fat0 | 0xF0000000u, 0xfffffff8u);
    
    printf("0x%08x should be 0x%08x\n", fat1 | 0xF0000000u, 0xffffffffu);   // because fat_entry() masks off the top 4 bits
    printf("Searching for root cluster at %u\n", root);
    
    printf("Sector 2 address is %u\n", root_sector * bps);  // byte offset
    printf("Sector 2 address is %u\n", root_sector);    // sector num

    printf("Volume ID: %s\n", vol_label);

    list_dir(root, 0);
}

/* -----------------------------------
 * COMMAND 3: get 
 *  
 * --------------------------------- */

 struct find_state {
    char **parts;     // array of path components
    int n_parts;   // total num of components
    int cur_part;
    int found;
    uint32_t cluster;  
    uint32_t file_size;
    uint8_t attr;
};

static void find_cb(const struct DirInfo *e, void *userdata)
{
    struct find_state *fs = (struct find_state *)userdata;  // cast void* back to find_state

    if (fs->found)
    {
        return;     // skip already checked entries
    }

    // format curr entry's name into a readable string
    char name83[13];
    format_83(name83, e->dir_name);

    // compare entry name against curr path component
    if (strcasecmp(name83, fs->parts[fs->cur_part]) != 0)
    {
        return;
    }

    // get first cluster of this entry
    uint32_t cluster = ((uint32_t)e->dir_first_cluster_hi << 16) | (uint32_t)e->dir_first_cluster_lo;

    if (fs->cur_part == fs->n_parts - 1) 
    {
        // store result back into find_state struct
        fs->found = 1;  // true
        fs->cluster = cluster;
        fs->file_size = e->dir_file_size;
        fs->attr = e->dir_attr;
    }
    else if (e->dir_attr & ATTR_DIRECTORY)
    {
        // directory, move into subdirectory
        fs->cur_part++;
        walk_dir(cluster, find_cb, fs);

        if (!fs->found)
        {
            fs->cur_part--; // backtrack cur_part
        }
    }


}

static void cmd_get(const char *path)
{
    char *path_copy = strdup(path);     // save a copy

    // split path into parts
    char *parts[64];
    int n_parts = 0;

    char *token = strtok(path_copy, "/\\");

    while (token && n_parts < 64) {
        parts[n_parts++] = token;
        token = strtok(NULL, "/\\");
    }

    // validate if path is found
    if (n_parts == 0) {
        fprintf(stderr, "Invalid path: %s\n", path);
        
        return;
    }

    struct find_state fs;
    memset(&fs, 0, sizeof(fs));
    
    // set path parts
    fs.parts = parts;
    fs.n_parts = n_parts;

    // traverse the dir tree
    walk_dir(image_bs.BPB_RootClus, find_cb, &fs);

    // ===== if file not found -> doesn't exist on the disk
    if (!fs.found) {
        fprintf(stderr, "File not found: %s\n", path);
        free(path_copy);

        return;
    }

    // ===== found a directory (can't print out or save a directory)
    if (fs.attr & ATTR_DIRECTORY) {
        fprintf(stderr, "%s is a directory, not a file\n", path);
        free(path_copy);

        return;
    }

    // ===== found a valid file
    mkdir("get_cmd_output", 0755);  // owner: r/w/x   everyone: r/e

    // build an output path
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "get_cmd_output/%s", parts[n_parts - 1]);  // ex: "get_cmd_output/filename.[exetension]"

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    
    // walk the cluster chain, read data, write to stdout AND file
    uint32_t cluster = fs.cluster;
    uint32_t bytes_left = fs.file_size;

    uint8_t *buf = malloc(bytes_per_clus);

    // loop until eoc or all bytes read
    while (!is_eoc(cluster) && cluster != 0 && cluster != FAT32_BAD && bytes_left > 0)
    {
        off_t off = cluster_to_offset(cluster);
        uint32_t to_read = bytes_per_clus < bytes_left ? bytes_per_clus : bytes_left;

        disk_read(off, buf, to_read);

        // stdout
        if (write(STDOUT_FILENO, buf, to_read) != (ssize_t)to_read)
        {
            perror("write stdout");
        }

        // file
        if (write(out_fd, buf, to_read) != (ssize_t)to_read)
        {    
            perror("write file");
        }

        // subtract bytes just read
        bytes_left -= to_read;
        
        // get next cluster
        cluster = fat_entry(cluster);
    }

    free(buf);
    close(out_fd);
    free(path_copy);

    fprintf(stderr, "\nFile written to %s\n", out_path);
}


/* -----------------------------------
 * main()
 * --------------------------------- */
int main(int argc, char *argv[])
{
    // handle args
    if (argc < 3) {
        fprintf(stderr, "Error reading command, exiting ...\n");

        return EXIT_FAILURE;
    }

    const char *image_path = argv[1];
    const char *command = argv[2];

    // open disk image
    image_fd = open(image_path, O_RDONLY);
    
    if (image_fd < 0)
    {
        fprintf(stderr, "Error reading disk file, exiting ...\n");

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
        cmd_list();
    }
    else if (strcmp(command, "get") == 0)
    {
        if (argc != 4)
        {
            fprintf(stderr, "Unknown command, exiting ...\n");

            return EXIT_FAILURE;
        }

        cmd_get(argv[3]);
    }
    else {
        fprintf(stderr, "Unknown command: %s, exiting ...\n", command);
        close(image_fd);

        return EXIT_FAILURE;
    }



    printf("\n=====================================\n");
    printf("   |Program completed normally.|\n");
    printf("=====================================\n");

    return EXIT_SUCCESS;
    
}