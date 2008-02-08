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
 * A simple pipe plugin for Bacula
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

#define PLUGIN_LICENSE      "GPL"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "January 2008"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Test File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startPluginBackup(bpContext *ctx, struct save_pkt *sp);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);


/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

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

static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startPluginBackup,
   pluginIO
};

struct plugin_ctx {
   boffset_t offset;
   FILE *fd;
   bool backup;
   char *cmd;
   char *fname;
   char *reader;
   char *writer;
};

bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
// printf("bpipe-fd: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);

   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

bRC unloadPlugin() 
{
// printf("bpipe-fd: Unloaded\n");
   return bRC_OK;
}

static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */
   return bRC_OK;
}

static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (p_ctx->cmd) {
      free(p_ctx->cmd);
   }
   free(p_ctx);
   return bRC_OK;
}

static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   char *name;

   switch (event->eventType) {
   case bEventJobStart:
//    printf("bpipe-fd: JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    printf("bpipe-fd: JobEnd\n");
      break;
   case bEventBackupStart:
//    printf("bpipe-fd: BackupStart\n");
      break;
   case bEventBackupEnd:
//    printf("bpipe-fd: BackupEnd\n");
      break;
   case bEventLevel:
//    printf("bpipe-fd: JobLevel=%c %d\n", (int)value, (int)value);
      break;
   case bEventSince:
//    printf("bpipe-fd: since=%d\n", (int)value);
      break;

   case bEventRestoreStart: 
//    printf("bpipe-fd: RestoreStart\n");
      break;
   case bEventRestoreEnd:
//    printf("bpipe-fd: RestoreEnd\n");
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:command */
   case bEventPluginCommand:
      char *p;
//    printf("bpipe-fd: pluginEvent cmd=%s\n", (char *)value);
      p_ctx->cmd = strdup((char *)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
         printf("Plugin terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
         printf("File terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
         printf("Reader terminator not found: %s\n", (char *)value);
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
   bfuncs->getBaculaValue(ctx, bVarFDName, (void *)&name);
// printf("FD Name=%s\n", name);
// bfuncs->JobMessage(ctx, __FILE__, __LINE__, 1, 0, "JobMesssage message");
// bfuncs->DebugMessage(ctx, __FILE__, __LINE__, 1, "DebugMesssage message");
   return bRC_OK;
}

static bRC startPluginBackup(bpContext *ctx, struct save_pkt *sp)
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
// printf("bpipe-fd: startPluginBackup\n");
   return bRC_OK;
}

/*
 * Do actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   char msg[200];
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      p_ctx->fd = popen(p_ctx->reader, "r");
      if (!p_ctx->fd) {
         io->io_errno = errno;
         snprintf(msg, sizeof(msg), "bpipe-fd: reader=%s failed: ERR=%d\n", p_ctx->reader, errno);
         bfuncs->JobMessage(ctx, __FILE__, __LINE__, 1, 0, msg);
         printf("%s", msg);
         return bRC_Error;
      }
      printf("bpipe-fd: IO_OPEN reader=%s fd=%p\n", p_ctx->reader, p_ctx->fd);
      sleep(1);                 /* let pipe connect */
      break;

   case IO_READ:
      io->status = fread(io->buf, 1, io->count, p_ctx->fd);
      printf("bpipe-fd: IO_READ buf=%p len=%d\n", io->buf, io->status);
      if (io->status == 0 && ferror(p_ctx->fd)) {
         printf("Error reading pipe\n");
         return bRC_Error;
      }
      printf("status=%d\n", io->status);
      break;

   case IO_WRITE:
      printf("bpipe-fd: IO_WRITE buf=%p len=%d\n", io->buf, io->count);
      break;

   case IO_CLOSE:
      io->status = pclose(p_ctx->fd);
      printf("bpipe-fd: IO_CLOSE\n");
      break;

   case IO_SEEK:
      io->offset = p_ctx->offset;
      break;
   }
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif
