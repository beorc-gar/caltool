/* calmodule.c
 * Date: March 17, 2016
 * Description: python - c extension for xcal */

#include <Python.h>
#include "calutil.h"
#include <stdio.h>
#include <string.h>

static char* parseError(char* file, CalStatus status) {
    char* err = "UNKNOWN";
    char* out = NULL;
    
    switch(status.code) {
        case OK    : err =     "OK"; break;
        case AFTEND: err = "AFTEND"; break;
        case BADVER: err = "BADVER"; break;
        case BEGEND: err = "BEGEND"; break;
        case IOERR : err =  "IOERR"; break;
        case NOCAL : err =  "NOCAL"; break;
        case NOCRNL: err = "NOCRNL"; break;
        case NODATA: err = "NODATA"; break;
        case NOPROD: err = "NOPROD"; break;
        case SUBCOM: err = "SUBCOM"; break;
        case SYNTAX: err = "SYNTAX"; break;
    }

    out = malloc(strlen(err) + strlen(file) + 30);
    sprintf(out, "%s: %s in lines %d-%d\n", 
        file, err, status.linefrom, status.lineto);

    return out;
}

static char* formatTime(char* str) {
	//'YYYY-MM-DD HH:MM:SS'
    char* format = malloc(20);

    assert(format != NULL);
    if(str == NULL || strlen(str) < 15) {
        free(format);
        return "0000-00-00 00:00:00";
    }

    for(int i = 0; i < 19; i++) {
        format[i] = ' ';
    }

    for(int i = 0; i < 4; i++) {
        format[i] = str[i];
    }

    for(int i = 0; i < 2; i++) {
        format[i +  5] = str[i +  4];
        format[i +  8] = str[i +  6];
        format[i + 11] = str[i +  9];
        format[i + 14] = str[i + 11];
        format[i + 17] = str[i + 13];
    }
    format[4]  = '-';
    format[7]  = '-';
    format[13] = ':';
    format[16] = ':';
    format[19] = '\0';

    return format;
}

static PyObject *Cal_readFile( PyObject *self, PyObject *args ) {
	CalStatus status = initCalStatus(OK, 0,0);
	char *filename   = NULL;
	PyObject *result = PyTuple_New(2);
	PyObject *tuple  = NULL;
	FILE* ics        = NULL;
	CalComp* pcal    = NULL;
	
	PyArg_ParseTuple(args, "s", &filename);
	ics = fopen(filename, "r");

	if(ics == NULL) {
		PyTuple_SetItem(result, 0, Py_BuildValue("s", "error"));
		PyTuple_SetItem(result, 1, Py_BuildValue("s", strerror(errno)));
		return result;
	} else {
		status = readCalFile(ics, &pcal);
	}
	fclose(ics);

	if(status.code != OK) {
		char* out = parseError(filename, status);
		PyTuple_SetItem(result, 0, Py_BuildValue("s", "error"));
		PyTuple_SetItem(result, 1, Py_BuildValue("s", out));
		free(out);
		return result;
	}
	tuple = PyTuple_New(pcal->ncomps);

	for(int i = 0; i < pcal->ncomps; i++) {
		char* summary = "    ";
		char* orgName = "    ";
		char* orgInfo = "    ";
		char* locaton = "    ";
		char* priorty = "    ";
		char* dtstart = "0000-00-00 00:00:00";
		CalProp* prop = pcal->comp[i]->prop;
		PyObject *tmp = NULL;
        
        for(int j = 0; j < pcal->comp[i]->nprops; j++) {
            if(strcmp(prop->name, "ORGANIZER") == 0) {
            	CalParam* param = prop->param;
            	for(int k = 0; k < prop->nparams; k++) {
            		if(strcmp(param->name, "CN") == 0) {
            			orgName = param->value[0];
            			break;
            		}
            		param = param->next;
            	}
            	orgInfo = prop->value;
            } else if(strcmp(prop->name, "DTSTART") == 0) {
            	dtstart = formatTime(prop->value);
            } else if(strcmp(prop->name, "LOCATION") == 0) {
            	locaton = prop->value;
            } else if(strcmp(prop->name, "PRIORITY") == 0) {
            	priorty = prop->value;
            } else if(strcmp(prop->name, "SUMMARY") == 0) {
                summary = prop->value;
            }
            prop = prop->next;
        }

		tmp = Py_BuildValue("(s,i,i,s,s,s,s,s,s)",pcal->comp[i]->name,
			pcal->comp[i]->nprops, pcal->comp[i]->ncomps, summary,
			orgName, orgInfo, dtstart, locaton, priorty);
		PyTuple_SetItem(tuple, i, tmp);

		if(strcmp(dtstart, "0000-00-00 00:00:00") != 0) {
			free(dtstart);
		}
	}
	PyTuple_SetItem(result, 0, Py_BuildValue("k", (unsigned long*)pcal));
	PyTuple_SetItem(result, 1, tuple);

	return result;
}

static PyObject *Cal_writeFile(PyObject *self, PyObject *args ) {
	CalStatus status = initCalStatus(OK, 0, 0);
	char *filename   = NULL;
	CalComp *pcal    = NULL;
	FILE* ics        = NULL;
	int compNum      = 0;
	
	PyArg_ParseTuple(args, "ski", &filename, (unsigned long*)&pcal, &compNum);

	ics = fopen(filename, "w");

	if(ics == NULL) {
		return Py_BuildValue("s", strerror(errno));
	} else if(compNum > -1) {
		status = writeCalComp(ics, pcal->comp[compNum]);
	} else {
		status = writeCalComp(ics, pcal);
	}
	fclose(ics);

	if(status.code != OK) {
		char* out = parseError(filename, status);
		PyObject* pOut = Py_BuildValue("s", out);
		free(out);
		return pOut;
	}

	return Py_BuildValue("s", "OK");
}

static PyObject *Cal_freeFile( PyObject *self, PyObject *args ) {
	CalComp *pcal;
	
	PyArg_ParseTuple(args, "k", (unsigned long*)&pcal);

	freeCalComp(pcal);

	return Py_BuildValue("");
}

static PyMethodDef CalMethods[] = {
	{"readFile",  Cal_readFile,  METH_VARARGS},
	{"writeFile", Cal_writeFile, METH_VARARGS},
	{"freeFile",  Cal_freeFile,  METH_VARARGS},
	{NULL, NULL} 
};

static struct PyModuleDef calModuleDef = {PyModuleDef_HEAD_INIT, "Cal", NULL, -1, CalMethods};

PyMODINIT_FUNC PyInit_Cal(void) { 
	return PyModule_Create(&calModuleDef); 
}
