/*
 *
 * Bacula interface to Python for the File Daemon
 *
 * Kern Sibbald, March MMV
 *
 *   Version $Id$
 *
 */

/*
   Copyright (C) 2005 Kern Sibbald

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

#include "bacula.h"
#include "filed.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

PyObject *bacula_get(PyObject *self, PyObject *args);
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw);

/* Define Bacula entry points */
PyMethodDef BaculaMethods[] = {
    {"get", bacula_get, METH_VARARGS, "Get Bacula FD variables."},
    {"set", (PyCFunction)bacula_set, METH_VARARGS|METH_KEYWORDS,
        "Set FD Bacula variables."},
    {NULL, NULL, 0, NULL}	      /* last item */
};


struct s_vars {
   const char *name;
   char *fmt;
};

static struct s_vars vars[] = {
   { N_("FDName"),     "s"},          /* 0 */
   { N_("Level"),      "s"},          /* 1 */
   { N_("Type"),       "s"},          /* 2 */
   { N_("JobId"),      "i"},          /* 3 */
   { N_("Client"),     "s"},          /* 4 */
   { N_("JobName"),    "s"},          /* 5 */
   { N_("JobStatus"),  "s"},          /* 6 */

   { NULL,	       NULL}
};

/* Return Bacula variables */
PyObject *bacula_get(PyObject *self, PyObject *args)
{
   PyObject *CObject;
   JCR *jcr;
   char *item;
   bool found = false;
   int i;
   char buf[10];

   if (!PyArg_ParseTuple(args, "Os:get", &CObject, &item)) {
      return NULL;
   }
   jcr = (JCR *)PyCObject_AsVoidPtr(CObject);
   for (i=0; vars[i].name; i++) {
      if (strcmp(vars[i].name, item) == 0) {
	 found = true;
	 break;
      }
   }
   if (!found) {
      return NULL;
   }
   switch (i) {
   case 0:                            /* FD's name */
      return Py_BuildValue(vars[i].fmt, my_name);
   case 1:			      /* level */
      return Py_BuildValue(vars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 2:			      /* type */
      return Py_BuildValue(vars[i].fmt, job_type_to_str(jcr->JobType));
   case 3:			      /* JobId */
      return Py_BuildValue(vars[i].fmt, jcr->JobId);
   case 4:			      /* Client */
      return Py_BuildValue(vars[i].fmt, jcr->client_name);
   case 5:			      /* JobName */
      return Py_BuildValue(vars[i].fmt, jcr->Job);
   case 6:			      /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue(vars[i].fmt, buf);
   }
   return NULL;
}

/* Set Bacula variables */
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw)
{
   PyObject *CObject;
   JCR *jcr;
   char *msg = NULL;
   static char *kwlist[] = {"jcr", "JobReport", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "O|ss:set", kwlist,
	&CObject, &msg)) {
      return NULL;
   }
   jcr = (JCR *)PyCObject_AsVoidPtr(CObject);

   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
   }
   return Py_BuildValue("i", 1);
}

#endif /* HAVE_PYTHON */
