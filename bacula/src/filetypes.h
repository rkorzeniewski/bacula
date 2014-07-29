/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/**
 * Stream definitions.  Split from baconfig.h Nov 2010
 *
 *  Kern Sibbald, MM
 *
 */

#ifndef __BFILETYPES_H
#define __BFILETYPES_H 1


/**
 *  File type (Bacula defined).
 *  NOTE!!! These are saved in the Attributes record on the tape, so
 *          do not change them. If need be, add to them.
 *
 *  This is stored as 32 bits on the Volume, but only FT_MASK (16) bits are
 *    used for the file type. The upper bits are used to indicate
 *    additional optional fields in the attribute record.
 */
#define FT_MASK       0xFFFF          /* Bits used by FT (type) */
#define FT_LNKSAVED   1               /* hard link to file already saved */
#define FT_REGE       2               /* Regular file but empty */
#define FT_REG        3               /* Regular file */
#define FT_LNK        4               /* Soft Link */
#define FT_DIREND     5               /* Directory at end (saved) */
#define FT_SPEC       6               /* Special file -- chr, blk, fifo, sock */
#define FT_NOACCESS   7               /* Not able to access */
#define FT_NOFOLLOW   8               /* Could not follow link */
#define FT_NOSTAT     9               /* Could not stat file */
#define FT_NOCHG     10               /* Incremental option, file not changed */
#define FT_DIRNOCHG  11               /* Incremental option, directory not changed */
#define FT_ISARCH    12               /* Trying to save archive file */
#define FT_NORECURSE 13               /* No recursion into directory */
#define FT_NOFSCHG   14               /* Different file system, prohibited */
#define FT_NOOPEN    15               /* Could not open directory */
#define FT_RAW       16               /* Raw block device */
#define FT_FIFO      17               /* Raw fifo device */
/**
 * The DIRBEGIN packet is sent to the FD file processing routine so
 * that it can filter packets, but otherwise, it is not used
 * or saved */
#define FT_DIRBEGIN  18               /* Directory at beginning (not saved) */
#define FT_INVALIDFS 19               /* File system not allowed for */
#define FT_INVALIDDT 20               /* Drive type not allowed for */
#define FT_REPARSE   21               /* Win NTFS reparse point */
#define FT_PLUGIN    22               /* Plugin generated filename */
#define FT_DELETED   23               /* Deleted file entry */
#define FT_BASE      24               /* Duplicate base file entry */
#define FT_RESTORE_FIRST 25           /* Restore this "object" first */
#define FT_JUNCTION  26               /* Win32 Junction point */
#define FT_PLUGIN_CONFIG 27           /* Object for Plugin configuration */
#define FT_PLUGIN_CONFIG_FILLED 28    /* Object for Plugin configuration filled by Director */

/* Definitions for upper part of type word (see above). */
#define AR_DATA_STREAM (1<<16)        /* Data stream id present */

/* Quick way to know if a Filetype is about a plugin "Object" */
#define IS_FT_OBJECT(x) (((x) == FT_RESTORE_FIRST) || ((x) == FT_PLUGIN_CONFIG_FILLED) || ((x) == FT_PLUGIN_CONFIG))

#endif /* __BFILETYPES_H */
