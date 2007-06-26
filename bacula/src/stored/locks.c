/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
 * Collection of Bacula Storage daemon locking software
 *
 *  Kern Sibbald, 2000-2007.  June 2007
 *
 *   Version $Id$
 */

#include "bacula.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

#ifdef SD_DEBUG_LOCK
const int dbglvl = 0;
#else
const int dbglvl = 500;
#endif


void DEVICE::block(int why)
{
   r_dlock();              /* need recursive lock to block */
   block_device(this, why);
   r_dunlock();
}

void DEVICE::unblock(bool locked)
{
   if (!locked) {
      dlock();
   }
   unblock_device(this);
   dunlock();
}


#ifdef SD_DEBUG_LOCK
void DEVICE::_dlock(const char *file, int line)
{
   Dmsg4(sd_dbglvl, "dlock from %s:%d precnt=%d JobId=%u\n", file, line,
         m_count, get_jobid_from_tid()); 
   /* Note, this *really* should be protected by a mutex, but
    *  since it is only debug code we don't worry too much.  
    */
   if (m_count > 0 && pthread_equal(m_pid, pthread_self())) {
      Dmsg5(sd_dbglvl, "Possible DEADLOCK!! lock held by JobId=%u from %s:%d m_count=%d JobId=%u\n", 
            get_jobid_from_tid(m_pid),
            file, line, m_count, get_jobid_from_tid());
   }
   P(m_mutex);
   m_pid = pthread_self();
   m_count++; 
}

void DEVICE::_dunlock(const char *file, int line)
{
   m_count--; 
   Dmsg4(sd_dbglvl+1, "dunlock from %s:%d postcnt=%d JobId=%u\n", file, line,
         m_count, get_jobid_from_tid()); 
   V(m_mutex);   
}

void DEVICE::_r_dunlock(const char *file, int line)
{
   this->_dunlock(file, line);
}

#endif


/*
 * This is a recursive lock that checks if the device is blocked.
 *
 * When blocked is set, all threads EXCEPT thread with id no_wait_id
 * must wait. The no_wait_id thread is out obtaining a new volume
 * and preparing the label.
 */
#ifdef SD_DEBUG_LOCK
void DEVICE::_r_dlock(const char *file, int line)
#else
void DEVICE::r_dlock()
#endif
{
   int stat;
#ifdef SD_DEBUG_LOCK
   Dmsg4(sd_dbglvl+1, "r_dlock blked=%s from %s:%d JobId=%u\n", this->print_blocked(),
         file, line, get_jobid_from_tid());
#else
   Dmsg1(sd_dbglvl, "reclock blked=%s\n", this->print_blocked());
#endif
   this->dlock();   
   if (this->blocked() && !pthread_equal(this->no_wait_id, pthread_self())) {
      this->num_waiting++;             /* indicate that I am waiting */
      while (this->blocked()) {
         Dmsg3(sd_dbglvl, "r_dlock blked=%s no_wait=%p me=%p\n", this->print_blocked(),
               this->no_wait_id, pthread_self());
         if ((stat = pthread_cond_wait(&this->wait, &m_mutex)) != 0) {
            berrno be;
            this->dunlock();
            Emsg1(M_ABORT, 0, _("pthread_cond_wait failure. ERR=%s\n"),
               be.bstrerror(stat));
         }
      }
      this->num_waiting--;             /* no longer waiting */
   }
}

/*
 * Block all other threads from using the device
 *  Device must already be locked.  After this call,
 *  the device is blocked to any thread calling dev->r_lock(),
 *  but the device is not locked (i.e. no P on device).  Also,
 *  the current thread can do slip through the dev->r_lock()
 *  calls without blocking.
 */
void _block_device(const char *file, int line, DEVICE *dev, int state)
{
   ASSERT(dev->blocked() == BST_NOT_BLOCKED);
   dev->set_blocked(state);           /* make other threads wait */
   dev->no_wait_id = pthread_self();  /* allow us to continue */
   Dmsg3(sd_dbglvl, "set blocked=%s from %s:%d\n", dev->print_blocked(), file, line);
}

/*
 * Unblock the device, and wake up anyone who went to sleep.
 * Enter: device locked
 * Exit:  device locked
 */
void _unblock_device(const char *file, int line, DEVICE *dev)
{
   Dmsg3(sd_dbglvl, "unblock %s from %s:%d\n", dev->print_blocked(), file, line);
   ASSERT(dev->blocked());
   dev->set_blocked(BST_NOT_BLOCKED);
   dev->no_wait_id = 0;
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

/*
 * Enter with device locked and blocked
 * Exit with device unlocked and blocked by us.
 */
void _steal_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold, int state)
{

   Dmsg3(sd_dbglvl, "steal lock. old=%s from %s:%d\n", dev->print_blocked(),
      file, line);
   hold->dev_blocked = dev->blocked();
   hold->dev_prev_blocked = dev->dev_prev_blocked;
   hold->no_wait_id = dev->no_wait_id;
   dev->set_blocked(state);
   Dmsg1(sd_dbglvl, "steal lock. new=%s\n", dev->print_blocked());
   dev->no_wait_id = pthread_self();
   dev->dunlock();
}

/*
 * Enter with device blocked by us but not locked
 * Exit with device locked, and blocked by previous owner
 */
void _give_back_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold)
{
   Dmsg3(sd_dbglvl, "return lock. old=%s from %s:%d\n",
      dev->print_blocked(), file, line);
   dev->dlock();
   dev->set_blocked(hold->dev_blocked);
   dev->dev_prev_blocked = hold->dev_prev_blocked;
   dev->no_wait_id = hold->no_wait_id;
   Dmsg1(sd_dbglvl, "return lock. new=%s\n", dev->print_blocked());
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

const char *DEVICE::print_blocked() const 
{
   switch (m_blocked) {
   case BST_NOT_BLOCKED:
      return "BST_NOT_BLOCKED";
   case BST_UNMOUNTED:
      return "BST_UNMOUNTED";
   case BST_WAITING_FOR_SYSOP:
      return "BST_WAITING_FOR_SYSOP";
   case BST_DOING_ACQUIRE:
      return "BST_DOING_ACQUIRE";
   case BST_WRITING_LABEL:
      return "BST_WRITING_LABEL";
   case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      return "BST_UNMOUNTED_WAITING_FOR_SYSOP";
   case BST_MOUNT:
      return "BST_MOUNT";
   default:
      return _("unknown blocked code");
   }
}


/*
 * Check if the device is blocked or not
 */
bool is_device_unmounted(DEVICE *dev)
{
   bool stat;
   int blocked = dev->blocked();
   stat = (blocked == BST_UNMOUNTED) ||
          (blocked == BST_UNMOUNTED_WAITING_FOR_SYSOP);
   return stat;
}

