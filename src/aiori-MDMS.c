/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 */
/******************************************************************************\
*                                                                              *
*        Copyright (c) 2003, The Regents of the University of California       *
*      See the file COPYRIGHT for a complete copyright notice and license.     *
*                                                                              *
********************************************************************************
*
* Implement of abstract I/O interface for POSIX.
*
\******************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>

#ifdef __linux__
#  include <sys/ioctl.h>          /* necessary for: */
#  define __USE_GNU               /* O_DIRECT and */
#  include <fcntl.h>              /* IO operations */
#  undef __USE_GNU
#endif                          /* __linux__ */

#include <errno.h>
#include <fcntl.h>              /* IO operations */
#include <sys/stat.h>
#include <assert.h>

#include "ior.h"
#include "aiori.h"
#include "iordef.h"
#include "utilities.h"

#ifndef   open64                /* necessary for TRU64 -- */
#  define open64  open            /* unlikely, but may pose */
#endif  /* not open64 */                        /* conflicting prototypes */

#ifndef   lseek64               /* necessary for TRU64 -- */
#  define lseek64 lseek           /* unlikely, but may pose */
#endif  /* not lseek64 */                        /* conflicting prototypes */

#ifndef   O_BINARY              /* Required on Windows    */
#  define O_BINARY 0
#endif

#if defined(HAVE_SYS_STATVFS_H)
#include <sys/statvfs.h>
#endif

#if defined(HAVE_SYS_STATFS_H)
#include <sys/statfs.h>
#endif

/**************************** P R O T O T Y P E S *****************************/
static IOR_offset_t MDMS_Xfer(int, void *, IOR_size_t *,
                               IOR_offset_t, IOR_param_t *);
static void MDMS_Fsync(void *, IOR_param_t *);
static void MDMS_Sync(IOR_param_t * );
int sockfd, connfd;
int local_rank;
#define MAX 1000
char buff[MAX];

void callGoClient()
{
        write(sockfd, buff, sizeof(buff));
        bzero(buff, MAX);
        read(sockfd, buff, sizeof(buff));
}

void genCommand(char *type, char *path)
{
        bzero(buff, MAX);
        strcpy(buff, type);
        strcpy(buff + strlen(type), path);
        buff[strlen(type) + strlen(path)] = '\0';
}
/************************** O P T I O N S *****************************/
typedef struct{
  /* in case of a change, please update depending MMAP module too */
  int direct_io;
} mdms_options_t;


option_help * MDMS_options(void ** init_backend_options, void * init_values){
  //printf("MDMS_options\n");
  mdms_options_t * o = malloc(sizeof(mdms_options_t));

  if (init_values != NULL){
    memcpy(o, init_values, sizeof(mdms_options_t));
  }else{
    o->direct_io = 0;
  }

  *init_backend_options = o;

  option_help h [] = {
    {0, "posix.odirect", "Direct I/O Mode", OPTION_FLAG, 'd', & o->direct_io},
    LAST_OPTION
  };
  option_help * help = malloc(sizeof(h));
  memcpy(help, h, sizeof(h));
  return help;
}

/***************************** F U N C T I O N S ******************************/

/*
 * Creat and open a file through the POSIX interface.
 */
void *MDMS_Create(char *testFileName, IOR_param_t * param)
{
        genCommand("create ", testFileName);
        callGoClient();
}

/*
 * Creat a file through mknod interface.
 */
int MDMS_Mknod(char *testFileName)
{
        genCommand("mknod ", testFileName);
        callGoClient();
}

/*
 * Open a file through the POSIX interface.
 */
void *MDMS_Open(char *testFileName, IOR_param_t * param)
{
        genCommand("open ", testFileName);
        callGoClient();
        
        int *fd;
        fd = (int *)malloc(sizeof(int));
        *fd = 16;
        return ((void*)fd);
}

/*
 * Write or read access to file using the POSIX interface.
 */
static IOR_offset_t MDMS_Xfer(int access, void *file, IOR_size_t * buffer,
                               IOR_offset_t length, IOR_param_t * param)
{
        genCommand("xfer ", "");
        callGoClient();
        //printf("MDMS_Xfer\n");

        int xferRetries = 0;
        long long remaining = (long long)length;
        char *ptr = (char *)buffer;
        long long rc;
        int fd;

        if(param->dryRun)
          return length;

        fd = *(int *)file;

        /* seek to offset */
        if (lseek64(fd, param->offset, SEEK_SET) == -1)
                ERRF("lseek64(%d, %lld, SEEK_SET) failed", fd, param->offset);

        while (remaining > 0) {
                /* write/read file */
                if (access == WRITE) {  /* WRITE */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout,
                                        "task %d writing to offset %lld\n",
                                        rank,
                                        param->offset + length - remaining);
                        }
                        rc = write(fd, ptr, remaining);
                        if (rc == -1)
                                ERRF("write(%d, %p, %lld) failed",
                                        fd, (void*)ptr, remaining);
                        if (param->fsyncPerWrite == TRUE)
                                MDMS_Fsync(&fd, param);
                } else {        /* READ or CHECK */
                        if (verbose >= VERBOSE_4) {
                                fprintf(stdout,
                                        "task %d reading from offset %lld\n",
                                        rank,
                                        param->offset + length - remaining);
                        }
                        rc = read(fd, ptr, remaining);
                        if (rc == 0)
                                ERRF("read(%d, %p, %lld) returned EOF prematurely",
                                        fd, (void*)ptr, remaining);
                        if (rc == -1)
                                ERRF("read(%d, %p, %lld) failed",
                                        fd, (void*)ptr, remaining);
                }
                if (rc < remaining) {
                        fprintf(stdout,
                                "WARNING: Task %d, partial %s, %lld of %lld bytes at offset %lld\n",
                                rank,
                                access == WRITE ? "write()" : "read()",
                                rc, remaining,
                                param->offset + length - remaining);
                        if (param->singleXferAttempt == TRUE)
                                MPI_CHECK(MPI_Abort(MPI_COMM_WORLD, -1),
                                          "barrier error");
                        if (xferRetries > MAX_RETRY)
                                ERR("too many retries -- aborting");
                }
                assert(rc >= 0);
                assert(rc <= remaining);
                remaining -= rc;
                ptr += rc;
                xferRetries++;
        }

        return (length);
}

/*
 * Perform fsync().
 */
static void MDMS_Fsync(void *fd, IOR_param_t * param)
{
        //printf("MDMS_Fsync, fd = %d\n", *(int*)fd);
        if (fsync(*(int *)fd) != 0)
                EWARNF("fsync(%d) failed", *(int *)fd);
}


static void MDMS_Sync(IOR_param_t * param)
{
  //printf("MDMS_Sync\n");
  int ret = system("sync");
  if (ret != 0){
    FAIL("Error executing the sync command, ensure it exists.");
  }
}


/*
 * Close a file through the POSIX interface.
 */
void MDMS_Close(void *fd, IOR_param_t * param)
{
        //char fdn[10];
        //itoa(*(int*)fd, fdn, 10);
        //sprintf(fdn, "%d", *(int*)fd);
        genCommand("close", "");
        callGoClient();
        //printf("MDMS_Close, fd = %d\n", *(int*)fd);
        //printf("MDMS_Close\n");

}

/*
 * Delete a file through the POSIX interface.
 */
void MDMS_Delete(char *testFileName, IOR_param_t * param)
{
        genCommand("delete ", testFileName);
        callGoClient();
        //printf("MDMS_Delete %s\n", testFileName);

}

/*
 * Use POSIX stat() to return aggregate file size.
 */
IOR_offset_t MDMS_GetFileSize(IOR_param_t * test, MPI_Comm testComm,
                                      char *testFileName)
{
        //printf("MDMS_GetFileSize %s\n", testFileName);
        if(test->dryRun)
          return 0;
        struct stat stat_buf;
        IOR_offset_t aggFileSizeFromStat, tmpMin, tmpMax, tmpSum;

        if (stat(testFileName, &stat_buf) != 0) {
                ERRF("stat(\"%s\", ...) failed", testFileName);
        }
        aggFileSizeFromStat = stat_buf.st_size;

        if (test->filePerProc == TRUE) {
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpSum, 1,
                                        MPI_LONG_LONG_INT, MPI_SUM, testComm),
                          "cannot total data moved");
                aggFileSizeFromStat = tmpSum;
        } else {
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMin, 1,
                                        MPI_LONG_LONG_INT, MPI_MIN, testComm),
                          "cannot total data moved");
                MPI_CHECK(MPI_Allreduce(&aggFileSizeFromStat, &tmpMax, 1,
                                        MPI_LONG_LONG_INT, MPI_MAX, testComm),
                          "cannot total data moved");
                if (tmpMin != tmpMax) {
                        if (rank == 0) {
                                WARN("inconsistent file size by different tasks");
                        }
                        /* incorrect, but now consistent across tasks */
                        aggFileSizeFromStat = tmpMin;
                }
        }

        return (aggFileSizeFromStat);
}

char* MDMS_GetVersion()
{
  return "";
}

int MDMS_Statfs (const char *path, ior_aiori_statfs_t *stat_buf, IOR_param_t * param)
{
        //printf("MDMS_Statfs, path = %d\n", path);
        int ret;
#if defined(HAVE_STATVFS)
        struct statvfs statfs_buf;

        ret = statvfs (path, &statfs_buf);
#else
        struct statfs statfs_buf;

        ret = statfs (path, &statfs_buf);
#endif
        if (-1 == ret) {
                return -1;
        }

        stat_buf->f_bsize = statfs_buf.f_bsize;
        stat_buf->f_blocks = statfs_buf.f_blocks;
        stat_buf->f_bfree = statfs_buf.f_bfree;
        stat_buf->f_files = statfs_buf.f_files;
        stat_buf->f_ffree = statfs_buf.f_ffree;

        return 0;
}

int MDMS_Mkdir (const char *path, mode_t mode, IOR_param_t * param)
{
        //bzero(buff, MAX);
        //strcpy(buff, "mkdir ");
        //strcpy(buff + 6, path);
        genCommand("mkdir ", path);
        callGoClient();
        //printf("MDMS_Mkdir %s receive from server = %s\n", path, buff);
        
        return 0;
        
}

int MDMS_Rmdir (const char *path, IOR_param_t * param)
{
        genCommand("rmdir ", path);
        callGoClient();
        //printf("MDMS_Rmdir %s\n", path);

        return 0;
}

int MDMS_Access (const char *path, int mode, IOR_param_t * param)
{
        genCommand("access ", path);
        callGoClient();
        //printf("MDMS_Access %s\n", path);

        return 0;
}

int MDMS_Stat (const char *path, struct stat *buf, IOR_param_t * param)
{
        genCommand("stat ", path);
        callGoClient();
        //printf("MDMS_Stat %s\n", path);
        
        return 0;
}

void MDMS_Initialize ()
{
        printf("MDMS_Initialize, hello world! #%d\n", local_rank);
        struct sockaddr_in servaddr, cli;

        // socket create and varification
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
                printf("socket creation failed...\n");
                exit(0);
        }
        else
                printf("Socket successfully created..\n");
        bzero(&servaddr, sizeof(servaddr));

        // assign IP, PORT
        servaddr.sin_family = AF_INET;
        servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        servaddr.sin_port = htons(6789 + local_rank);

        // connect the client socket to server socket
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0) {
                printf("connection with the server failed...\n");
                exit(0);
        }
        else
                printf("connected to the server..\n");
}

void MDMS_Finalize ()
{
        close(sockfd);
}

/************************** D E C L A R A T I O N S ***************************/

ior_aiori_t mdms_aiori = {
        .name = "MDMS",
        .name_legacy = NULL,
        .create = MDMS_Create,
        .mknod = MDMS_Mknod,
        .open = MDMS_Open,
        .xfer = MDMS_Xfer,
        .close = MDMS_Close,
        .delete = MDMS_Delete,
        .get_version = MDMS_GetVersion,
        .fsync = MDMS_Fsync,
        .get_file_size = MDMS_GetFileSize,
        .statfs = MDMS_Statfs,
        .mkdir = MDMS_Mkdir,
        .rmdir = MDMS_Rmdir,
        .access = MDMS_Access,
        .stat = MDMS_Stat,
        .get_options = MDMS_options,
        .enable_mdtest = true,
        .sync = MDMS_Sync,
        .initialize = MDMS_Initialize,
        .finalize = MDMS_Finalize
};
// aiori_get_version -> MDMS_GetVersion
// aiori_posix_statfs -> MDMS_Statfs
// aiori_posix_mkdir -> MDMS_Mkdir
// aiori_posix_rmdir -> MDMS_Rmdir
// aiori_posix_access -> MDMS_Access
// aiori_posix_stat -> MDMS_Stat
