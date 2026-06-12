# COMP 3430 — Assignment 3
Reads a FAT32 formatted disk image and supports three commands: 
info (drive metadata), 
list (directory tree), and 
get (extract a file).

**Name:** Jian Yang  
**Student Number:** 8000293  
**Course:** COMP 3430, Section A01  
**Instructor:** Dr. Saulo dos Santos  

---

## Compiling
```bash
make
```

## Commands

### info

```bash
./fat32 imagename info
```
Example:
```bash
./fat32 ~comp3430/fat32volumes/3430-good.img info
```

### list
```bash
./fat32 imagename list
```
Example:
```bash
./fat32 ~comp3430/fat32volumes/3430-good.img list
```

### get

```bash
./fat32 imagename get path/to/file.txt
```
Example:
```bash
./fat32 ~comp3430/fat32volumes/3430-good.img get BOOKS/PANDP.TXT
```
The fetched file will be saved to `/get_cmd_output`, this folder will be deleted when `make clean` is run.

## Cleaning
```bash
make clean
```
Removes the compiled binary and the `/get_cmd_output` folder.