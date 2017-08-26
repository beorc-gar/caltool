/* caltool.c
 * Author: Bronson Graansma 0872249
 * Contact: bgraansm@mail.uoguelph.ca
 * Date: Feb 21, 2016
 * Description: Command line interface to interact with iCalandar
 *              ics files. */

#include "caltool.h"

typedef struct CalEvent {
    char* dtstart;
    char* summary;
    struct tm date;
} CalEvent;

/* Function name: initCalComp
 * Description: Initialize a CalComp's members to nul or 0
 * Args: The adress of the CalComp to initialize. */
static void initCalComp(CalComp* comp) {
    comp->name   = NULL;
    comp->prop   = NULL;
    comp->nprops = 0;
    comp->ncomps = 0;
}

/* Function name: parseError
 * Description: Prints an error message to stderr saying the error in more 
 *              readable terms, based on the status.
 * Args: The name of the file that was parsed, and the status it received. */
static void parseError(char* file, CalStatus status) {
    char* err = "UNKNOWN";
    
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

    fprintf(stderr, "%s: %s in lines %d-%d\n", 
        file, err, status.linefrom, status.lineto);
}

/* Function name: freeCalEvent
 * Description: Frees any allocated memory within a CalEvent.
 * Args: The CalEvent to free the allocated memory in. */
static void freeCalEvent(CalEvent event) {
    free(event.dtstart);
}

/* Function name: compareDates
 * Description: Compare function used to qsort CalEvents by date.
 * Args: Two void (CalEvent) pointers to compare.
 * Returns: 1 if first > second, -1 if first < second, or 0 if equal. */
static int compareDates(const void* date1, const void* date2) {
    time_t time1 = mktime(&((CalEvent*)date1)->date); 
    time_t time2 = mktime(&((CalEvent*)date2)->date);

    return (time1 > time2) - (time1 < time2);
}

/* Function name: compareStr
 * Description: Compare function used to qsort strings alphabetically.
 * Args: Two void (char*) pointers to compare.
 * Returns: 1 if first > second, -1 if first < second, or 0 if equal. */
static int compareStr(const void* str1, const void* str2) {
    return strcmp(*(char**)str1, *(char**)str2);
}

/* Function name: countComps
 * Description: Counts top level V components.
 * Args: The CalComp to sum up the V components.
 * Returns: The number of top level V compenents. */
static int countComps(const CalComp* comp) {
    int comps = 0;

    for(int i = 0; i < comp->ncomps; i++) {
        if(comp->comp[i]->name[0] == 'V') {
            comps++;
        }
    }
    return comps;
}

/* Function name: countEvents
 * Description: Counts how many VEVENTs are in a component.
 * Args: The component to sum up the VEVENTs.
 * Returns: The number of VEVENTs. */
static int countEvents(const CalComp* comp) {
    int events = 0;

    for(int i = 0; i < comp->ncomps; i++) {
        if(strcmp(comp->comp[i]->name, "VEVENT") == 0) {
            events++;
        }
    }
    return events;
}

/* Function name: countTodos
 * Description: Counts how many VTODOs are in a component.
 * Args: The component to sum up the VTODOs.
 * Returns: The number of VTODOs. */
static int countTodos(const CalComp* comp) {
    int todos = 0;

    for(int i = 0; i < comp->ncomps; i++) {
        if(strcmp(comp->comp[i]->name, "VTODO") == 0) {
            todos++;
        }
    }
    return todos;
}

/* Function name: countSubcomps
 * Description: Counts how many subcomponents are in a component.
 * Args: The component to sum up the subcomponents.
 * Returns: The number of subcomponents. */
static int countSubcomps(const CalComp* comp) {
    int subcomps = 0;
    
    if(comp == NULL) {
        return 0;
    }

    for(int i = 0; i < comp->ncomps; i++) {
        subcomps += comp->comp[i]->ncomps + countSubcomps(comp->comp[i]);
    }

    return subcomps;
}

/* Function name: countOrganizers
 * Description: Counts how many organizers are in a component.
 *              Stores the organizers CN value into orgList.
 * Args: The component to sum up the organizers, the adress of the array
 *       of strings to store the organizers in, and the number of previously
 *       found organizers (call with zero, this is used for recursion).
 * Returns: The number of organizers. */
static int countOrganizers(const CalComp* comp, char*** orgList, int organs) {
    for(int i = 0; i < comp->ncomps; i++) {
        CalProp* prop = comp->comp[i]->prop;
        
        for(int j = 0; j < comp->comp[i]->nprops; j++) {
            CalParam* param = prop->param;
            
            if(strcmp(prop->name, "ORGANIZER") == 0) {
                for(int k = 0; k < prop->nparams; k++) {
                    if(strcmp(param->name, "CN") == 0) {
                        *orgList = realloc(*orgList, (organs + 
                            param->nvalues + 1) * sizeof(char*));
                        assert(*orgList != NULL);
                        for(int m = 0; m < param->nvalues; m++) {
                            (*orgList)[organs + m] = param->value[m];
                        } // copy all the cn of organizers into a list
                        organs += param->nvalues;
                    }
                    param = param->next;
                }
            }
            prop = prop->next;
        }
        organs = countOrganizers(comp->comp[i], orgList, organs);
    }
    return organs;
}

/* Function name: countProps
 * Description: Counts how many properties are in a component.
 * Args: The component to sum up the properties.
 * Returns: The number of properties. */
static int countProps(const CalComp* comp) {
    int props = 0;

    if(comp == NULL) {
        return 0;
    }

    props = comp->nprops;
    for(int i = 0; i < comp->ncomps; i++) {
        props += countProps(comp->comp[i]);
    }
    return props;
}

/* Function name: isTimeProp
 * Description: Checks if a property contains a time value.
 * Args: The property to check if it's a time property.
 * Returns: True if the property has a time, false otherwise. */
static bool isTimeProp(CalProp* prop, bool mod) {
    if(prop == NULL) {
        return false;
    } else if(strcmp(prop->name, "DTSTART"      ) == 0   ||
             (strcmp(prop->name, "DTSTAMP") == 0 && mod) ||
              strcmp(prop->name, "DTEND"        ) == 0   ||
             (strcmp(prop->name, "CREATED") == 0 && mod) ||
              strcmp(prop->name, "COMPLETED"    ) == 0   ||
              strcmp(prop->name, "DUE"          ) == 0   ||
             (strcmp(prop->name, "LAST-MODIFIED") == 0 && mod)) {
        return true;
    }
    return false;
}

/* Function name: formatTime
 * Description: Creates a string that is easier to parse ("%Y %m %d %H %M %S")
 *              in strptime, than the property value.
 * Args: The string to match a date with in an easier way.
 *       Expects str to be in format (yyyymmddThhmmss[Z]).
 * Returns: The easier to parse date string. "%Y %m %d %H %M %S" */
static char* formatTime(char* str) {
    char* format = malloc(30);

    assert(format != NULL);
    if(str == NULL || strlen(str) < 15) {
        free(format);
        return NULL;
    }

    for(int i = 0; i < 19; i++) {
        format[i] = ' ';
    }

    for(int i = 0; i < 4; i++) {
        format[i] = str[i];
    }

    //basically insert spaces between times for easy parsing
    for(int i = 0; i < 2; i++) {
        format[i +  5] = str[i +  4];
        format[i +  8] = str[i +  6];
        format[i + 11] = str[i +  9];
        format[i + 14] = str[i + 11];
        format[i + 17] = str[i + 13];
    }
    format[19] = '\0';

    return format;
}

/* Function name: fetFirstDate
 * Description: Fetches the earliest date from a component tree.
 * Args: The component to retreive the oldest date from.
 * Returns: The string of the date in format (yyyymmddThhmmss[Z]). */
static char* getFirstDate(const CalComp* comp) {
    char* dateStr = NULL;
    char* dateTmp = NULL;
    time_t lowest = 0;
    time_t curent = 0;
    struct tm dateTime;

    if(comp == NULL) {
        return NULL;
    }

    for(int i = 0; i < comp->ncomps; i++) {
        CalProp* prop = comp->comp[i]->prop;

        if(strcmp(comp->comp[i]->name, "VTIMEZONE") == 0) {
            continue;
        }
        
        for(int j = 0; j < comp->comp[i]->nprops; j++) {
            if(isTimeProp(prop, true) == true) {
                dateTmp = formatTime(prop->value);
                if(dateTmp == NULL) {
                    free(dateTmp);
                    continue; 
                }
                strptime(dateTmp, "%Y %m %d %H %M %S",&dateTime);
                dateTime.tm_isdst = -1;
                curent = mktime(&dateTime);

                if(curent < lowest || lowest == 0) {
                    lowest = curent;
                    free(dateStr);
                    dateStr = dateTmp;
                    dateTmp = NULL;
                } else {
                    free(dateTmp);
                    dateTmp = NULL;
                }
            }
            prop = prop->next;
        }
        dateTmp = getFirstDate(comp->comp[i]);
        if(dateTmp == NULL) {
            free(dateTmp);
            continue; 
        }
        strptime(dateTmp, "%Y %m %d %H %M %S", &dateTime);
        dateTime.tm_isdst = -1;
        curent = mktime(&dateTime);

        if(curent < lowest || lowest == 0) {
            lowest = curent;
            free(dateStr);
            dateStr = dateTmp;
        } else {
            free(dateTmp);
            dateTmp = NULL;
        }
    }

    return dateStr;
}

/* Function name: fetLastDate
 * Description: Fetches the latest date from a component tree.
 * Args: The component to retreive the newest date from.
 * Returns: The string of the date in format (yyyymmddThhmmss[Z]). */
static char* getLastDate(const CalComp* comp) {
    char* dateStr = NULL;
    char* dateTmp = NULL;
    time_t highst = 0;
    time_t curent = 0;
    struct tm dateTime;

    if(comp == NULL) {
        return NULL;
    }

    for(int i = 0; i < comp->ncomps; i++) {
        CalProp* prop = comp->comp[i]->prop;

        if(strcmp(comp->comp[i]->name, "VTIMEZONE") == 0) {
            continue;
        }
        
        for(int j = 0; j < comp->comp[i]->nprops; j++) {
            if(isTimeProp(prop, true) == true) {
                dateTmp = formatTime(prop->value);
                if(dateTmp == NULL) {
                    free(dateTmp);
                    continue; 
                }
                strptime(dateTmp, "%Y %m %d %H %M %S", &dateTime);
                dateTime.tm_isdst = -1;
                curent = mktime(&dateTime);

                if(curent > highst || highst == 0) {
                    highst = curent;
                    free(dateStr);
                    dateStr = dateTmp;
                    dateTmp = NULL;
                } else {
                    free(dateTmp);
                    dateTmp = NULL;
                }
            }
            prop = prop->next;
        }
        dateTmp = getLastDate(comp->comp[i]);
        if(dateTmp == NULL) {
            free(dateTmp);
            continue; 
        }
        strptime(dateTmp, "%Y %m %d %H %M %S", &dateTime);
        dateTime.tm_isdst = -1;
        curent = mktime(&dateTime);

        if(curent > highst || highst == 0) {
            highst = curent;
            free(dateStr);
            dateStr = dateTmp;
        } else {
            free(dateTmp);
            dateTmp = NULL;
        }
    }
    return dateStr;
}

/* Function name: isInRange
 * Description: Checks of a component lies within two dates. If dates are
 *              unspecified, any time is good.
 * Args: The component to check, and the two dates to see if the component
 *       is between them.
 * Returns: True if the component falls within the dates, false otherwise. */
static bool isInRange(const CalComp* comp, time_t datefrom, time_t dateto) {
    CalProp* prop = comp->prop;
    char* dateTmp = NULL;
    time_t curent = 0;
    struct tm date;

    if(comp == NULL) {
        return false;
    }
        
    for(int i = 0; i < comp->nprops; i++) {
        if(isTimeProp(prop, false) == true) {
            dateTmp = formatTime(prop->value);
            if(dateTmp == NULL) {
                free(dateTmp);
                continue; 
            }
            strptime(dateTmp, "%Y %m %d %H %M %S", &date);
            date.tm_isdst = -1;
            curent = mktime(&date);
            free(dateTmp);

            if(datefrom == 0 && dateto == 0) {
                return true;
            } else if(datefrom == 0 && curent <= dateto) {
                return true;
            } else if(dateto == 0 && curent >= datefrom) {
                return true;
            } else if(curent >= datefrom && curent <= dateto) {
                return true;
            } else {
                dateTmp = NULL;
            }
        }
        for(int j = 0; j < comp->ncomps; j++) {
            if(isInRange(comp->comp[j], datefrom, dateto) == true) {
                return true;
            }
        }
        prop = prop->next;
    }

    return false;
}

/* Function name: remDupe
 * Description: Nullifies strings in a list that appear more than once.
 * Args: The array of strings to remove duplicates from, and how many 
 *       elements it has.
 * Returns: How many non NULL elements remain in the list. */
static int remDupe(char** list, int elements) {
    int removed = 0;
    
    for(int i = 0; i < elements; i++) {
        for(int j = i + 1; j < elements; j++) {
            if(strcmp(list[i], list[j]) == 0) {
                list[i] = NULL;
                removed++;
                break;
            }
        }
    }

    return elements - removed;
}

/* Function name: extract
 * Description: Creates an array of strings that are all the X properties
 *              in a component, and stores how many elements are in the array
 *              in xprops.
 * Args: The component to list the xproperties of, the adress of the array of
 *       strings to store their names in, and the adress of how many
 *       elements are being stored in the array. */
static void extract(const CalComp* comp, char*** xprop, int* xprops) {
    CalProp* prop = comp->prop;
        
    for(int i = 0; i < comp->nprops; i++) {
        if(prop->name[0] == 'X' && prop->name[1] == '-') {
            *xprop = realloc(*xprop, ((*xprops) + 1) * sizeof(char*));
            assert(*xprop != NULL);
            (*xprop)[*xprops] = prop->name;
            (*xprops)++;
        }
        prop = prop->next;
    }

    for(int i = 0; i < comp->ncomps; i++) {
        extract(comp->comp[i], xprop, xprops);
    }
}

CalStatus calInfo(const CalComp *comp, int lines, FILE *const txtfile) {
    char** orgList = NULL;
    char* first    = getFirstDate(comp);
    char* last     = getLastDate(comp);
    char* plural   = "";
    char* from     = "No dates";
    char* to       = "";
    int components = countComps(comp);
    int events     = countEvents(comp);
    int todos      = countTodos(comp);
    int others     = (components - events) - todos;
    int subcomps   = countSubcomps(comp);
    int organizers = countOrganizers(comp, &orgList, 0);
    int props      = countProps(comp);
    int nonDupes   = 0;
    int linesWrote = 0;
    struct tm date;

    if(first != NULL && last != NULL) {
        strptime(first, "%Y %m %d %H %M %S", &date);
        date.tm_isdst = -1;
        strftime(first, 19, "%Y-%b-%d", &date);
        strptime(last, "%Y %m %d %H %M %S", &date);
        date.tm_isdst = -1;
        strftime(last, 19, "%Y-%b-%d", &date);
    }
    
    if(lines != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d line%s\n", lines, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    plural = "";

    if(components != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d component%s: ", components, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    plural = "";

    if(events != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d event%s, ", events, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    plural = "";

    if(todos != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d todo%s, ", todos, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    plural = "";

    if(others != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d other%s\n", others, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    plural = "";

    if(subcomps != 1) {
        plural = "s";
    }
    if(fprintf(txtfile, "%d subcomponent%s\n", subcomps, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    plural = "";

    if(props != 1) {
        plural = "ies";
    } else {
        plural = "y";
    }
    if(fprintf(txtfile, "%d propert%s\n", props, plural) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    plural = "";

    if(first != NULL) {
        from = "From ";
        to   = " to ";
    } else {
        free(first);
        free(last);
        first = "";
        last  = "";
    }

    if(fprintf(txtfile, "%s%s%s%s\n", from, first, to, last) < 0) {
        free(first);
        free(last);
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    if(strcmp(first, "") == 0) {
        first = NULL;
        last  = NULL;
    }
    free(first);
    free(last);
    plural = "";

    qsort(orgList, organizers, sizeof(char*), compareStr);
    nonDupes = remDupe(orgList, organizers);

    if(nonDupes == 0) {
        plural = "No o";
    } else {
        plural = "O";
    }

    if(fprintf(txtfile, "%srganizers", plural) < 0) {
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    if(nonDupes != 0) {
        if(fprintf(txtfile, ":") < 0) {
            return initCalStatus(IOERR, linesWrote, linesWrote);
        }
    }
    if(fprintf(txtfile, "\n") < 0) {
        return initCalStatus(IOERR, linesWrote, linesWrote);
    }
    linesWrote++;
    plural = "";

    for(int i = 0; i < organizers; i++) {
        if(orgList[i] == NULL) {
            continue;
        }
        if(fprintf(txtfile, "%s\n", orgList[i]) < 0) {
            return initCalStatus(IOERR, linesWrote, linesWrote);
        }
        linesWrote++;
    }
    free(orgList);
    
    return initCalStatus(OK, linesWrote, linesWrote);
}

CalStatus calExtract(const CalComp *comp, CalOpt kind, FILE *const txtfile) {
    CalError err   = OK;
    int linesWrote = 0;

    if(kind == OEVENT) {
        CalEvent* event = NULL;
        int events = 0;

        for(int i = 0; i < comp->ncomps; i++) {
            CalProp* prop = comp->comp[i]->prop;
            
            if(strcmp(comp->comp[i]->name, "VEVENT") == 0) {
                event = realloc(event, (events + 1) *sizeof(CalEvent));
                assert(event != NULL);
                for(int j = 0; j < comp->comp[i]->nprops; j++) {
                    if(strcmp(prop->name, "DTSTART") == 0) {
                        event[events].dtstart = formatTime(prop->value);
                        strptime(event[events].dtstart, "%Y %m %d %H %M %S"
                            , &event[events].date);
                        event[events].date.tm_isdst = -1;
                        strftime(event[events].dtstart, 29, "%Y-%b-%d %l:%M %p"
                            , &event[events].date);
                        event[events].summary = NULL;
                        
                        prop = comp->comp[i]->prop;
                        for(int k = 0; k < comp->comp[i]->nprops; k++) {
                            if(strcmp(prop->name, "SUMMARY") == 0) {
                                event[events].summary = prop->value;
                                break;
                            }
                            prop = prop->next;
                        }
                        if(event[events].summary == NULL) {
                            event[events].summary = "(na)";
                        }
                        break;
                    }
                    prop = prop->next;
                }
                events++;
            }
        }
        qsort(event, events, sizeof(CalEvent), compareDates);
        for(int i = 0; i < events; i++) {
            if(fprintf(txtfile, "%s: %s\n", event[i].dtstart
                    , event[i].summary) < 0) {
                err = IOERR;
                break;
            }
            freeCalEvent(event[i]);
            linesWrote++;
        }
        free(event);
    } else if(kind == OPROP) {
        char** xprop = NULL;
        int xprops   = 0;

        extract(comp, &xprop, &xprops);
        qsort(xprop, xprops, sizeof(char*), compareStr);
        remDupe(xprop, xprops);

        for(int i = 0; i < xprops; i++) {
            if(xprop[i] == NULL) {
                continue;
            }
            if(fprintf(txtfile, "%s\n", xprop[i]) < 0) {
                err = IOERR;
                break;
            }
            linesWrote++;
        }
        free(xprop);
    } else {
        err = IOERR;
    }

    return initCalStatus(err, linesWrote, linesWrote);
}

CalStatus calFilter(const CalComp *comp, CalOpt content, time_t datefrom, 
                    time_t dateto, FILE *const icsfile) {
    CalComp* copy    = malloc(sizeof(CalComp));
    CalStatus status = initCalStatus(OK, 0, 0);
    char* filter     = "";

    assert(copy != NULL);
    initCalComp(copy);
    copy->name   = comp->name;
    copy->nprops = comp->nprops;
    copy->prop   = comp->prop;

    if(content == OEVENT) {
        filter = "VEVENT";
    } else if(content == OTODO) {
        filter = "VTODO";
    }

    for(int i = 0; i < comp->ncomps; i++) {
            if(strcmp(comp->comp[i]->name, filter) == 0) {
                if(isInRange(comp->comp[i], datefrom, dateto) == true) {
                    copy = realloc(copy, sizeof(CalComp) + ((copy->ncomps + 1)
                        * sizeof(CalComp*)));
                    assert(copy != NULL);
                    copy->comp[copy->ncomps] = comp->comp[i];
                    copy->ncomps++;
                }
            }
    }

    if(copy->ncomps == 0) {
        free(copy);
        return initCalStatus(NOCAL, 0, 0);
    }

    status = writeCalComp(icsfile, copy);
    free(copy);
    return status;
}

CalStatus calCombine(const CalComp *comp1, const CalComp *comp2, 
                     FILE *const icsfile) {
    const size_t size = comp1->ncomps + comp2->ncomps; 
    CalComp* copy     = malloc(sizeof(CalComp) + size * sizeof(CalComp*));
    CalStatus status  = initCalStatus(OK, 0, 0);
    CalProp* prop     = comp2->prop;

    assert(copy != NULL);

    copy->name   = comp1->name;
    copy->prop   = comp1->prop;
    copy->nprops = comp1->nprops;
    copy->ncomps = size;

    for(int i = 0; i < comp1->ncomps; i++) {
        copy->comp[i] = comp1->comp[i];

    }
    for(int i = comp1->ncomps; i < size; i++) {
        copy->comp[i] = comp2->comp[i - comp1->ncomps];
    }
    for(int i = 0; i < comp2->nprops; i++) {
        if(strcmp(prop->name, "VERSION") == 0); //don't copy
        else if(strcmp(prop->name, "PRODID") == 0); //don't copy
        else {
            CalProp* tempProp = copy->prop;
            
            for(int j = 0; j < copy->nprops - 1; j++) {
                tempProp = tempProp->next;
            }
            tempProp->next = prop;
            copy->nprops++;
        }
        prop = prop->next;
    }

    status = writeCalComp(icsfile, copy);
    free(copy);
    return status;
}

/* Function name: main
 * Description: Analyzes command line arguments and attempts to
 *              do as the user instructs. Reports feedback for
 *              errors, otherwise silently performs it's task.
 * Args: Command line arguments.
 * Returns: EXIT_SUCCESS if everything went nicely, otherwise EXIT_FAILURE. */
int main(int argc, char** argv) {
    CalStatus status = initCalStatus(OK, 0, 0);
    CalOpt opt       = OPROP;
    FILE* ics        = stdin;
    FILE* ocs        = stdout;
    char* todayStr   = NULL;
    CalComp* comp    = NULL;
    int ioerr        = 0;
    time_t today     = time(NULL);
    struct tm todaym = *localtime(&today);

    if(argc < 2) {
        fprintf(stderr, "general usage: %s <option>\n", argv[0]);
        goto FAIL;
        /* goto only used to unclutter a list of statements that
           would be executed many times for each error feedback */
    }

    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "today") == 0) {
            todayStr = malloc(20);
            assert(todayStr != NULL);
            todaym.tm_isdst = -1;
            strftime(todayStr, 19, "%m/%d/%Y", &todaym);
            break;
        }
    }

    if(strcmp(argv[1], "-info") == 0) {
        if(argc != 2) {
            fprintf(stderr, "info usage: %s -info\n", argv[0]);
            goto FAIL;
        }
        status = readCalFile(ics, &comp);
        if(status.code == OK) {
            status = calInfo(comp, status.lineto, ocs);
        } else {
            parseError(argv[2], status); 
            goto FAIL;
        }
    } else if(strcmp(argv[1], "-extract") == 0) {
        if(argc == 3) {
            if(strcmp(argv[2], "e") == 0) {
                opt = OEVENT;
            } else if(strcmp(argv[2], "x") == 0) {
                opt = OPROP;
            } else {
                fprintf(stderr, "kinds: e, x\n");
                goto FAIL;
            }
        } else {
            fprintf(stderr, "extract usage: %s -extract <kind>\n", argv[0]);
            goto FAIL;
        }

        status = readCalFile(ics, &comp);
        if(status.code == OK) {
            status = calExtract(comp, opt, ocs);
        } else {
            parseError(argv[2], status); 
            goto FAIL;
        }
    } else if(strcmp(argv[1], "-filter") == 0) {
        time_t dateFrom  = 0;
        time_t dateTo    = 0;
        struct tm date;

        if(argc > 2) {
            if(strcmp(argv[2], "e") == 0) {
                opt = OEVENT;
            } else if(strcmp(argv[2], "t") == 0) {
                opt = OTODO;
            } else {
                fprintf(stderr, "content: e, t\n");
                goto FAIL;
            }
        } else {
            fprintf(stderr, "filter usage: %s -filter <content>\n", argv[0]);
            goto FAIL;
        }

        if(!(argc == 3 || argc == 5 || argc == 7)) {
            fprintf(stderr, 
                "filter usage: %s -filter <content> from <date> to <date>\n"
                , argv[0]);
            goto FAIL;
        }

        if(argc > 4) {
            if(strcmp(argv[4], "today") == 0) {
                ioerr = getdate_r(todayStr, &date);
            } else {
                ioerr = getdate_r(argv[4], &date);
            }
            assert(ioerr != 6);
            if(ioerr != 0) {
                if(ioerr > 6) {
                    fprintf(stderr, "%s doesn't match any date\n", argv[4]);
                } else {
                    fprintf(stderr, "DATEMSK environment variable error\n");
                }
                goto FAIL;
            }
                
            if(strcmp(argv[3], "from") == 0) {
                date.tm_hour = 0;
                date.tm_min  = 0;
                date.tm_sec  = 0;
                date.tm_isdst = -1;
                dateFrom = mktime(&date);
            } else if(strcmp(argv[3], "to") == 0) {
                if(argc > 6) {
                   fprintf(stderr, 
                        "filter usage: %s -filter <content> to <date>\n"
                        , argv[0]); 
                   goto FAIL;
                }
                date.tm_hour = 23;
                date.tm_min  = 59;
                date.tm_sec  = 0;
                date.tm_isdst = -1;
                dateTo = mktime(&date);
            } else {
                fprintf(stderr, 
                    "filter usage: %s -filter <content> from <date>\n"
                    , argv[0]);
                goto FAIL;
            }
        }

        if (argc > 6) {
            if(strcmp(argv[3], "to") == 0 || strcmp(argv[5], "to") != 0) {
                fprintf(stderr, 
                    "filter usage: %s -filter <content> from <date> to <date>\n"
                    , argv[0]);
                goto FAIL;
            }

            if(strcmp(argv[6], "today") == 0) {
                ioerr = getdate_r(todayStr, &date);
            } else {
                ioerr = getdate_r(argv[6], &date);
            }
            assert(ioerr != 6);
            if(ioerr != 0) {
                if(ioerr > 6) {
                    fprintf(stderr, "%s doesn't match any date\n", argv[6]);
                } else {
                    fprintf(stderr, "DATEMSK environment variable error\n");
                }
                goto FAIL;
            }
            date.tm_hour = 23;
            date.tm_min  = 59;
            date.tm_sec  = 0;
            date.tm_isdst = -1;
            dateTo = mktime(&date);
        }

        if(dateFrom > dateTo && dateTo != 0) {
            fprintf(stderr, "from date must preceed to date\n");
            goto FAIL;
        }
        status = readCalFile(ics, &comp);
        if(status.code == OK) {
            status = calFilter(comp, opt, dateFrom, dateTo, ocs);
        } else {
            parseError(argv[2], status); 
            goto FAIL;
        }
    } else if(strcmp(argv[1], "-combine") == 0) {
        FILE* fCombine = fopen(argv[2], "r");
        CalComp* combine = NULL;
            
        if(argc != 3) {
            fprintf(stderr, "combine usage: %s -combine <file>\n", argv[0]);
            goto FAIL;
        }
            
        if(fCombine == NULL) {
            fprintf(stderr, "can't open %s\n", argv[2]);
            goto FAIL;
        } else {
            status = readCalFile(fCombine, &combine);
            fclose(fCombine);
            if(status.code != OK) {
                parseError("stdin", status); 
                goto FAIL;
            }
        }
        status = readCalFile(ics, &comp);
        if(status.code == OK) {
            status = calCombine(comp, combine, ocs);
            freeCalComp(combine);
        } else {
            parseError(argv[2], status); 
            goto FAIL;
        }
    } else {
        fprintf(stderr, "options: -info, -extract, -filter, -combine\n");
        goto FAIL;
    }
    
    fclose(ocs);
    fclose(ics);
    free(todayStr);
    freeCalComp(comp);

    if(status.code != OK) {
        parseError("stdout", status);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
    
    FAIL:
    fclose(ocs);
    fclose(ics);
    free(todayStr);
    freeCalComp(comp);
    return EXIT_FAILURE;
}
