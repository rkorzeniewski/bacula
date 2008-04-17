/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

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
 * mtops.c - Emulate the Linux st (scsi tape) driver on file.
 *
 * Version $Id$
 *
 */

#include <stdarg.h>
#include <stddef.h>

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

#include "sys/mtio.h"

//
// SCSI bus status codes.
//

#define SCSISTAT_GOOD                  0x00
#define SCSISTAT_CHECK_CONDITION       0x02
#define SCSISTAT_CONDITION_MET         0x04
#define SCSISTAT_BUSY                  0x08
#define SCSISTAT_INTERMEDIATE          0x10
#define SCSISTAT_INTERMEDIATE_COND_MET 0x14
#define SCSISTAT_RESERVATION_CONFLICT  0x18
#define SCSISTAT_COMMAND_TERMINATED    0x22
#define SCSISTAT_QUEUE_FULL            0x28

static int tape_get(int fd, struct mtget *mt_get);
static int tape_op(int fd, struct mtop *mt_com);
static int tape_pos(int fd, struct mtpos *mt_pos);

int
fake_tape_open(const char *file, int flags, int mode)
{
   int fd;
   fd = open(file, flags, mode);
   return fd;
}

int
fake_tape_read(int fd, void *buffer, unsigned int count)
{
   int ret;
   if (buffer == NULL) {
      errno = EINVAL;
      return -1;
   }
   ret = read(fd, buffer, count);
   return ret;
}

int
fake_tape_write(int fd, const void *buffer, unsigned int count)
{
   int ret;
   if (buffer == NULL) {
      errno = EINVAL;
      return -1;
   }
   ret = write(fd, buffer, count);
   return ret;
}

int
fake_tape_close(int fd)
{
   close(fd);
   return 0;
}

int
fake_tape_ioctl(int fd, unsigned long int request, ...)
{
   va_list argp;
   int result;

   va_start(argp, request);

   switch (request) {
   case MTIOCTOP:
      result = tape_op(fd, va_arg(argp, mtop *));
      break;

   case MTIOCGET:
      result = tape_get(fd, va_arg(argp, mtget *));
      break;

   case MTIOCPOS:
      result = tape_pos(fd, va_arg(argp, mtpos *));
      break;

   default:
      errno = ENOTTY;
      result = -1;
      break;
   }

   va_end(argp);

   return result;
}

static int tape_op(int fd, struct mtop *mt_com)
{
   int result;

   switch (mt_com->mt_op)
   {
   case MTRESET:
   case MTNOP:
   case MTSETDRVBUFFER:
      break;

   default:
   case MTRAS1:
   case MTRAS2:
   case MTRAS3:
   case MTSETDENSITY:
      errno = ENOTTY;
      result = (DWORD)-1;
      break;

   case MTFSF:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
         if (result == NO_ERROR) {
            pHandleInfo->ulFile++;
            pHandleInfo->bEOF = true;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTBSF:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
         if (result == NO_ERROR) {
            pHandleInfo->ulFile--;
            pHandleInfo->bBlockValid = false;
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTFSR:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_RELATIVE_BLOCKS, 0, mt_com->mt_count, 0, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      } else if (result == ERROR_FILEMARK_DETECTED) {
         pHandleInfo->bEOF = true;
      }
      break;

   case MTBSR:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_RELATIVE_BLOCKS, 0, -mt_com->mt_count, ~0, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      } else if (result == ERROR_FILEMARK_DETECTED) {
         pHandleInfo->ulFile--;
         pHandleInfo->bBlockValid = false;
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
      }
      break;

   case MTWEOF:
      result = WriteTapemark(pHandleInfo->OSHandle, TAPE_FILEMARKS, mt_com->mt_count, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOF = true;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile += mt_com->mt_count;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTREW:
      if (lseek(fd, (boffset_t)0, SEEK_SET) < 0) {
         berrno be;
         dev_errno = errno;
         Mmsg2(errmsg, _("lseek error on %s. ERR=%s.\n"),
            print_name(), be.bstrerror());
         result = -1 ;
      } else {
	 result = NO_ERROR;
      }
      break;

   case MTOFFL:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOAD, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTRETEN:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_TENSION, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = false;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTBSFM:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
         if (result == NO_ERROR) {
            result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTFSFM:
      for (index = 0; index < mt_com->mt_count; index++) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, mt_com->mt_count, 0, FALSE);
         if (result == NO_ERROR) {
            result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, (DWORD)-1, ~0, FALSE);
            pHandleInfo->bEOD = false;
            pHandleInfo->bEOF = false;
            pHandleInfo->bEOT = false;
         }
      }
      break;

   case MTEOM:
      for ( ; ; ) {
         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_FILEMARKS, 0, 1, 0, FALSE);
         if (result != NO_ERROR) {
            pHandleInfo->bEOF = false;

            if (result == ERROR_END_OF_MEDIA) {
               pHandleInfo->bEOD = true;
               pHandleInfo->bEOT = true;
               return 0;
            }
            if (result == ERROR_NO_DATA_DETECTED) {
               pHandleInfo->bEOD = true;
               pHandleInfo->bEOT = false;
               return 0;
            }
            break;
         } else {
            pHandleInfo->bEOF = true;
            pHandleInfo->ulFile++;
         }
      }
      break;

   case MTERASE:
      result = EraseTape(pHandleInfo->OSHandle, TAPE_ERASE_LONG, FALSE);
      if (result == NO_ERROR) {
         pHandleInfo->bEOD = true;
         pHandleInfo->bEOF = false;
         pHandleInfo->bEOT = false;
         pHandleInfo->ulFile = 0;
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = 0;
      }
      break;

   case MTSETBLK:
      {
         TAPE_SET_MEDIA_PARAMETERS  SetMediaParameters;

         SetMediaParameters.BlockSize = mt_com->mt_count;
         result = SetTapeParameters(pHandleInfo->OSHandle, SET_TAPE_MEDIA_INFORMATION, &SetMediaParameters);
      }
      break;

   case MTSEEK:
      {
         TAPE_POSITION_INFO   TapePositionInfo;

         result = SetTapePosition(pHandleInfo->OSHandle, TAPE_ABSOLUTE_BLOCK, 0, mt_com->mt_count, 0, FALSE);

         memset(&TapePositionInfo, 0, sizeof(TapePositionInfo));
         DWORD dwPosResult = GetTapePositionInfo(pHandleInfo->OSHandle, &TapePositionInfo);
         if (dwPosResult == NO_ERROR && TapePositionInfo.FileSetValid) {
            pHandleInfo->ulFile = (ULONG)TapePositionInfo.FileNumber;
         } else {
            pHandleInfo->ulFile = ~0U;
         }
      }
      break;

   case MTTELL:
      {
         DWORD partition;
         DWORD offset;
         DWORD offsetHi;

         result = GetTapePosition(pHandleInfo->OSHandle, TAPE_ABSOLUTE_BLOCK, &partition, &offset, &offsetHi);
         if (result == NO_ERROR) {
            return offset;
         }
      }
      break;

   case MTFSS:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_SETMARKS, 0, mt_com->mt_count, 0, FALSE);
      break;

   case MTBSS:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_SPACE_SETMARKS, 0, -mt_com->mt_count, ~0, FALSE);
      break;

   case MTWSM:
      result = WriteTapemark(pHandleInfo->OSHandle, TAPE_SETMARKS, mt_com->mt_count, FALSE);
      break;

   case MTLOCK:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_LOCK, FALSE);
      break;

   case MTUNLOCK:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOCK, FALSE);
      break;

   case MTLOAD:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_LOAD, FALSE);
      break;

   case MTUNLOAD:
      result = PrepareTape(pHandleInfo->OSHandle, TAPE_UNLOAD, FALSE);
      break;

   case MTCOMPRESSION:
      {
         TAPE_GET_DRIVE_PARAMETERS  GetDriveParameters;
         TAPE_SET_DRIVE_PARAMETERS  SetDriveParameters;
         DWORD                      size;

         size = sizeof(GetDriveParameters);

         result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_DRIVE_INFORMATION, &size, &GetDriveParameters);

         if (result == NO_ERROR)
         {
            SetDriveParameters.ECC = GetDriveParameters.ECC;
            SetDriveParameters.Compression = (BOOLEAN)mt_com->mt_count;
            SetDriveParameters.DataPadding = GetDriveParameters.DataPadding;
            SetDriveParameters.ReportSetmarks = GetDriveParameters.ReportSetmarks;
            SetDriveParameters.EOTWarningZoneSize = GetDriveParameters.EOTWarningZoneSize;

            result = SetTapeParameters(pHandleInfo->OSHandle, SET_TAPE_DRIVE_INFORMATION, &SetDriveParameters);
         }
      }
      break;

   case MTSETPART:
      result = SetTapePosition(pHandleInfo->OSHandle, TAPE_LOGICAL_BLOCK, mt_com->mt_count, 0, 0, FALSE);
      break;

   case MTMKPART:
      if (mt_com->mt_count == 0)
      {
         result = CreateTapePartition(pHandleInfo->OSHandle, TAPE_INITIATOR_PARTITIONS, 1, 0);
      }
      else
      {
         result = CreateTapePartition(pHandleInfo->OSHandle, TAPE_INITIATOR_PARTITIONS, 2, mt_com->mt_count);
      }
      break;
   }

   if ((result == NO_ERROR && pHandleInfo->bEOF) || 
       (result == ERROR_FILEMARK_DETECTED && mt_com->mt_op == MTFSR)) {

      TAPE_POSITION_INFO TapePositionInfo;

      if (GetTapePositionInfo(pHandleInfo->OSHandle, &TapePositionInfo) == NO_ERROR) {
         pHandleInfo->bBlockValid = true;
         pHandleInfo->ullFileStart = TapePositionInfo.BlockNumber;
      }
   }

   switch (result) {
   case NO_ERROR:
   case (DWORD)-1:   /* Error has already been translated into errno */
      break;

   default:
   case ERROR_FILEMARK_DETECTED:
      errno = EIO;
      break;

   case ERROR_END_OF_MEDIA:
      pHandleInfo->bEOT = true;
      errno = EIO;
      break;

   case ERROR_NO_DATA_DETECTED:
      pHandleInfo->bEOD = true;
      errno = EIO;
      break;

   case ERROR_NO_MEDIA_IN_DRIVE:
      pHandleInfo->bEOF = false;
      pHandleInfo->bEOT = false;
      pHandleInfo->bEOD = false;
      errno = ENOMEDIUM;
      break;

   case ERROR_INVALID_HANDLE:
   case ERROR_ACCESS_DENIED:
   case ERROR_LOCK_VIOLATION:
      errno = EBADF;
      break;
   }

   return result == NO_ERROR ? 0 : -1;
}

static int tape_get(int fd, struct mtget *mt_get)
{
   TAPE_POSITION_INFO pos_info;
   BOOL result;

   if (fd < 3 || fd >= (int)(NUMBER_HANDLE_ENTRIES + 3) || 
       TapeHandleTable[fd - 3].OSHandle == INVALID_HANDLE_VALUE) {
      errno = EBADF;
      return -1;
   }

   PTAPE_HANDLE_INFO    pHandleInfo = &TapeHandleTable[fd - 3];

   if (GetTapePositionInfo(pHandleInfo->OSHandle, &pos_info) != NO_ERROR) {
      return -1;
   }

   DWORD density = 0;
   DWORD blocksize = 0;

   result = GetDensityBlockSize(pHandleInfo->OSHandle, &density, &blocksize);

   if (result != NO_ERROR) {
      TAPE_GET_DRIVE_PARAMETERS drive_params;
      DWORD size;

      size = sizeof(drive_params);

      result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_DRIVE_INFORMATION, &size, &drive_params);

      if (result == NO_ERROR) {
         blocksize = drive_params.DefaultBlockSize;
      }
   }

   mt_get->mt_type = MT_ISSCSI2;

   // Partition #
   mt_get->mt_resid = pos_info.PartitionBlockValid ? pos_info.Partition : (ULONG)-1;

   // Density / Block Size
   mt_get->mt_dsreg = ((density << MT_ST_DENSITY_SHIFT) & MT_ST_DENSITY_MASK) |
                      ((blocksize << MT_ST_BLKSIZE_SHIFT) & MT_ST_BLKSIZE_MASK);

   mt_get->mt_gstat = 0x00010000;  /* Immediate report mode.*/

   if (pHandleInfo->bEOF) {
      mt_get->mt_gstat |= 0x80000000;     // GMT_EOF
   }

   if (pos_info.PartitionBlockValid && pos_info.BlockNumber == 0) {
      mt_get->mt_gstat |= 0x40000000;     // GMT_BOT
   }

   if (pHandleInfo->bEOT) {
      mt_get->mt_gstat |= 0x20000000;     // GMT_EOT
   }

   if (pHandleInfo->bEOD) {
      mt_get->mt_gstat |= 0x08000000;     // GMT_EOD
   }

   TAPE_GET_MEDIA_PARAMETERS  media_params;
   DWORD size = sizeof(media_params);
   
   result = GetTapeParameters(pHandleInfo->OSHandle, GET_TAPE_MEDIA_INFORMATION, &size, &media_params);

   if (result == NO_ERROR && media_params.WriteProtected) {
      mt_get->mt_gstat |= 0x04000000;     // GMT_WR_PROT
   }

   result = GetTapeStatus(pHandleInfo->OSHandle);

   if (result != NO_ERROR) {
      if (result == ERROR_NO_MEDIA_IN_DRIVE) {
         mt_get->mt_gstat |= 0x00040000;  // GMT_DR_OPEN
      }
   } else {
      mt_get->mt_gstat |= 0x01000000;     // GMT_ONLINE
   }

   // Recovered Error Count
   mt_get->mt_erreg = 0;

   // File Number
   mt_get->mt_fileno = (__daddr_t)pHandleInfo->ulFile;

   // Block Number
   mt_get->mt_blkno = (__daddr_t)(pHandleInfo->bBlockValid ? pos_info.BlockNumber - pHandleInfo->ullFileStart : (ULONGLONG)-1);

   return 0;
}
