#include "lib_tar.h"
#include <stdio.h>
#include <sys/stat.h> 
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

char* checksum(int size, int fd){
    char* sm=malloc(sizeof(char)*8);
    unsigned int sum = 0;
    char buffer[size];
    size_t f_read = read(fd,buffer,size);
    if(f_read==-1) return NULL;
    for (int i = 0; i < sizeof(buffer); i++) {
        sum += (unsigned char) buffer[i];
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
    char* buffer;


    buffer = malloc(sizeof(char)*TMAGLEN);
    off_t magic_v = lseek(tar_fd,(off_t)257,SEEK_SET);
    int m = snprintf(buffer, sizeof(buffer), "%ld", magic_v);
    if(strcmp(buffer,TMAGIC) != 0){
        free(buffer);
        return -1;
    }
    free(buffer);

    buffer=malloc(sizeof(char)*TVERSLEN);
    off_t version_v = lseek(tar_fd,(off_t)263,SEEK_SET);
    int v = snprintf(buffer, sizeof(buffer), "%ld", version_v);
    if (strcmp(buffer,TVERSION) != 0){
        free(buffer);
        return -2;
    }
    free(buffer);

    // /The chksum field represents the simple sum of all bytes in the header block.
    //  Each 8-bit byte in the header is added to an unsigned integer, initialized to zero, 
    //  the precision of which shall be no less than seventeen bits. When calculating the checksum, 
    //  the chksum field is treated as if it were all blanks. 
    buffer=malloc(sizeof(char)*8);
    off_t cheksum_v = lseek(tar_fd,(off_t)148,SEEK_SET);
    int c = snprintf(buffer, sizeof(buffer), "%ld", cheksum_v);
    char *size_buf = malloc(sizeof(char)*12);
    off_t size_seek = lseek(tar_fd,(off_t)124,SEEK_SET);
    int s = snprintf(size_buf, sizeof(size_buf), "%ld", size_seek);
    int size = atoi(size_buf);
    char* chsm = checksum(size,tar_fd);
    if (strcmp(chsm,buffer) != 0){
        free(chsm);
        free(size_buf);
        free(buffer);
        return -3;
    }
    free(chsm);
    free(size_buf);
    free(buffer);


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
    return 4;
}