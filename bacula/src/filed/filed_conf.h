/*
 * Bacula File Daemon specific configuration
 *
 *     Kern Sibbald, Sep MM
 */
/*
   Copyright (C) 2000, 2001, 2002 Kern Sibbald and John Walker

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

/*
 * Resource codes -- they must be sequential for indexing   
 */
#define R_FIRST 		      1001

#define R_DIRECTOR		      1001
#define R_CLIENT		      1002
#define R_MSGS			      1003

#define R_LAST			      R_MSGS

/*
 * Some resource attributes
 */
#define R_NAME			      1020
#define R_ADDRESS		      1021
#define R_PASSWORD		      1022
#define R_TYPE			      1023


/* Definition of the contents of each Resource */
struct s_res_dir {
   RES	 hdr;
   char *password;		      /* Director password */
   char *address;		      /* Director address or zero */
};
typedef struct s_res_dir DIRRES;

struct s_res_client {
   RES	 hdr;
   int	 FDport;		      /* where we listen for Directors */ 
   char *working_directory;
   char *pid_directory;
   char *subsys_directory;
};
typedef struct s_res_client CLIENT;



/* Define the Union of all the above
 * resource structure definitions.
 */
union u_res {
   struct s_res_dir	res_dir;
   struct s_res_client	res_client;
   struct s_res_msgs	res_msgs;
   RES hdr;
};

typedef union u_res URES;
