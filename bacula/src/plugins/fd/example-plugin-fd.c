/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2010-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/

#define BUILD_PLUGIN
#define BUILDING_DLL            /* required for Windows plugin */

#include "bacula.h"
#include "fd_plugins.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Your name"
#define PLUGIN_DATE         "January 2010"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Test File Daemon Plugin"

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
static bRC checkFile(bpContext *ctx, char *fname);


/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

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
   setFileAttributes,
   checkFile
};

/*
 * Plugin called here when it is first loaded
 */
bRC DLL_IMP_EXP
loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   printf("plugin: Loaded: size=%d version=%d\n", bfuncs->size, bfuncs->version);

   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when
 *  Bacula is going to exit.
 */
bRC DLL_IMP_EXP
unloadPlugin()
{
   printf("plugin: Unloaded\n");
   return bRC_OK;
}

/*
 * Called here to make a new instance of the plugin -- i.e. when
 *  a new Job is started.  There can be multiple instances of
 *  each plugin that are running at the same time.  Your
 *  plugin instance must be thread safe and keep its own
 *  local data.
 */
static bRC newPlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
// printf("plugin: newPlugin JobId=%d\n", JobId);
   bfuncs->registerBaculaEvents(ctx, 1, 2, 0);
   return bRC_OK;
}

/*
 * Release everything concerning a particular instance of a
 *  plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
   int JobId = 0;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
// printf("plugin: freePlugin JobId=%d\n", JobId);
   return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 *   Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
// printf("plugin: getPluginValue var=%d\n", var);
   return bRC_OK;
}

/*
 * Called by core code to set a plugin variable.
 *  Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
// printf("plugin: setPluginValue var=%d\n", var);
   return bRC_OK;
}

/*
 * Called by Bacula when there are certain events that the
 *   plugin might want to know.  The value depends on the
 *   event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   char *name;

   switch (event->eventType) {
   case bEventJobStart:
      printf("plugin: JobStart=%s\n", NPRT((char *)value));
      break;
   case bEventJobEnd:
      printf("plugin: JobEnd\n");
      break;
   case bEventStartBackupJob:
      printf("plugin: BackupStart\n");
      break;
   case bEventEndBackupJob:
      printf("plugin: BackupEnd\n");
      break;
   case bEventLevel:
      printf("plugin: JobLevel=%c %d\n", (int64_t)value, (int64_t)value);
      break;
   case bEventSince:
      printf("plugin: since=%d\n", (int64_t)value);
      break;
   case bEventStartRestoreJob:
      printf("plugin: StartRestoreJob\n");
      break;
   case bEventEndRestoreJob:
      printf("plugin: EndRestoreJob\n");
      break;
   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:command */
   case bEventRestoreCommand:
      printf("plugin: backup command=%s\n", NPRT((char *)value));
      break;
   case bEventBackupCommand:
      printf("plugin: backup command=%s\n", NPRT((char *)value));
      break;

   case bEventComponentInfo:
      printf("plugin: Component=%s\n", NPRT((char *)value));
      break;

   default:
      printf("plugin: unknown event=%d\n", event->eventType);
   }
   bfuncs->getBaculaValue(ctx, bVarFDName, (void *)&name);
// printf("FD Name=%s\n", name);
// bfuncs->JobMessage(ctx, __FILE__, __LINE__, 1, 0, "JobMesssage message");
// bfuncs->DebugMessage(ctx, __FILE__, __LINE__, 1, "DebugMesssage message");
   return bRC_OK;
}

/*
 * Called when starting to backup a file.  Here the plugin must
 *  return the "stat" packet for the directory/file and provide
 *  certain information so that Bacula knows what the file is.
 *  The plugin can create "Virtual" files by giving them a
 *  name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   return bRC_OK;
}

/*
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
   return bRC_OK;
}

/*
 * Do actual I/O.  Bacula calls this after startBackupFile
 *   or after startRestoreFile to do the actual file
 *   input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      printf("plugin: IO_OPEN\n");
      break;
   case IO_READ:
      printf("plugin: IO_READ buf=%p len=%d\n", io->buf, io->count);
      break;
   case IO_WRITE:
      printf("plugin: IO_WRITE buf=%p len=%d\n", io->buf, io->count);
      break;
   case IO_CLOSE:
      printf("plugin: IO_CLOSE\n");
      break;
   }
   return bRC_OK;
}

static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   return bRC_OK;
}

static bRC endRestoreFile(bpContext *ctx)
{
   return bRC_OK;
}

/*
 * Called here to give the plugin the information needed to
 *  re-create the file on a restore.  It basically gets the
 *  stat packet that was created during the backup phase.
 *  This data is what is needed to create the file, but does
 *  not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

/*
 * Called after the file has been restored. This can be used to
 *  set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(bpContext *ctx, char *fname)
{
   return bRC_OK;
}


#ifdef __cplusplus
}
#endif
