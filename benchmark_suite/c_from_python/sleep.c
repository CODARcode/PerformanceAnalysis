#include <unistd.h>
#include <Python.h>
#include <stdio.h>

//The internal function is the one that will have the anomaly
void sleep_c_internal(int input){
  printf("sleep_c_internal called with sleep time %d s\n", input);
  sleep(input);
}

//External Python interface "sleep_c"
static PyObject* sleeper_c(PyObject* self, PyObject* args){
	int input;

	if(!PyArg_ParseTuple(args, "i", &input))
			return NULL;

	sleep_c_internal(input);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef sleeper_funcs[] = {
	{ "sleep_c", sleeper_c, METH_VARARGS, "sleeps"},
	{ NULL, NULL, 0, NULL}
};

static struct PyModuleDef sleepermodule = {
    PyModuleDef_HEAD_INIT,  /* m_base */
    "sleep_c",              /* m_name */
    NULL,                   /* m_doc */
    -1,                     /* m_size */
    sleeper_funcs           /* m_methods */
};

PyMODINIT_FUNC PyInit_sleeper(void) {
	return PyModule_Create(&sleepermodule);
}
