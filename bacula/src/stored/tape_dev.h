/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2014-2014 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from many
   others, a complete list can be found in the file AUTHORS.

   You may use this file and others of this release according to the
   license defined in the LICENSE file, which includes the Affero General
   Public License, v3.0 ("AGPLv3") and some additional permissions and
   terms pursuant to its AGPLv3 Section 7.

   Bacula® is a registered trademark of Kern Sibbald.
*/
/*
 * Inspired by vtape.h
 */

#ifndef __TAPE_DEV_
#define __TAPE_DEV_

class tape_dev : public DEVICE {
public:

   tape_dev() { };
   ~tape_dev() { };

   /* DEVICE virtual functions that we redefine with our tape code */
   bool fsf(int num);
   bool offline();
   bool rewind(DCR *dcr);
   bool bsf(int num);
   void lock_door();
   void unlock_door();
   bool reposition(DCR *dcr, uint32_t rfile, uint32_t rblock);
};

#endif /* __TAPE_DEV_ */
