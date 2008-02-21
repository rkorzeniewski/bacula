/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * A simple pipe plugin for the Bacula File Daemon
 *
 *  Kern Sibbald, October 2007
 *
 */
#include "fd-plugins.h"

#undef malloc
#undef free
#undef strdup

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_LICENSE      "GPLv2"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "January 2008"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Pipe File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);


/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/* Plugin Information block */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   PLUGIN_INTERFACE_VERSION,
   PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

/* Plugin entry points for Bacula */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   boffset_t offset;
   FILE *fd;                          /* pipe file descriptor */
   bool backup;                       /* set for backup (not needed) */
   char *cmd;                         /* plugin command line */
   char *fname;                       /* filename to "backup/restore" */
   char *reader;                      /* reader program for backup */
   char *writer;                      /* writer program for backup */
};

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 */
/*
 * External entry point called by Bacula to "load the plugin
 */
bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * External entry point to unload the plugin 
 */
bRC unloadPlugin() 
{
// printf("bpipe-fd: Unloaded\n");
   return bRC_OK;
}

/*
 * The following entry points are accessed through the function 
 *   pointers we supplied to Bacula. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
/*
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */
   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (p_ctx->cmd) {
      free(p_ctx->cmd);                  /* free any allocated command string */
   }
   free(p_ctx);                          /* free our private context */
   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Handle an event that was generated in Bacula
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
// char *name;

   switch (event->eventType) {
   case bEventJobStart:
//    printf("bpipe-fd: JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    printf("bpipe-fd: JobEnd\n");
      break;
   case bEventStartBackupJob:
//    printf("bpipe-fd: BackupStart\n");
      break;
   case bEventEndBackupJob:
//    printf("bpipe-fd: BackupEnd\n");
      break;
   case bEventLevel:
//    printf("bpipe-fd: JobLevel=%c %d\n", (int)value, (int)value);
      break;
   case bEventSince:
//    printf("bpipe-fd: since=%d\n", (int)value);
      break;

   case bEventStartRestoreJob:
      break;

   case bEventEndRestoreJob:
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventRestoreCommand:
      printf("bpipe-fd: EventRestoreCommand cmd=%s\n", (char *)value);
   case bEventBackupCommand:
      char *p;
      printf("bpipe-fd: pluginEvent cmd=%s\n", (char *)value);
      p_ctx->cmd = strdup((char *)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "Plugin terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "File terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "Reader terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate reader string */
      p_ctx->writer = p;
      printf("bpipe-fd: plugin=%s fname=%s reader=%s writer=%s\n", 
         p_ctx->cmd, p_ctx->fname, p_ctx->reader, p_ctx->writer);
      break;

   default:
      printf("bpipe-fd: unknown event=%d\n", event->eventType);
   }
   return bRC_OK;
}

/* 
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   time_t now = time(NULL);
   sp->fname = p_ctx->fname;
   sp->statp.st_mode = 0700 | S_IFREG;
   sp->statp.st_ctime = now;
   sp->statp.st_mtime = now;
   sp->statp.st_atime = now;
   sp->statp.st_size = -1;
   sp->statp.st_blksize = 4096;
   sp->statp.st_blocks = 1;
   p_ctx->backup = true;
// printf("bpipe-fd: startBackupFile\n");
   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   /*
    * We would return bRC_More if we wanted startBackupFile to be
    * called again to backup another file
    */
   return bRC_OK;
}

/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
//    printf("bpipe-fd: IO_OPEN\n");
      if (io->flags & (O_CREAT | O_WRONLY)) {
         p_ctx->fd = popen(p_ctx->writer, "w");
         printf("bpipe-fd: IO_OPEN writer=%s\n", p_ctx->writer);
         if (!p_ctx->fd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, 
               "Open pipe writer=%s failed: ERR=%s\n", p_ctx->writer, strerror(errno));
            return bRC_Error;
         }
      } else {
         p_ctx->fd = popen(p_ctx->reader, "r");
//       printf("bpipe-fd: IO_OPEN reader=%s\n", p_ctx->reader);
         if (!p_ctx->fd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, 
               "Open pipe reader=%s failed: ERR=%s\n", p_ctx->reader, strerror(errno));
            return bRC_Error;
         }
      }
      sleep(1);                 /* let pipe connect */
      break;

   case IO_READ:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "Logic error: NULL read FD\n");
         return bRC_Error;
      }
      io->status = fread(io->buf, 1, io->count, p_ctx->fd);
//    printf("bpipe-fd: IO_READ buf=%p len=%d\n", io->buf, io->status);
      if (io->status == 0 && ferror(p_ctx->fd)) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, 
            "Pipe read error: ERR=%s\n", strerror(errno));
//       printf("Error reading pipe\n");
         return bRC_Error;
      }
      break;

   case IO_WRITE:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "Logic error: NULL write FD\n");
         return bRC_Error;
      }
//    printf("bpipe-fd: IO_WRITE fd=%p buf=%p len=%d\n", p_ctx->fd, io->buf, io->count);
      io->status = fwrite(io->buf, 1, io->count, p_ctx->fd);
//    printf("bpipe-fd: IO_WRITE buf=%p len=%d\n", io->buf, io->status);
      if (io->status == 0 && ferror(p_ctx->fd)) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, 
            "Pipe write error\n");
//       printf("Error writing pipe\n");
         return bRC_Error;
      }
      break;

   case IO_CLOSE:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, M_FATAL, 0, "Logic error: NULL FD\n");
         return bRC_Error;
      }
      io->status = pclose(p_ctx->fd);
      break;

   case IO_SEEK:
      io->offset = p_ctx->offset;
      break;
   }
   return bRC_OK;
}

static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
// printf("bpipe-fd: startRestoreFile cmd=%s\n", cmd);
   return bRC_OK;
}

static bRC endRestoreFile(bpContext *ctx)
{
// printf("bpipe-fd: endRestoreFile\n");
   return bRC_OK;
}

static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: createFile\n");
   return bRC_OK;
}

static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: setFileAttributes\n");
   return bRC_OK;
}


#ifdef __cplusplus
}
#endif
