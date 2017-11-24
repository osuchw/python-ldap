/* constants defined for LDAP
 * See https://www.python-ldap.org/ for details. */

#include "common.h"
#include "constants.h"
#include "lber.h"
#include "ldap.h"

/* the base exception class */

PyObject*
LDAPexception_class;

/* list of exception classes */

#define LDAP_ERROR_MIN          LDAP_REFERRAL_LIMIT_EXCEEDED

#ifdef LDAP_PROXIED_AUTHORIZATION_DENIED
  #define LDAP_ERROR_MAX          LDAP_PROXIED_AUTHORIZATION_DENIED
#else
  #ifdef LDAP_ASSERTION_FAILED
    #define LDAP_ERROR_MAX          LDAP_ASSERTION_FAILED
  #else
    #define LDAP_ERROR_MAX          LDAP_OTHER
  #endif
#endif

#define LDAP_ERROR_OFFSET       -LDAP_ERROR_MIN

static PyObject* errobjects[ LDAP_ERROR_MAX-LDAP_ERROR_MIN+1 ];


/* Convert a bare LDAP error number into an exception */
PyObject*
LDAPerr(int errnum)
{
  if (errnum >= LDAP_ERROR_MIN && errnum <= LDAP_ERROR_MAX)
    PyErr_SetNone(errobjects[errnum+LDAP_ERROR_OFFSET]);
  else
    PyErr_SetObject(LDAPexception_class,
    Py_BuildValue("{s:i}", "errnum", errnum));
  return NULL;
}

/* Convert an LDAP error into an informative python exception */
PyObject*
LDAPerror( LDAP *l, char *msg )
{
  if (l == NULL) {
    PyErr_SetFromErrno( LDAPexception_class );
    return NULL;
  }
  else {
    int myerrno, errnum, opt_errnum;
    PyObject *errobj;
    PyObject *info;
    PyObject *str;
    PyObject *pyerrno;

    /* at first save errno for later use before it gets overwritten by another call */
    myerrno = errno;

    char *matched, *error;

    opt_errnum = ldap_get_option(l, LDAP_OPT_ERROR_NUMBER, &errnum);
    if (opt_errnum != LDAP_OPT_SUCCESS)
      errnum = opt_errnum;

    if (errnum == LDAP_NO_MEMORY)
      return PyErr_NoMemory();

    if (errnum >= LDAP_ERROR_MIN && errnum <= LDAP_ERROR_MAX)
      errobj = errobjects[errnum+LDAP_ERROR_OFFSET];
    else
      errobj = LDAPexception_class;

    info = PyDict_New();
    if (info == NULL)
      return NULL;

    str = PyUnicode_FromString(ldap_err2string(errnum));
    if (str)
      PyDict_SetItemString( info, "desc", str );
    Py_XDECREF(str);

    if (myerrno != 0) {
        pyerrno = PyInt_FromLong(myerrno);
        if (pyerrno)
            PyDict_SetItemString( info, "errno", pyerrno );
        Py_XDECREF(pyerrno);
    }

    if (ldap_get_option(l, LDAP_OPT_MATCHED_DN, &matched) >= 0
      && matched != NULL) {
        if (*matched != '\0') {
      str = PyUnicode_FromString(matched);
      if (str)
          PyDict_SetItemString( info, "matched", str );
      Py_XDECREF(str);
        }
        ldap_memfree(matched);
    }

    if (errnum == LDAP_REFERRAL) {
        str = PyUnicode_FromString(msg);
        if (str)
      PyDict_SetItemString( info, "info", str );
        Py_XDECREF(str);
    } else if (ldap_get_option(l, LDAP_OPT_ERROR_STRING, &error) >= 0) {
        if (error != NULL && *error != '\0') {
            str = PyUnicode_FromString(error);
            if (str)
                PyDict_SetItemString( info, "info", str );
            Py_XDECREF(str);
        }
        ldap_memfree(error);
    }
    PyErr_SetObject( errobj, info );
    Py_DECREF(info);
    return NULL;
  }
}

/* initialise the module constants */

int
LDAPinit_constants( PyObject* m )
{
    PyObject *exc;
    int result;

#define check_result() {  \
    if (result != 0) return -1;  \
}

    /* simple constants */

    result = PyModule_AddIntConstant(m, "OPT_ON", 1);
    check_result();
    result = PyModule_AddIntConstant(m, "OPT_OFF", 0);
    check_result();

    /* exceptions */

    LDAPexception_class = PyErr_NewException("ldap.LDAPError", NULL, NULL);
    if (LDAPexception_class == NULL) {
        return -1;
    }

    result = PyModule_AddObject(m, "LDAPError", LDAPexception_class);
    check_result();
    Py_INCREF(LDAPexception_class);

    /* XXX - backward compatibility with pre-1.8 */
    result = PyModule_AddObject(m, "error", LDAPexception_class);
    check_result();
    Py_INCREF(LDAPexception_class);

    /* Generated constants -- see Lib/ldap/constants.py */

#define seterrobj2(n, o) \
    PyModule_AddObject(m, #n, (errobjects[LDAP_##n+LDAP_ERROR_OFFSET] = o))

#define add_err(n) {  \
    exc = PyErr_NewException("ldap." #n, LDAPexception_class, NULL);  \
    if (exc == NULL) return -1;  \
    result = seterrobj2(n, exc);  \
    check_result();  \
    Py_INCREF(exc);  \
}

#define add_int(n) {  \
    result = PyModule_AddIntConstant(m, #n, LDAP_##n);  \
    check_result();  \
}

#define add_string(n) {  \
    result = PyModule_AddStringConstant(m, #n, LDAP_##n);  \
    check_result();  \
}

#include "constants_generated.h"

    return 0;
}
