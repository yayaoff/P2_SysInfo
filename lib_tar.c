#include "lib_tar.h"
#include <stdio.h>
#include <sys/stat.h> 
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

int is_end(tar_header_t* buffer){
    if (buffer == MAP_FAILED) {
        perror("mmap");
        return 0;
    }
    char* buf = (char*)buffer;
    for (int i = 0; i < 1024; i++)
    {
        if(buf[i] != '\0') return 0;
    }
    return 1; //End
}

char* checksum(tar_header_t* buffer){
    char* sm=malloc(sizeof(char)*8);
    unsigned int sum = 0;
    char* buf = (char*) buffer;
    for (int i = 0; i < sizeof(buf); i++) {
        sum += buf[i];
    }
    sprintf(sm, "%d", sum);
    return sm;
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
int check_archive(int tar_fd) {
    struct stat sb;
    fstat(tar_fd, &sb);
    tar_header_t* buffer = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, tar_fd, 0);
    char* magic_v = buffer->magic;
    if(strcmp(magic_v,TMAGIC) != 0) {
        munmap(buffer, sb.st_size);
        return -1;
    }
    char* version_v = buffer->version;
    if(strcmp(version_v,TVERSION) != 0){
        munmap(buffer, sb.st_size);
        return -2;
    } 
    // /The chksum field represents the simple sum of all bytes in the header block.
    //  Each 8-bit byte in the header is added to an unsigned integer, initialized to zero, 
    //  the precision of which shall be no less than seventeen bits. When calculating the checksum, 
    //  the chksum field is treated as if it were all blanks. 
    char* size = buffer->size;
    char* chsm = checksum(atoi(size),tar_fd);
    char* cheksum_v = buffer->chksum;
    if (strcmp(chsm,cheksum_v) != 0){
        munmap(buffer, sb.st_size);
        free(chsm);
        return -3;
    }
    free(chsm);
    munmap(buffer, sb.st_size);

    return 0;
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
int exists(int tar_fd, char *path) {
    if(check_archive(tar_fd)==0){
        if (access(path, F_OK) == 0) return 1;
    }
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
int is_dir(int tar_fd, char *path) {

    if(exists(tar_fd,path)!=0){
        char* buffer=malloc(sizeof(char));
        off_t typeflag_seek = lseek(tar_fd,(off_t)156,SEEK_SET);
        int tf = snprintf(buffer, sizeof(buffer), "%ld", typeflag_seek);
        char* buf_dirtype=malloc(sizeof(char));
        sprintf(buf_dirtype, "%c", DIRTYPE);
        if(strcmp(buffer,buf_dirtype)!=0){
            free(buffer);
            return 0;
        }
        free(buffer);
        return 1;
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
int is_file(int tar_fd, char *path) {
    if(exists(tar_fd,path)!=0){
        char* buffer=malloc(sizeof(char));
        off_t typeflag_seek = lseek(tar_fd,(off_t)156,SEEK_SET);
        int tf = snprintf(buffer, sizeof(buffer), "%ld", typeflag_seek);
        char* buf_filetype=malloc(sizeof(char));
        char* buf_afiletype=malloc(sizeof(char));
        sprintf(buf_filetype, "%c", REGTYPE);
        sprintf(buf_afiletype, "%c", AREGTYPE);
        if(strcmp(buffer,buf_filetype)!=0 && strcmp(buffer,buf_afiletype)!=0 ){
            free(buffer);
            return 0;
        }
        free(buffer);
        return 1;
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
int is_symlink(int tar_fd, char *path) {
    if(exists(tar_fd,path)!=0){
        struct stat s;
        if(stat(path,&s)==-1){
            return -1;
        }
        if (S_ISLNK(s.st_mode)) return 1;
        return 0;
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
ssize_t read_file(int tar_fd, char *path, size_t offset, uint8_t *dest, size_t *len) {
    if(is_file(tar_fd,path)!=0){
        struct stat s;
        if(stat(path,&s)==-1){
            return -1;
        }
        if(offset>s.st_size){
            return -2;
        }
        int fd = open(path,S_IRUSR);
        if(fd==-1){
            return -1;
        }
        ssize_t read_bytes = pread(fd,dest,*len,offset);
        if(read_bytes==-1){
            return -1;
        }
        *len = read_bytes;
        return s.st_size-read_bytes;
    }
    return -1;
}

int main(){
    return 1;
}