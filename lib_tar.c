#include "lib_tar.h"
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

int is_end(tar_header_t *buffer)
{
    if (buffer == MAP_FAILED)
    {
        perror("mmap");
        return 0;
    }
    char *buf = (char *)buffer;
    for (int i = 0; i < 1024; i++)
    {
        if (buf[i] != '\0')
            return 0;
    }
    return 1; // End
}

char *checksum(tar_header_t *buffer)
{
    char *sm = malloc(sizeof(char) * 8);
    unsigned int sum = 0;
    char *buf = (char *)buffer;
    for (int i = 0; i < 148; i++)
    {
        sum += buf[i];
    }
    for (int i = 0; i < 8; i++)
    {
        sum += ' ';
    }
    for (int i = 156; i < 512; i++)
    {
        sum += buf[i];
    }
    sprintf(sm, "%06o", sum);
    return sm;
}

int find_block(size_t size)
{
    int block = size / 512;
    if (size % 512 != 0)
        block++;
    return block;
}

/**
 * Checks whether the archive is valid.
 *
 * Each non-null header of a valid archive has:
 *  - a magic value of "ustar" and a null,
 *  - a version value of "00" and no null,
 *  - a correct checksum
 *
 * @param tar_fd A file descriptor pointing to the start of a file supposed to contain a tar archive.
 *
 * @return a zero or positive value if the archive is valid, representing the number of non-null headers in the archive,
 *         -1 if the archive contains a header with an invalid magic value,
 *         -2 if the archive contains a header with an invalid version value,
 *         -3 if the archive contains a header with an invalid checksum value
 */
int check_archive(int tar_fd)
{
    int count = 0;
    struct stat sb;
    fstat(tar_fd, &sb);
    tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
    while (is_end(buffer) == 0)
    {
        if (strncmp(buffer->magic, TMAGIC, TMAGLEN) != 0)
        {
            munmap(buffer, sb.st_size);
            return -1;
        }
        if (strncmp(buffer->version, TVERSION, TVERSLEN) != 0)
        {
            munmap(buffer, sb.st_size);
            return -2;
        }
        char *chsm = checksum(buffer);
        if (strncmp(chsm, buffer->chksum, 8) != 0)
        {
            munmap(buffer, sb.st_size);
            free(chsm);
            return -3;
        }
        count++;
        buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
        free(chsm);
    }
    munmap(buffer, sb.st_size);
    return count;
}

/**
 * Checks whether an entry exists in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive,
 *         any other value otherwise.
 */
int exists(int tar_fd, char *path)
{
    struct stat sb;
    fstat(tar_fd, &sb);
    tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
    while (is_end(buffer) == 0)
    {
        if (strcmp(buffer->name, path) == 0)
        {
            munmap(buffer, sb.st_size);
            return 1;
        }
        buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
    }
    munmap(buffer, sb.st_size);
    return 0;
}

// /**
//  * Checks whether an entry exists in the archive and is a directory.
//  *
//  * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
//  * @param path A path to an entry in the archive.
//  *
//  * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
//  *         any other value otherwise.
//  *
//  **/
// int is_dir(int tar_fd, char *path) {
//     if(exists(tar_fd,path)!=0){
//         struct stat s;
//         if(stat(path,&s)==-1){
//             return -1;
//         }
//         if (S_ISDIR(s.st_mode)) return 1;
//         return 0;
//     }
//     return 0;
// }

/**
 * Checks whether an entry exists in the archive and is a directory.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a directory,
 *         any other value otherwise.
 *
 **/
int is_dir(int tar_fd, char *path)
{
    if (exists(tar_fd, path) != 0)
    {
        struct stat sb;
        fstat(tar_fd, &sb);
        tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
        char *dir_type = malloc(sizeof(char));
        sprintf(dir_type, "%c", DIRTYPE);
        while (is_end(buffer) == 0)
        {
            if (strcmp(buffer->name, path) == 0)
            {
                char *dir_flag = malloc(sizeof(char));
                sprintf(dir_flag, "%c", buffer->typeflag);
                if (strcmp(dir_flag, dir_type) == 0)
                {
                    free(dir_type);
                    free(dir_flag);
                    munmap(buffer, sb.st_size);
                    return 1;
                }
                free(dir_type);
                free(dir_flag);
                munmap(buffer, sb.st_size);
                return 0;
            }
            buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
        }
        free(dir_type);
        munmap(buffer, sb.st_size);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a file.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 *
 * @return zero if no entry at the given path exists in the archive or the entry is not a file,
 *         any other value otherwise.
 */
int is_file(int tar_fd, char *path)
{

    if (exists(tar_fd, path) != 0)
    {
        struct stat sb;
        fstat(tar_fd, &sb);
        tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
        char *areg_type = malloc(sizeof(char));
        sprintf(areg_type, "%c", AREGTYPE);
        char *reg_type = malloc(sizeof(char));
        sprintf(reg_type, "%c", REGTYPE);
        while (is_end(buffer) == 0)
        {
            if (strcmp(buffer->name, path) == 0)
            {
                char *flag = malloc(sizeof(char));
                sprintf(flag, "%c", buffer->typeflag);
                if (strcmp(flag, areg_type) == 0 || strcmp(flag, reg_type) == 0)
                {
                    free(flag);
                    free(areg_type);
                    free(reg_type);
                    munmap(buffer, sb.st_size);
                    return 1;
                }
                free(flag);
                free(areg_type);
                free(reg_type);
                munmap(buffer, sb.st_size);
                return 0;
            }
            buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
        }
        free(areg_type);
        free(reg_type);
        munmap(buffer, sb.st_size);
    }
    return 0;
}

/**
 * Checks whether an entry exists in the archive and is a symlink.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive.
 * @return zero if no entry at the given path exists in the archive or the entry is not symlink,
 *         any other value otherwise.
 */
int is_symlink(int tar_fd, char *path)
{
    if (exists(tar_fd, path) != 0)
    {
        struct stat sb;
        fstat(tar_fd, &sb);
        tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
        char *lnk_type = malloc(sizeof(char));
        sprintf(lnk_type, "%c", SYMTYPE);
        while (is_end(buffer) == 0)
        {
            if (strcmp(buffer->name, path) == 0)
            {
                char *flag = malloc(sizeof(char));
                sprintf(flag, "%c", buffer->typeflag);
                if (strcmp(flag, lnk_type) == 0)
                {
                    free(lnk_type);
                    free(flag);
                    munmap(buffer, sb.st_size);
                    return 1;
                }
                free(lnk_type);
                free(flag);
                munmap(buffer, sb.st_size);
                return 0;
            }
            buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
        }
        free(lnk_type);
        munmap(buffer, sb.st_size);
    }
    return 0;
}

/**
 * Lists the entries at a given path in the archive.
 * list() does not recurse into the directories listed at the given path.
 *
 * Example:
 *  dir/          list(..., "dir/", ...) lists "dir/a", "dir/b", "dir/c/" and "dir/e/"
 *   ├── a
 *   ├── b
 *   ├── c/
 *   │   └── d
 *   └── e/
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive. If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param entries An array of char arrays, each one is long enough to contain a tar entry path.
 * @param no_entries An in-out argument.
 *                   The caller set it to the number of entries in `entries`.
 *                   The callee set it to the number of entries listed.
 *
 * @return zero if no directory at the given path exists in the archive,
 *         any other value otherwise.
 */
int list(int tar_fd, char *path, char **entries, size_t *no_entries) {
    if(exists(tar_fd,path)!=0){
        struct stat sb;
        fstat(tar_fd, &sb);
        tar_header_t* buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
        int count = 0;
        while (is_end(buffer) == 0){
            if(strcmp(buffer->name,path) == 0){
                if(is_dir(tar_fd,path)!=0){
                    int dir_size = TAR_INT(buffer->size);
                    printf("SIZE= %d\n",dir_size);
                    printf("%s\n",buffer->gid);
                    buffer = (tar_header_t*)((uint8_t*)buffer + 512 + dir_size);
                    for (int i = 0; i < dir_size ; i++){
                        no_entries++;
                        memcpy(entries[i],buffer->name,(size_t)100);
                        printf("e[%d] = %s\n",i,entries[i]);
                        buffer = (tar_header_t*)((uint8_t*)buffer + 512 + dir_size);
                    }
                    munmap(buffer, sb.st_size);
                    return 1;
                }
                if(is_symlink(tar_fd,path)!=0){
                    return(list(tar_fd, buffer->linkname, entries, no_entries));
                }
            }
            buffer = (tar_header_t*)((uint8_t*)buffer + 512 + find_block(TAR_INT(buffer->size))*512);
            count++;
        }
        munmap(buffer, sb.st_size);
    }
    return 0;
}


/**
 * Reads a file at a given path in the archive.
 *
 * @param tar_fd A file descriptor pointing to the start of a valid tar archive file.
 * @param path A path to an entry in the archive to read from.  If the entry is a symlink, it must be resolved to its linked-to entry.
 * @param offset An offset in the file from which to start reading from, zero indicates the start of the file.
 * @param dest A destination buffer to read the given file into.
 * @param len An in-out argument.
 *            The caller set it to the size of dest.
 *            The callee set it to the number of bytes written to dest.
 *
 * @return -1 if no entry at the given path exists in the archive or the entry is not a file,
 *         -2 if the offset is outside the file total length,
 *         zero if the file was read in its entirety into the destination buffer,
 *         a positive value if the file was partially read, representing the remaining bytes left to be read to reach
 *         the end of the file.
 *
 */
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len)
{
    struct stat sb;
    fstat(tar_fd, &sb);
    tar_header_t *buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
    while (is_end(buffer) == 0)
    {
        if (strcmp(buffer->name, path) == 0)
        {
            if (is_file(tar_fd, path) != 0)
            {
                if (TAR_INT(buffer->size) < offset)
                {
                    munmap(buffer, sb.st_size);
                    return -2;
                }
                size_t size = TAR_INT(buffer->size) - offset;
                if (size < *len)
                {
                    *len = size;
                }
                memcpy(dest, (uint8_t *)buffer + 512 + offset, *len);
                munmap(buffer, sb.st_size);
                return size - *len;
            }
            if (is_symlink(tar_fd, path) != 0)
            {
                return read_file(tar_fd, buffer->linkname, offset, dest, len);
            }
        }
        buffer = (tar_header_t *)((uint8_t *)buffer + 512 + find_block(TAR_INT(buffer->size)) * 512);
    }
    munmap(buffer, sb.st_size);
    return -1;
}

int main(int argc, char *argv[]) {
    int tar_fd = open(argv[1], O_RDONLY);
    if (tar_fd == -1) {
        perror("open");
        return 1;
    }
    char **entries = malloc(sizeof(char*)*100);
    for(int i =0; i < 100; i++){
        entries[i]=malloc(sizeof(char)*100);
    }
    int rep = list(tar_fd,argv[2],entries,0);
    printf("%d\n", rep);
    free(entries);
    return 0;
}