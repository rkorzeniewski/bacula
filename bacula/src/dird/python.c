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


/* Return Bacula variables */
PyObject *bacula_get(PyObject *self, PyObject *args)
{
   PyObject *CObject;
   JCR *jcr;
   char *item;
   if (!PyArg_ParseTuple(args, "Os:get", &CObject, &item)) {
      return NULL;
   }
   jcr = (JCR *)PyCObject_AsVoidPtr(CObject);
   /* ***FIXME*** put this in a table */
   if (strcmp(item, "JobId") == 0) {
      return Py_BuildValue("i", jcr->JobId);
   } else if (strcmp(item, "Client") == 0) {
      return Py_BuildValue("s", jcr->client->hdr.name);
   } else if (strcmp(item, "Pool") == 0) {
      return Py_BuildValue("s", jcr->pool->hdr.name);
   } else if (strcmp(item, "Storage") == 0) {
      return Py_BuildValue("s", jcr->store->hdr.name);
   } else if (strcmp(item, "Catalog") == 0) {
      return Py_BuildValue("s", jcr->catalog->hdr.name);
   } else if (strcmp(item, "MediaType") == 0) {
      return Py_BuildValue("s", jcr->store->media_type);
   } else if (strcmp(item, "NumVols") == 0) {
      return Py_BuildValue("i", jcr->NumVols);
   } else if (strcmp(item, "DirName") == 0) {
      return Py_BuildValue("s", my_name);
   } else if (strcmp(item, "Level") == 0) {
      return Py_BuildValue("s", job_level_to_str(jcr->JobLevel));
   } else if (strcmp(item, "Type") == 0) {
      return Py_BuildValue("s", job_type_to_str(jcr->JobType));
   } else if (strcmp(item, "Job") == 0) {
      return Py_BuildValue("s", jcr->job->hdr.name);
   } else if (strcmp(item, "JobName") == 0) {
      return Py_BuildValue("s", jcr->Job);
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
