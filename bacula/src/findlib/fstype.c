/*
 *  Implement routines to determine file system types.
 *
 *   Written by Preben 'Peppe' Guldberg, December MMIV
 *
 *   Version $Id$
 */

/*
   Copyright (C) Kern Sibbald

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

 */

#ifndef TEST_PROGRAM
#include "bacula.h"
#include "find.h"
#else /* Set up for testing a stand alone program */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SUPPORTEDOSES      "HAVE_DARWIN_OS\n" \
			   "HAVE_FREEBSD_OS\n" \
			   "HAVE_HPUX_OS\n" \
			   "HAVE_IRIX_OS\n" \
			   "HAVE_LINUX_OS\n" \
			   "HAVE_NETBSD_OS\n" \
			   "HAVE_OPENBSD_OS\n" \
			   "HAVE_SUN_OS\n"
#define POOLMEM 	   char
#define bstrdup 	   strdup
#define Dmsg0(n,s)         fprintf(stderr, s);
#define Dmsg1(n,s,a1)      fprintf(stderr, s, a1);
#define Dmsg2(n,s,a1,a2)   fprintf(stderr, s, a1, a2);
#endif

/*
 * These functions should be implemented for each OS
 *
 *	 POOLMEM *fstype(const char *fname);
 */
#if defined(HAVE_DARWIN_OS) \
   || defined(HAVE_FREEBSD_OS ) \
   || defined(HAVE_NETBSD_OS) \
   || defined(HAVE_OPENBSD_OS)
#include <sys/param.h>
#include <sys/mount.h>
POOLMEM *fstype(const char *fname)
{
   struct statfs st;
   if (statfs(fname, &st) == 0) {
      return bstrdup(st.f_fstypename);
   }
   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return NULL;
}

#elif defined(HAVE_HPUX_OS) \
   || defined(HAVE_IRIX_OS)
#include <sys/types.h>
#include <sys/statvfs.h>
POOLMEM *fstype(const char *fname)
{
   struct statvfs st;
   if (statvfs(fname, &st) == 0) {
      return bstrdup(st.f_basetype);
   }
   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return NULL;
}

#elif defined(HAVE_LINUX_OS)
#include <sys/vfs.h>
POOLMEM *fstype(const char *fname)
{
   struct statfs st;
   if (statfs(fname, &st) == 0) {
      /*
       * Values nicked from statfs(2), testing and
       *
       *    $ grep -r SUPER_MAGIC /usr/include/linux
       *
       * Entries are sorted on ("fsname")
       */
      switch (st.f_type) {

#if 0	    /* These need confirmation */
      case 0xadf5:         return bstrdup("adfs");          /* ADFS_SUPER_MAGIC */
      case 0xadff:         return bstrdup("affs");          /* AFFS_SUPER_MAGIC */
      case 0x6B414653:     return bstrdup("afs");           /* AFS_FS_MAGIC */
      case 0x0187:         return bstrdup("autofs");        /* AUTOFS_SUPER_MAGIC */
      case 0x62646576:     return bstrdup("bdev");          /* ??? */
      case 0x42465331:     return bstrdup("befs");          /* BEFS_SUPER_MAGIC */
      case 0x1BADFACE:     return bstrdup("bfs");           /* BFS_MAGIC */
      case 0x42494e4d:     return bstrdup("binfmt_misc");   /* ??? */
      case (('C'<<8)|'N'): return bstrdup("capifs");        /* CAPIFS_SUPER_MAGIC */
      case 0xFF534D42:     return bstrdup("cifs");          /* CIFS_MAGIC_NUMBER */
      case 0x73757245:     return bstrdup("coda");          /* CODA_SUPER_MAGIC */
      case 0x012ff7b7:     return bstrdup("coherent");      /* COH_SUPER_MAGIC */
      case 0x28cd3d45:     return bstrdup("cramfs");        /* CRAMFS_MAGIC */
      case 0x1373:         return bstrdup("devfs");         /* DEVFS_SUPER_MAGIC */
      case 0x1cd1:         return bstrdup("devpts");        /* ??? */
      case 0x414A53:       return bstrdup("efs");           /* EFS_SUPER_MAGIC */
      case 0x03111965:     return bstrdup("eventpollfs");   /* EVENTPOLLFS_MAGIC */
      case 0x137d:         return bstrdup("ext");           /* EXT_SUPER_MAGIC */
      case 0xef51:         return bstrdup("ext2");          /* EXT2_OLD_SUPER_MAGIC */
      case 0xBAD1DEA:      return bstrdup("futexfs");       /* ??? */
      case 0xaee71ee7:     return bstrdup("gadgetfs");      /* GADGETFS_MAGIC */
      case 0x00c0ffee:     return bstrdup("hostfs");        /* HOSTFS_SUPER_MAGIC */
      case 0xf995e849:     return bstrdup("hpfs");          /* HPFS_SUPER_MAGIC */
      case 0xb00000ee:     return bstrdup("hppfs");         /* HPPFS_SUPER_MAGIC */
      case 0x958458f6:     return bstrdup("hugetlbfs");     /* HUGETLBFS_MAGIC */
      case 0x12061983:     return bstrdup("hwgfs");         /* HWGFS_MAGIC */
      case 0x66726f67:     return bstrdup("ibmasmfs");      /* IBMASMFS_MAGIC */
      case 0x9660:         return bstrdup("iso9660");       /* ISOFS_SUPER_MAGIC */
      case 0x9660:         return bstrdup("isofs");         /* ISOFS_SUPER_MAGIC */
      case 0x07c0:         return bstrdup("jffs");          /* JFFS_MAGIC_SB_BITMASK */
      case 0x72b6:         return bstrdup("jffs2");         /* JFFS2_SUPER_MAGIC */
      case 0x3153464a:     return bstrdup("jfs");           /* JFS_SUPER_MAGIC */
      case 0x2468:         return bstrdup("minix");         /* MINIX2_SUPER_MAGIC */
      case 0x2478:         return bstrdup("minix");         /* MINIX2_SUPER_MAGIC2 */
      case 0x137f:         return bstrdup("minix");         /* MINIX_SUPER_MAGIC */
      case 0x138f:         return bstrdup("minix");         /* MINIX_SUPER_MAGIC2 */
      case 0x19800202:     return bstrdup("mqueue");        /* MQUEUE_MAGIC */
      case 0x4d44:         return bstrdup("msdos");         /* MSDOS_SUPER_MAGIC */
      case 0x564c:         return bstrdup("ncpfs");         /* NCP_SUPER_MAGIC */
      case 0x6969:         return bstrdup("nfs");           /* NFS_SUPER_MAGIC */
      case 0x5346544e:     return bstrdup("ntfs");          /* NTFS_SB_MAGIC */
      case 0x9fa1:         return bstrdup("openpromfs");    /* OPENPROM_SUPER_MAGIC */
      case 0x6f70726f:     return bstrdup("oprofilefs");    /* OPROFILEFS_MAGIC */
      case 0xa0b4d889:     return bstrdup("pfmfs");         /* PFMFS_MAGIC */
      case 0x50495045:     return bstrdup("pipfs");         /* PIPEFS_MAGIC */
      case 0x9fa0:         return bstrdup("proc");          /* PROC_SUPER_MAGIC */
      case 0x002f:         return bstrdup("qnx4");          /* QNX4_SUPER_MAGIC */
      case 0x858458f6:     return bstrdup("ramfs");         /* RAMFS_MAGIC */
      case 0x52654973:     return bstrdup("reiserfs");      /* REISERFS_SUPER_MAGIC */
      case 0x7275:         return bstrdup("romfs");         /* ROMFS_MAGIC */
      case 0x858458f6:     return bstrdup("rootfs");        /* RAMFS_MAGIC */
      case 0x67596969:     return bstrdup("rpc_pipefs");    /* RPCAUTH_GSSMAGIC */
      case 0x517B:         return bstrdup("smbfs");         /* SMB_SUPER_MAGIC */
      case 0x534F434B:     return bstrdup("sockfs");        /* SOCKFS_MAGIC */
      case 0x62656572:     return bstrdup("sysfs");         /* SYSFS_MAGIC */
      case 0x012ff7b6:     return bstrdup("sysv2");         /* SYSV2_SUPER_MAGIC */
      case 0x012ff7b5:     return bstrdup("sysv4");         /* SYSV4_SUPER_MAGIC */
      case 0x858458f6:     return bstrdup("tmpfs");         /* RAMFS_MAGIC */
      case 0x01021994:     return bstrdup("tmpfs");         /* TMPFS_MAGIC */
      case 0x15013346:     return bstrdup("udf");           /* UDF_SUPER_MAGIC */
      case 0x00011954:     return bstrdup("ufs");           /* UFS_MAGIC */
      case 0x9fa2:         return bstrdup("usbdevfs");      /* USBDEVICE_SUPER_MAGIC */
      case 0xa501FCF5:     return bstrdup("vxfs");          /* VXFS_SUPER_MAGIC */
      case 0x012ff7b4:     return bstrdup("xenix");         /* XENIX_SUPER_MAGIC */
      case 0x58465342:     return bstrdup("xfs");           /* XFS_SB_MAGIC */
      case 0x012fd16d:     return bstrdup("xiafs");         /* _XIAFS_SUPER_MAGIC */

      case 0xef53:         return bstrdup("ext2");          /* EXT2_SUPER_MAGIC */
   /* case 0xef53:	   ext2 and ext3 are the same */    /* EXT3_SUPER_MAGIC */
#else	    /* Known good values */
#endif

      default:
	 Dmsg2(10, "Unknown file system type \"0x%x\" for \"%s\".\n", st.f_type,
	       fname);
	 return NULL;
      }
   }
   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return NULL;
}

#elif defined(HAVE_SUN_OS)
#include <sys/types.h>
#include <sys/stat.h>
POOLMEM *fstype(const char *fname)
{
   struct stat st;
   if (lstat(fname, &st) == 0) {
      return bstrdup(st.st_fstype);
   }
   Dmsg1(50, "lstat() failed for \"%s\"\n", fname);
   return NULL;
}

#else	 /* No recognised OS */
POOLMEM *fstype(const char *fname)
{
   Dmsg0(10, "!!! fstype() not implemented for this OS. !!!\n");
#ifdef TEST_PROGRAM
   Dmsg1(10, "Please define one of the following when compiling:\n\n%s\n",
	 SUPPORTEDOSES);
   exit(EXIT_FAILURE);
#endif
   return NULL;
}
#endif

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
   char *p;
   int status = 0;

   if (argc < 2) {
      p = (argc < 1) ? "fstype" : argv[0];
      printf("usage:\t%s path ...\n"
	    "\t%s prints the file system type and pathname of the paths.\n",
	    p, p);
      return EXIT_FAILURE;
   }
   while (*++argv) {
      if ((p = fstype(*argv)) == NULL) {
	 status = EXIT_FAILURE;
      } else {
	 printf("%s\t%s\n", p, *argv);
      }
   }
   return status;
}
#endif
