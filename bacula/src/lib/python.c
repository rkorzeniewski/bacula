/*
 * 
 * Bacula common code library interface to Python
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

#ifdef HAVE_PYTHON
#include <Python.h>


PyObject *bacula_get(PyObject *self, PyObject *args);
PyObject *bacula_set(PyObject *self, PyObject *args, PyObject *keyw);

/* Pull in Bacula entry points */
extern PyMethodDef BaculaMethods[];


/* Start the interpreter */
void init_python_interpreter(const char *progname, const char *scripts)
{
   char buf[MAXSTRING];
   Py_SetProgramName((char *)progname);
   Py_Initialize();
   PyEval_InitThreads();
   Py_InitModule("bacula", BaculaMethods);
   bsnprintf(buf, sizeof(buf), "import sys\n"
            "sys.path.append('%s')\n", scripts);
   PyRun_SimpleString(buf);
   PyEval_ReleaseLock();
}

void term_python_interpreter()
{
   Py_Finalize();
}


/* 
 * Generate and process a Bacula event by importing a Python
 *  module and running it.
 *
 *  Returns: 0 if Python not configured or module not found
 *	    -1 on Python error
 *	     1 OK
 */
int generate_event(JCR *jcr, const char *event)
{
   PyObject *pName, *pModule, *pDict, *pFunc;
   PyObject *pArgs, *pValue;

   pName = PyString_FromString(event);
   if (!pName) {
      Jmsg(jcr, M_ERROR, 0, "Could not convert \"%s\" to Python string.\n", event);
      return -1;		      /* Could not convert string */
   }

   pModule = PyImport_Import(pName);
   Py_DECREF(pName);		      /* release pName */

   if (pModule != NULL) {
      pDict = PyModule_GetDict(pModule);
      /* pDict is a borrowed reference */

      pFunc = PyDict_GetItemString(pDict, (char *)event);
      /* pFun: Borrowed reference */

      if (pFunc && PyCallable_Check(pFunc)) {
	  /* Create JCR argument to send to function */
	  pArgs = PyTuple_New(1);
	  pValue = PyCObject_FromVoidPtr((void *)jcr, NULL);
	  if (!pValue) {
	     Py_DECREF(pArgs);
	     Py_DECREF(pModule);
             Jmsg(jcr, M_ERROR, 0, "Could not convert JCR to Python CObject.\n");
	     return -1; 	      /* Could not convert JCR to CObject */
	  }
	  /* pValue reference stolen here: */
	  PyTuple_SetItem(pArgs, 0, pValue);

	  /* Finally, we call the module here */
	  pValue = PyObject_CallObject(pFunc, pArgs);
	  Py_DECREF(pArgs);
	  if (pValue != NULL) {
	     Py_DECREF(pValue);
	  } else {
	     Py_DECREF(pModule);
	     PyErr_Print();
             Jmsg(jcr, M_ERROR, 0, "Error running Python module: %s\n", event);
	     return 0;		      /* error running function */
	  }
	  /* pDict and pFunc are borrowed and must not be Py_DECREF-ed */
      } else {
	 if (PyErr_Occurred()) {
	    PyErr_Print();
	 }
         Jmsg(jcr, M_ERROR, 0, "Python function \"%s\" not found in module.\n", event);
	 return -1;		      /* function not found */ 
      }
      Py_DECREF(pModule);
   } else {
      return 0; 		      /* Module not present */
   }
   return 1;
}

#else

/*
 *  No Python configured
 */

int generate_event(JCR *jcr, const char *event) { return 0; }
void init_python_interpreter(char *progname) { }
void term_python_interpreter() { }


#endif /* HAVE_PYTHON */
