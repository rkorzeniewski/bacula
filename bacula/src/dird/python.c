/*
 * 
 * Bacula interface to Python for the Director
 *
 * Kern Sibbald, November MMIV
 *
 *   Version $Id$
 *
 */

/*
   
   Copyright (C) 2004 Kern Sibbald

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
#include "dird.h"

#ifdef HAVE_PYTHON
#include <Python.h>

bool run_module(const char *module);

PyObject *bacula_get(PyObject *self, PyObject *args);
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw);

/* Define Bacula entry points */
PyMethodDef BaculaMethods[] = {
    {"get", bacula_get, METH_VARARGS, "Get Bacula variables."},
    {"set", (PyCFunction)bacula_set, METH_VARARGS|METH_KEYWORDS,
        "Set Bacula variables."}, 
    {NULL, NULL, 0, NULL}	      /* last item */
};


struct s_vars {
   const char *name;
   char *fmt;
};     

static struct s_vars vars[] = {
   { N_("Job"),        "s"},
   { N_("Dir"),        "s"},
   { N_("Level"),      "s"},
   { N_("Type"),       "s"},
   { N_("JobId"),      "i"},
   { N_("Client"),     "s"},
   { N_("NumVols"),    "i"},
   { N_("Pool"),       "s"},
   { N_("Storage"),    "s"},
   { N_("Catalog"),    "s"},
   { N_("MediaType"),  "s"},
   { N_("JobName"),    "s"},
   { N_("JobStatus"),  "s"},
   
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
   case 0:			      /* Job */
      return Py_BuildValue(vars[i].fmt, jcr->job->hdr.name);
   case 1:                            /* Director's name */
      return Py_BuildValue(vars[i].fmt, my_name);
   case 2:			      /* level */
      return Py_BuildValue(vars[i].fmt, job_level_to_str(jcr->JobLevel));
   case 3:			      /* type */
      return Py_BuildValue(vars[i].fmt, job_type_to_str(jcr->JobType));
   case 4:			      /* JobId */
      return Py_BuildValue(vars[i].fmt, jcr->JobId);
   case 5:			      /* Client */
      return Py_BuildValue(vars[i].fmt, jcr->client->hdr.name);
   case 6:			      /* NumVols */
      return Py_BuildValue(vars[i].fmt, jcr->NumVols);
   case 7:			      /* Pool */
      return Py_BuildValue(vars[i].fmt, jcr->pool->hdr.name);
   case 8:			      /* Storage */
      return Py_BuildValue(vars[i].fmt, jcr->store->hdr.name);
   case 9:
      return Py_BuildValue(vars[i].fmt, jcr->catalog->hdr.name);
   case 10:			      /* MediaType */
      return Py_BuildValue(vars[i].fmt, jcr->store->media_type);
   case 11:			      /* JobName */
      return Py_BuildValue(vars[i].fmt, jcr->Job);
   case 12:			      /* JobStatus */
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
   char *VolumeName = NULL;
   static char *kwlist[] = {"jcr", "JobReport", "VolumeName", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, keyw, "O|ss:set", kwlist, 
	&CObject, &msg, &VolumeName)) {
      return NULL;
   }
   jcr = (JCR *)PyCObject_AsVoidPtr(CObject);

   if (msg) {
      Jmsg(jcr, M_INFO, 0, "%s", msg);
   }
   if (VolumeName) {
      pm_strcpy(jcr->VolumeName, VolumeName);
   }
   return Py_BuildValue("i", 1);
}

#endif /* HAVE_PYTHON */
