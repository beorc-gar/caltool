/* calutil.c
 * Date: Feb 21, 2016
 * Description: Reads in an ics file and stores meaningful info in memory.
 *              Various status codes are provided in order to discern what
 *              went wrong in the file while reading it. */

#include "calutil.h"

#define BUFF_LEN 1024

/* Function name: initCalStatus
 * Description: Initialize a CalStatus' members.
 * Args: Code and lines to initilaize your CalStatus with.
 * Returns: A CalStatus with values you passed. */
CalStatus initCalStatus(CalError code, int linefrom, int lineto) {
    CalStatus status;

    status.code     = code;
    status.linefrom = linefrom;
    status.lineto   = lineto;

    return status;
}

/* Function name: initCalProp
 * Description: Initialize a CalProp's members.
 * Returns: A CalProp with nul or 0 values. */
static CalProp initCalProp(void) {
    CalProp prop;

    prop.name    = NULL;
    prop.value   = NULL;
    prop.nparams = 0;
    prop.param   = NULL;
    prop.next    = NULL;

    return prop;
}

/* Function name: initCalComp
 * Description: Initialize a CalComp's members to nul or 0
 * Args: The adress of the CalComp to initialize. */
static void initCalComp(CalComp* comp) {
    comp->name   = NULL;
    comp->prop   = NULL;
    comp->ncomps = 0;
    comp->nprops = 0;
}

/* Function name: printBuffer
 * Description: Prints a string to a file, and folds any strings that
 *              are longer than FOLD_LEN bytes.
 * Args: The char* to print, and the stream to print it to. 
 * Returns: The number of lines written, or it's negative if an error. */
static int printBuffer(char* buff, FILE* ics) {
    int len   = strlen(buff);
    int lines = 0;
    int count = 0;

    for(int i = 0; i < len; i++) {
        if(fprintf(ics, "%c", buff[i]) < 0) {
            return lines * (-1);
        }
        count++;
        if(i == len - 1) {
            if(fprintf(ics, "\r\n") < 0) {
                return lines * (-1);
            }
            lines++;
            break;
        }
        if(count == FOLD_LEN) {
            if(fprintf(ics, "\r\n ") < 0) {
                return lines * (-1);
            }
            lines++;
            count = 1;
        }
    }

    return lines;
}

/* Function name: printParam
 * Description: Formats a string to match iCalandar syntax for parameters.
 * Args: The parameter to format a string for, and the string to store it in.*/
static void printParam(CalParam* param, char** buff) {
    char* comma = "";
    char* temp  = NULL;

    if(param == NULL) {
        return;
    } else if(*buff == NULL) {
        *buff = malloc(1);
        (*buff)[0] = '\0';
        assert(*buff != NULL);
    }
    
    *buff = realloc(*buff, strlen(*buff) + strlen(param->name) + 3);
    assert(*buff != NULL);
    temp = malloc(strlen(*buff) + strlen(param->name) + 3);
    assert(temp != NULL);
    
    sprintf(temp, "%s;%s=", *buff, param->name);
    sprintf(*buff, "%s", temp);
    
    for(int i = 0; i < param->nvalues; i++) {
        *buff = realloc(*buff, strlen(*buff) + strlen(param->value[i]) + 2);
        temp  = realloc(temp , strlen(temp ) + strlen(param->value[i]) + 2);
        assert(*buff != NULL && temp != NULL);
        sprintf(temp, "%s", *buff);
        sprintf(*buff, "%s%s%s", temp, comma, param->value[i]);;
        comma = ",";
    }
    free(temp);
    printParam(param->next, buff);
}

/* Function name: printProp
 * Description: Recursively prints a property to a stream in iCalandar syntax.
 * Args: The property to print, and the stream to print it to. 
 * Returns: The number of lines written, or it's negative if an error. */
static int printProp(CalProp* prop, FILE* ics) {
    char* buff = NULL;
    char* temp = NULL;
    int lines  = 0;
    int ioerr  = 0;
    int len    = 0;

    if(prop == NULL) {
        return 0;
    } else if(prop->name == NULL) {
        len = 0;
    } else {
        len = strlen(prop->name);
    }

    buff = malloc(len + 1);
    assert(buff != NULL);
    
    sprintf(buff, "%s", prop->name);
    printParam(prop->param, &buff);
    temp = malloc(strlen(buff) + strlen(prop->value) + 4);
    assert(temp != NULL);
    sprintf(temp, "%s:%s", buff, prop->value);
    free(buff);
    lines = printBuffer(temp, ics);
    free(temp);
    
    if(lines < 0) {
        return lines;
    }

    ioerr = printProp(prop->next, ics);
    if(ioerr < 0) {
        return ((-1) * lines) + ioerr;
    }

    return lines + ioerr;
}

/* Function name: printComp
 * Description: Recursively prints a component to a stream in iCalandar syntax.
 * Args: The component to print, and the stream to print it to. 
 * Returns: The number of lines written, or it's negative if an error. */
static int printComp(const CalComp* comp, FILE* ics) {
    int lines  = 0;
    int ioerr  = 0;
    char* buff = NULL;

    if(comp == NULL) {
        return 0;
    }

    buff = malloc(strlen(comp->name) + 9);
    assert(buff != NULL);

    sprintf(buff, "BEGIN:%s", comp->name);
    lines = printBuffer(buff, ics);
    free(buff);
    if(lines < 0) {
        return lines;
    }
    buff = NULL;
    ioerr = printProp(comp->prop, ics);
    if(ioerr < 0) {
        return ((-1) * lines) + ioerr;
    }
    lines += ioerr;

    for(int i = 0; i < comp->ncomps; i++) {
        ioerr = printComp(comp->comp[i], ics);
        if(ioerr < 0) {
            return ((-1) * lines) + ioerr;
        }
        lines += ioerr;
    }
    buff = malloc(strlen(comp->name) + 7);
    assert(buff != NULL);
    sprintf(buff, "END:%s", comp->name);
    ioerr = printBuffer(buff, ics);
    free(buff);
    
    if(ioerr < 0) {
        return ((-1) * lines) + ioerr;
    }
    return lines + ioerr;
}

/* Function name: validateProp
 * Description: Validates that there is only 1 version, and that it matches
 *              VCAL_VER macro. Also validates that there is only 1 prodid.
 * Note: To reset static variables safely, call with (NULL, true).
 * Args: The CalProp to validate
 * Returns: The error code if any, OK otherwise. */
static CalError validateProp(CalComp* comp) {
    int nprodid  = 0;
    int nversion = 0;

    for(CalProp* prop = comp->prop; prop != NULL; prop = prop->next) {
        if(strcmp(prop->name, "VERSION") == 0) {
            nversion++;
            if(strcmp(prop->value, VCAL_VER) != 0) {
                return BADVER;
            }
        } else if (strcmp(prop->name, "PRODID") == 0) {
            nprodid++;
        }
    }

    if(nversion != 1) {
        return BADVER;
    } else if(nprodid != 1) {
        return NOPROD;
    }
    return OK;
}

/* Function name: validateComp
 * Description: Validates that at least one component starts with V,
 *              and that there is atleast one component.
 * Args: The CalComp to validate.
 * Returns: The error code if any, OK otherwise. */
static CalError validateComp(CalComp* comp) {
    int vCount = 0; /* must have at least one comp stating with "V" */

    for(int i = 0; i < comp->ncomps; i++) {
        if((comp->comp[i])->name[0] == 'V') {
            vCount++;
        }
    }

    if(comp->ncomps == 0 || vCount == 0) {
        return NOCAL;
    }
    return OK;
}

/* Function name: displace
 * Description: Moves each character in a string forward by the amount of
 *              characters specified.
 * Args: String to displace, and the number of characters to offset it by. */
static void displace(char* str, int offset) {
    int len = strlen(str);

    for(int i = 0; i < len - offset; i++) {
        str[i] = str[i + offset];
    }
    str[len - offset] = '\0';
}

/* Function name: removeCRNL
 * Description: Checks for \r\n sequence and removes it.
 * Args: The string to search for CRNL.
 * Returns: True if CRNL was found and removed, false otherwise. */
static bool removeCRNL(char* str) {
    int len = strlen(str);

    for(int i = 0; i < len; i++) {
        switch(str[i]) {
            case '\r':
                str[i] = '\0';
                return str[i + 1] == '\n';
            case '\n':
                str[i] = '\0';
                return str[i - 1] == '\r';
        }
    }

    return false;
}

/* Function name: isEmpty
 * Description: Checks if a string contains only whitespace.
 * Args: The string to check.
 * Returns: True if string is all whitespace, false otherwise. */
static bool isEmpty(char* str) {
    int len = strlen(str);

    for(int i = 0; i < len; i++) {
        if(isspace(str[i]) == false) {
            return false;
        }
    }

    return true;
}

/* Function name: removeLead
 * Description: Checks for and removes a leading whitespace character.
 * Args: The string to check for and remove leading whitespace from.
 * Returns: True if string began with whitespace, false otherwise. */
static bool removeLead(char* str) {
    int len = strlen(str);
    int i   = 0;

    if(str[0] == ' ' || str[0] == '\t') {
        for(i = 0; i < len - 1; i++) {
            if(str[i] == '\0') {
                break;
            }
            str[i] = str[i + 1];
        }
        str[i] = '\0';

        return true;
    }

    return false;
}

/* Function name: capitalize
 * Description: Converts a string to uppercase.
 * Args: The string to make uppercase. */
static void capitalize(char* str) {
    int len = strlen(str);

    for(int i = 0; i < len; i++) {
        str[i] = toupper(str[i]);
    }
}

/* Function name: checkStart
 * Description: Determines if a prop matches "BEGIN:VCALENDAR".
 * Args: The prop to check.
 * Returns: True if prop matches "BEGIN:VCALENDAR", false otherwise. */
static bool checkStart(CalProp prop) {
    bool check = false;

    if(prop.name == NULL || prop.value == NULL) {
        return false;
    }

    capitalize(prop.value);

    check = strcmp(prop.name, "BEGIN") == 0;
    check = check && strcmp(prop.value, "VCALENDAR") == 0;
    check = check && prop.nparams == 0;

    return check;
}

/* Function name: parseName
 * Description: Identifies the name in this string and stores it in prop->name.
 *              Also removes the name from the string to simplify future parse.
 * Args: The string to parse a name out of, and the prop to store it in.
 * Returns: True if a name was found, false otherwise. */
static bool parseName(char* str, CalProp* prop) {
    int offset   = 0;
    int len      = strlen(str);
    bool success = false;
    int i        = 0;

    if(str == NULL) {
        return false;
    }

    for(i = 0; i < len + 1; i++) {
        if(str[i] == ';' || str[i] == ':') { /* property name ends here */
            if(i == 0) {
                return false;
            }
            offset = i;
            success = true;
            break;
        } else if(str[i] == '\0' || str[i] == '\"' || isspace(str[i])) {
            free(prop->name); /* property doesn't have a value, or it has */
            prop->name = NULL; /* quotes or whitespace in it */
            return false;
        }

        prop->name = realloc(prop->name, i + 2);
        assert(prop->name != NULL);
        prop->name[i] = str[i];
    }
    prop->name[i] = '\0';

    displace(str, offset);
    capitalize(prop->name);

    if(strlen(prop->name) == 0) {
        free(prop->name);
        prop->name = NULL;
        success = false;
    }

    return success;
}

/* Function name: parseParam
 * Description: Identifies the parameters in this string, and stores them in
 *              prop->param linked list, and increments prop->nparams for each.
 *              Also removes the params from string to simplify future parse.
 * Args: The string to parse patameters out of, and the prop to store them in.
 * Note: If a parameter contains a syntax error, str will be damaged in order
 *       for parseValue to throw the SYNTAX error.
 * Returns: True if any parameters were found, false otherwise. */
static bool parseParam(char* str, CalProp* prop) {
    int len         = strlen(str);
    int i           = 0;
    int j           = 0;
    int vals        = 0;
    int offset      = 0;
    CalParam* param = prop->param;
    bool quoted     = false;
    char name [len + 1];
    char value[len + 1];
    char temp [len + 1];

    if(str == NULL || str[0] != ';' || len == 0) {
        return false;
    }

    for(i = 1; i < len; i++) { /* parse parameter name out */
        if(str[i] == ':' || str[i] == ';') {
            offset = i; /* parameter name ends here if no value */
            break;
        } else if(str[i] == '\"' || str[i] == ' ' || str[i] == '\t') {
            str[i] = '\"';
            str[0] = '\0';
            return false;
        } else if(quoted == false && str[i] == '=') {
            vals = 1;
            offset = i + 1;
            break; /* parameter name ends here */
        }

        name[i - 1] = str[i];
    }
    name[i - 1] = '\0';

    if(vals != 1) {   /* param wasn't proper */
        str[i] = '\"';
        str[0] = '\0';
        return false; /* return while buffer is wrecked, so future parsing */
    }                 /* returns a SYNTAX error */

    capitalize(name);
    displace(str, offset);

    if(strlen(name) == 0) {
        return false;
    }

    len = strlen(str);
    offset = 0;

    for(i = 0; i < len; i++) { /* parse parameter value out */
        if(quoted == false && (str[i] == ':' || str[i] == ';')) {
            offset = i; /* parameter value ends here */
            break;
        } else if(quoted == false && isspace(str[i]) == true) {
            str[i] = '\"';
            str[0] = '\0';
            return false;
        }else if(str[i] == '\"') {
            quoted = !quoted;
        } else if(quoted == false && str[i] == ',') {
            vals++; /* this parameter has multiple values */
        }

        value[i] = str[i];
    }
    value[i] = '\0';

    displace(str, offset);

    for(param = prop->param; param != NULL; param = param->next) {
        if(param->next == NULL) { /* find the end of linked list */
            break; /* parameters will be copied into the end of list */
        }
    }

    if(prop->nparams == 0) { /* this is first parameter */
        prop->param = malloc(sizeof(CalParam) + (vals * sizeof(char*)));
        assert(prop->param != NULL);
        param = prop->param;
    } else { /* copy into the end of the linked list */
        param->next = malloc(sizeof(CalParam) + (vals * sizeof(char*)));
        assert(param->next != NULL);
        param = param->next;
    }
    prop->nparams++;
    param->nvalues = vals;
    param->next = NULL;

    param->name = malloc(strlen(name) + 1);
    assert(param->name != NULL);
    strncpy(param->name, name, strlen(name));
    param->name[strlen(name)] = '\0';

    for(i = 0; i < vals; i++) { /* seperate the values */
        offset = 0;
        len = strlen(value);

        for(j = 0; j < len; j++) {
            if(value[j] == '\"') {
                quoted = !quoted;
            } else if(quoted == false && value[j] == ',') {
                offset = j + 1; /* a value ends here */
                break;
            } else if(j == len - 1) {
                offset = j; /* the last value ends here */
            }
            temp[j] = value[j];
        }
        temp[j] = '\0';

        displace(value, offset);

        param->value[i] = malloc(strlen(temp) + 1);
        assert(param->value[i] != NULL);
        strncpy(param->value[i], temp, strlen(temp));
        (param->value[i])[strlen(temp)] = '\0';
    }

    return true;
}

/* Function name: parseValue
 * Description: Identifies the value in this string and stores it in prop.
 * Args: The string to parse the value out of, and the prop to store it in.
 * Returns: True if the value was found, false otherwise. */
static bool parseValue(char* str, CalProp* prop) {
    int len = strlen(str);

    if(str == NULL || str[0] != ':' || strlen(str) == 0) {
        return false;
    }

    prop->value = malloc(len);
    assert(prop->value != NULL);

    for(int i = 0; i < len; i++) { /* copy the string without the colon ':' */
        prop->value[i] = str[i + 1]; /* includes null terminator */
    }

    return true;
}

/* Function name: freeCalParam
 * Description: Recursively frees allocated CalParams in this linked list.
 * Args: The head of the linked list of CalParams to free. */
static void freeCalParam(CalParam* param) {
    if(param == NULL) {
        return;
    }

    free(param->name);
    param->name = NULL;

    freeCalParam(param->next);
    free(param->next);
    param->next = NULL;

    for(int i = 0; i < param->nvalues; i++) {
        free(param->value[i]);
        param->value[i] = NULL;
    }
    param->nvalues = 0;
}

/* Function name: freeCalProp
 * Description: Recursively frees allocated CalProp in this linked list.
 * Args: The head of the linked list of CalProps to free. */
static void freeCalProp(CalProp* prop) {
    if(prop == NULL) {
        return;
    }

    free(prop->name);
    prop->name = NULL;

    free(prop->value);
    prop->value = NULL;

    freeCalParam(prop->param);
    free(prop->param);
    prop->param = NULL;

    freeCalProp(prop->next);
    free(prop->next);
    prop->next = NULL;

    prop->nparams = 0;
}

CalStatus readCalFile( FILE *const ics, CalComp **const pcomp ) {
    CalStatus status = initCalStatus(OK, 0, 0);
    char* line       = NULL;

    assert(ics != NULL && pcomp != NULL);

    readCalLine(NULL, NULL);
    *pcomp = malloc(sizeof(CalComp));

    assert(*pcomp != NULL);
    initCalComp(*pcomp);

    status = readCalComp(ics, pcomp);

    if(status.code != OK) {
        freeCalComp(*pcomp);
        *pcomp = NULL;
        return status;
    }

    /* file has been read successfully. performing validations below */

    status.code = validateProp(*pcomp);
    if(status.code != OK) {
        freeCalComp(*pcomp);
        *pcomp = NULL;
        return status;
    }

    status.code = validateComp(*pcomp);
    if(status.code != OK) {
        freeCalComp(*pcomp);
        *pcomp = NULL;
        return status;
    }

    readCalLine(ics, &line);
    if(line != NULL) {
        status.code = AFTEND;
        status.linefrom++;
        status.lineto++;
        freeCalComp(*pcomp);
        *pcomp = NULL;
    }

    return status;
}

CalStatus readCalComp( FILE *const ics, CalComp **const pcomp ) {
    CalComp comp;
    CalStatus status      = initCalStatus(OK, 0, 0);
    CalProp prop          = initCalProp();
    CalProp *temp         = NULL;
    char* line            = NULL;
    static int depth      = 0; /* keeps track of nested begins */
    static bool firstCall = true;
    const size_t size     = sizeof(CalComp); /* these constants are purely for */
    const size_t sizePtr  = sizeof(CalComp*); /* managing readability */
    int num               = 0;
    int lineStart         = 0;
    int linesRead         = 0;

    if(ics == NULL && pcomp == NULL) { /* reset static var */
        depth = 0;
        firstCall = true;
        return status;
    }

    initCalComp(&comp);

    do {
        free(line);
        line = NULL;
        status = readCalLine(ics, &line);

        if(line == NULL || status.code != OK) {
            if(firstCall == true) {
                lineStart = status.linefrom + 1;
                linesRead = status.lineto + 1;
                status = initCalStatus(NOCAL, 0, 0);
            }
            break;
        }
        if(line == NULL && depth != 0) {
            status.code = BEGEND;
            break;
        }

        status.code = parseCalProp(line, &prop);
        if(status.code != OK) {
            freeCalProp(&prop);
            break;
        }

        if((*pcomp)->name == NULL && checkStart(prop) == false) {
            status.code = NOCAL; /* first component isnt BEGIN:VCALENDAR */
            freeCalProp(&prop);
            break;
        } else if((*pcomp)->name == NULL) { /* create the first compnent */
            (*pcomp)->name = malloc(strlen(prop.value) + 1);
            assert((*pcomp)->name != NULL);
            strncpy((*pcomp)->name, prop.value, strlen(prop.value));
            (*pcomp)->name[strlen(prop.value)] = '\0';

            depth = 1;
            freeCalProp(&prop);
            prop = initCalProp();
            continue;
        }

        if(strcmp(prop.name, "BEGIN") == 0) {
            capitalize(prop.value);
            if(depth >= 3) {
                status.code = SUBCOM;
                freeCalProp(&prop);
                break;
            } else { /* create a new compnent with values from prop */
                comp.name = malloc(strlen(prop.value) + 1);
                assert(comp.name != NULL);

                strncpy(comp.name, prop.value, strlen(prop.value));
                comp.name[strlen(prop.value)] = '\0';
                depth++;

                freeCalProp(&prop);
                prop = initCalProp();
                num = (*pcomp)->ncomps;
                (*pcomp)->ncomps++;

                *pcomp = realloc(*pcomp, size + ((*pcomp)->ncomps) * sizePtr);
                (*pcomp)->comp[num] = malloc(size);
                assert((*pcomp != NULL) && ((*pcomp)->comp[num] != NULL));

                *((*pcomp)->comp[num]) = comp;
                /* put newly created comp into the flexible array member */

                status = readCalComp(ics, &((*pcomp)->comp[num]));
                /* read the next compnents into the new one */
                if(status.code != OK) {
                    break;
                }
            }
        } else if(strcmp(prop.name, "END") == 0) {
            capitalize(prop.value);
            if((*pcomp)->ncomps == 0 && (*pcomp)->nprops == 0) {
                status.code = NODATA;
                freeCalProp(&prop);
                break;
            }

            if(strcmp(prop.value, (*pcomp)->name) == 0) {
                if(strcmp(prop.value, "VCALENDAR") != 0) {
                    depth--;
                } /* else we are done */
                freeCalProp(&prop);
                break;
            } else {
                status.code = BEGEND;
                freeCalProp(&prop);
                break;
            }
        } else { /* ordinary property */
            for(temp = (*pcomp)->prop; temp != NULL; temp = temp->next) {
                if(temp->next == NULL) {
                    break; /* find place in linked list to copy to */
                }
            }

            if(temp != NULL) { /* append to linked list */
                temp->next = malloc(sizeof(CalProp));
                assert(temp->next != NULL);
                *temp->next = prop;
                temp->next->next = NULL;
            } else { /* start a linked list */
                (*pcomp)->prop = malloc(sizeof(CalProp));
                assert((*pcomp)->prop != NULL);
                *(*pcomp)->prop = prop;
                (*pcomp)->prop->next = NULL;
            }

            (*pcomp)->nprops++;
            prop = initCalProp();
        }
    } while(line != NULL);

    if(status.code == NOCAL && depth != 0) {
        status = initCalStatus(BEGEND, lineStart, linesRead);
    }

    firstCall = false;
    free(line); 
    return status;
}

CalStatus readCalLine( FILE *const ics, char **const pbuff ) {
    static char* readAhead = NULL; /* buffer containing content on next line */
    static int linesRead   = 0; /* total number of lines read thus far */
    static bool lastLine   = false; /* used to check if EOF was hit or not */
    CalError status        = OK;
    char buffer[BUFF_LEN]  = {'\0'};
    char temp  [BUFF_LEN]  = {'\0'};
    bool folded            = true;
    int startLine          = linesRead + 1;
    int len                = 0;
    int i                  = 0;

    if(ics == NULL || pbuff == NULL) { /* NULL args | prepare for a new file */
        readCalComp(NULL, NULL);

        free(readAhead);
        readAhead = NULL;

        linesRead = 0;
        lastLine = false;

        return initCalStatus(OK, 0, 0);
    }

    assert(ics != NULL && pbuff != NULL);

    if(lastLine == true) { /* EOF was hit last call */
        *pbuff = NULL;
        return initCalStatus(status, linesRead-1, linesRead-1);
    }

    if(readAhead == NULL && lastLine == false) { /* first call */
        if(fgets(buffer, BUFF_LEN - 1, ics) == NULL){ /* empty file */
            *pbuff = NULL;
            lastLine = true;
            return initCalStatus(OK, 0, 0);
        } else {
            linesRead++;
        }

        if(removeCRNL(buffer) == false) {
            if(fgets(temp, BUFF_LEN - 1, ics) != NULL) {
                free(*pbuff);
                *pbuff = NULL;
                status = NOCRNL;
                folded = false;
            }
        } else {
            *pbuff = malloc(strlen(buffer) + 1);
            assert(*pbuff != NULL);
            strncpy(*pbuff, buffer, strlen(buffer));
            (*pbuff)[strlen(buffer)] = '\0';
        }
    } else { /* a string was stored last call for me */
        *pbuff = malloc(strlen(readAhead) + 1);
        assert(*pbuff != NULL);
        strncpy(*pbuff, readAhead, strlen(readAhead));
        (*pbuff)[strlen(readAhead)] = '\0';
        free(readAhead);
        readAhead = NULL;
        linesRead++;
    }

    while(folded == true) {
        for(i = 0; i < BUFF_LEN; i++) {
            buffer[i] = '\0';
        }

        if(fgets(buffer, BUFF_LEN - 1, ics) == NULL) {
            lastLine = true; /* let next call know not to read any further */
            folded = false; /* proccess remaining string, quit afterward */
            free(readAhead);
            readAhead = NULL;
        } else if(removeCRNL(buffer) == false) {
            if(fgets(temp, BUFF_LEN - 1, ics) != NULL) {
                free(*pbuff);
                *pbuff = NULL;
                status = NOCRNL;
                break;
            }
        }

        if(isEmpty(buffer) == true) {
            linesRead++;
        }

        folded = removeLead(buffer) && status == OK;

        if(strlen(buffer) == 0) {
            if(*pbuff == NULL) {
                *pbuff = malloc(1);
                assert(*pbuff != NULL);
                (*pbuff)[0] = '\0';
            }
            continue;
        } else if(folded == true) { /* append buffer to the end of pbuff */
            if(*pbuff == NULL) {
                len = strlen(buffer) + 1;
                *pbuff = malloc(len);
            } else {
                len = strlen(*pbuff) + strlen(buffer) + 1;
                *pbuff = realloc(*pbuff, len);
            }
            assert(*pbuff != NULL);
            strncat(*pbuff, buffer, strlen(buffer));
            (*pbuff)[len - 1] = '\0';
            linesRead++;
        } else { /* store buffer away for use on next call */
            readAhead = malloc(strlen(buffer) + 1);
            assert(readAhead != NULL);
            strncpy(readAhead, buffer, strlen(buffer));
            readAhead[strlen(buffer)] = '\0';
        }
    }

    if(lastLine == true) {
        linesRead--;
    }

    return initCalStatus(status, startLine, linesRead);
}

CalError parseCalProp( char *const buff, CalProp *const prop ) {
    int len = strlen(buff);
    char buffer[len + 1];

    *prop = initCalProp();

    assert(buff != NULL && prop != NULL);

    strncpy(buffer, buff, len);
    buffer[len] = '\0';

    if(parseName(buffer, prop) == false) {
        freeCalProp(prop);
        *prop = initCalProp();
        return SYNTAX;
    }

    while(parseParam(buffer, prop) == true) {
        /* optional parameters */
    }

    if(parseValue(buffer, prop) == false) {
        freeCalProp(prop);
        *prop = initCalProp();
        return SYNTAX;
    }

    return OK;
}

void freeCalComp( CalComp *const comp ) {
    if(comp == NULL) {
        return;
    }

    free(comp->name);
    comp->name = NULL;

    freeCalProp(comp->prop);
    free(comp->prop);
    comp->prop = NULL;

    for(int i = 0; i < comp->ncomps; i++) {
        freeCalComp(comp->comp[i]);
        comp->comp[i] = NULL;
    }
    free(comp);
}

CalStatus writeCalComp( FILE *const ics, const CalComp *comp ) {
    int lines = 0;

    if(ics == NULL || comp == NULL) {
        return initCalStatus(IOERR, 0, 0);
    }
    lines = printComp(comp, ics);

    if(lines < 1) {
        return initCalStatus(IOERR, (-1) * lines, (-1) * lines);
    }
    return initCalStatus(OK, lines, lines);
}
