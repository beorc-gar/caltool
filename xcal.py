#!usr/bin/python3

'''
 ' xcal.py
 ' Date: April 7, 2016
 ' Description: Tkinter GUI to interact with caltool + mySQL
'''

import sys, os, getpass
import mysql.connector as sql
import tkinter as tk
from tkinter import *
from tkinter import filedialog as fd
import random
from random import randint
import Cal

def holdfocus():
    global focus
    focus = window.focus_get()

def dbstatus():
    cursor.execute("SELECT COUNT(*) FROM EVENT")
    logtext.set(logtext.get()+"\nDatabase has "+str(cursor.fetchone()[0])+" events, ")
    cursor.execute("SELECT COUNT(*) FROM TODO")
    logtext.set(logtext.get()+str(cursor.fetchone()[0])+" todo-items, ")
    cursor.execute("SELECT COUNT(*) FROM ORGANIZER")
    logtext.set(logtext.get()+str(cursor.fetchone()[0])+" organizers.\n")

def storeall():
    global result
    for r in result[1]:
        if r[0] == "VEVENT" or r[0] == "VTODO":
            org_id = -1
            cursor.execute("SELECT COUNT(*) FROM ORGANIZER WHERE name=\"{}\"".format(r[4]))
            if cursor.fetchone()[0] == 0:
                cursor.execute("INSERT INTO ORGANIZER (name, contact) \
                    VALUES (\"{}\", \"{}\")".format(r[4], r[5]))
                database.commit()
            cursor.execute("SELECT org_id FROM ORGANIZER WHERE name=\"{}\"".format(r[4]))
            ftch = cursor.fetchone()
            if ftch != None:
                org_id = ftch[0]

            if r[0] == "VEVENT":
                cursor.execute("SELECT COUNT(*) FROM EVENT WHERE summary=\"{}\" \
                    AND start_time=\"{}\"".format(r[3], r[6]))
                if cursor.fetchone()[0] == 0 and org_id >= 0:
                    cursor.execute("INSERT INTO EVENT (summary, start_time, location, organizer) \
                        VALUES (\"{}\", \"{}\", \"{}\", {})".format(r[3], r[6], r[7], org_id))
            elif r[0] ==  "VTODO":
                cursor.execute("SELECT COUNT(*) FROM TODO WHERE summary=\"{}\"".format(r[3]))
                if cursor.fetchone()[0] == 0 and org_id >= 0:
                    cursor.execute("INSERT INTO TODO (summary, priority, organizer) \
                        VALUES (\"{}\", \"{}\", {})".format(r[3], r[8], org_id))
            database.commit()
    dbstatus()

def storesel():
    global result
    global entry

    global focus
    flag = False
    compNum = -1
    for i in range(len(entry)):
        for e in entry[i]:
            if e == focus:
                flag = True
                compNum = i
                break
        if flag:
            break
    if compNum >= 0:
        if result[1][compNum][0] == "VEVENT" or result[1][compNum][0] == "VTODO":
            org_id = -1
            cursor.execute("SELECT COUNT(*) FROM ORGANIZER WHERE name=\"{}\"".format(result[1][compNum][4]))
            if cursor.fetchone()[0] == 0:
                cursor.execute("INSERT INTO ORGANIZER (name, contact) \
                    VALUES (\"{}\", \"{}\")".format(result[1][compNum][4], result[1][compNum][5]))
                database.commit()
            cursor.execute("SELECT org_id FROM ORGANIZER WHERE name=\"{}\"".format(result[1][compNum][4]))
            ftch = cursor.fetchone()
            if ftch != None:
                org_id = ftch[0]

            if result[1][compNum][0] == "VEVENT":
                cursor.execute("SELECT COUNT(*) FROM EVENT WHERE summary=\"{}\" \
                    AND start_time=\"{}\"".format(result[1][compNum][3], result[1][compNum][6]))
                if cursor.fetchone()[0] == 0 and org_id >= 0:
                    cursor.execute("INSERT INTO EVENT (summary, start_time, location, organizer) \
                        VALUES (\"{}\", \"{}\", \"{}\", {})"\
                        .format(result[1][compNum][3], result[1][compNum][6], result[1][compNum][7], org_id))
            elif result[1][compNum][0] ==  "VTODO":
                cursor.execute("SELECT COUNT(*) FROM TODO WHERE summary=\"{}\"".format(result[1][compNum][3]))
                if cursor.fetchone()[0] == 0 and org_id >= 0:
                    cursor.execute("INSERT INTO TODO (summary, priority, organizer) \
                        VALUES (\"{}\", \"{}\", {})".format(result[1][compNum][3], result[1][compNum][8], org_id))
            database.commit()
    dbstatus()

def clearsql():
    cursor.execute("TRUNCATE TABLE TODO")
    cursor.execute("TRUNCATE TABLE EVENT")
    cursor.execute("DELETE FROM ORGANIZER")
    database.commit()
    dbstatus()

def querysql():
    dialog = queryDialog(window)

def save():
    if filename.get() == "":
        return
    os.system("cat .save.ics > \""+filename.get()+"\"")
    logtext.set(logtext.get()+"\n\n"+os.popen("wc -l .save.ics").read()+" lines written.\n")
    saved.set(True)
    string = list(filename.get())
    for i in range(len(filename.get())):
        if string[i] == '/':
            place = i+1
    window.title('xcal - ' + filename.get()[place:])

def saveas():
    if filename.get() == "":
        return
    saveto = fd.asksaveasfilename(filetypes=[('calendar files', '.ics'), ('text files', '.txt')])
    if saveto != "" and saveto != None:
        os.system("cat .save.ics > \""+saveto+"\"")
        saved.set(True)
        filename.set(saveto)
        string = list(filename.get())
        for i in range(len(filename.get())):
            if string[i] == '/':
                place = i+1
        window.title('xcal - ' + filename.get()[place:])

def combine():
    if filename.get() == "":
        logtext.set(logtext.get()+"\n\nYou must open a file before combining.")
        return
    file2 = fd.askopenfilename(filetypes=[('calendar files', '.ics'), ('text files', '.txt')])
    if file2 == "" or file2 == None:
        return
    os.system("./caltool -combine \""+file2+"\" < .save.ics > .xcal.log 2> .xcal.tmp")
    logtext.set(logtext.get()+"\n\n"+open(".xcal.log").read()+open(".xcal.tmp").read())
    os.system("cat .xcal.log > .save.ics")
    os.system("rm -f .xcal.*")
    saved.set(False)
    string = list(filename.get())
    for i in range(len(filename.get())):
        if string[i] == '/':
            place = i+1
    window.title('xcal - ' + filename.get()[place:]+"*")
    Cal.freeFile(result[0])
    tup = Cal.readFile(".save.ics")
    result[0] = tup[0]
    result[1] = tup[1]
    if result[0] == "error":
        logtext.set(logtext.get()+"\n\n"+result[1])
        return
    tup = Cal.readFile(".save.ics")
    result[0] = tup[0]
    result[1] = tup[1]
    if result[0] == "error":
        logtext.set(logtext.get()+"\n\n"+result[1])
        return
    for i in range(len(result[1])):
        s = StringVar()
        s.set("")
        entry.append([s, s, s, s, s])
        entry[i][0] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][0].insert(0, str(i+1))
        entry[i][0].configure(state='readonly')
        entry[i][0].grid(row=i+1, column=0)
        entry[i][1] = Entry(frame, width=25, validate="focusin", vcmd=holdfocus)
        entry[i][1].insert(0, result[1][i][0])
        entry[i][1].configure(state='readonly')
        entry[i][1].grid(row=i+1, column=1)
        entry[i][2] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][2].insert(0, str(result[1][i][1]))
        entry[i][2].configure(state='readonly')
        entry[i][2].grid(row=i+1, column=2)
        entry[i][3] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][3].insert(0, str(result[1][i][2]))
        entry[i][3].configure(state='readonly')
        entry[i][3].grid(row=i+1, column=3)
        entry[i][4] = Entry(frame, width=30, validate="focusin", vcmd=holdfocus)
        entry[i][4].insert(0, result[1][i][3])
        entry[i][4].configure(state='readonly')
        entry[i][4].grid(row=i+1, column=4)


def filter():
    if filename.get() == "":
        logtext.set(logtext.get()+"\n\nYou must open a file before filtering.")
        return
    dialog = fDialog(window)
    window.wait_window(dialog.top)
    saved.set(False)
    string = list(filename.get())
    for i in range(len(filename.get())):
        if string[i] == '/':
            place = i+1
    window.title('xcal - ' + filename.get()[place:]+"*")
    Cal.freeFile(result[0])
    tup = Cal.readFile(".save.ics")
    result[0] = tup[0]
    result[1] = tup[1]
    if result[0] == "error":
        logtext.set(logtext.get()+"\n\n"+result[1])
        return
    for i in range(len(result[1])):
        s = StringVar()
        s.set("")
        entry.append([s, s, s, s, s])
        entry[i][0] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][0].insert(0, str(i+1))
        entry[i][0].configure(state='readonly')
        entry[i][0].grid(row=i+1, column=0)
        entry[i][1] = Entry(frame, width=25, validate="focusin", vcmd=holdfocus)
        entry[i][1].insert(0, result[1][i][0])
        entry[i][1].configure(state='readonly')
        entry[i][1].grid(row=i+1, column=1)
        entry[i][2] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][2].insert(0, str(result[1][i][1]))
        entry[i][2].configure(state='readonly')
        entry[i][2].grid(row=i+1, column=2)
        entry[i][3] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][3].insert(0, str(result[1][i][2]))
        entry[i][3].configure(state='readonly')
        entry[i][3].grid(row=i+1, column=3)
        entry[i][4] = Entry(frame, width=30, validate="focusin", vcmd=holdfocus)
        entry[i][4].insert(0, result[1][i][3])
        entry[i][4].configure(state='readonly')
        entry[i][4].grid(row=i+1, column=4)

def exte():
    os.system("./caltool -extract e < .save.ics > .xcal.log 2> .xcal.tmp")
    logtext.set(logtext.get()+"\n\n"+open(".xcal.log").read()+open(".xcal.tmp").read())
    os.system("rm -f .xcal.*")

def extx():
    os.system("./caltool -extract x < .save.ics > .xcal.log 2> .xcal.tmp")
    logtext.set(logtext.get()+"\n\n"+open(".xcal.log").read()+open(".xcal.tmp").read())
    os.system("rm -f .xcal.*")

def show():
    focus = window.focus_get()
    flag = False
    compNum = -1
    for i in range(len(entry)):
        for e in entry[i]:
            if e == focus:
                flag = True
                compNum = i
                break
        if flag:
            break
    if compNum >= 0:
        os.system("touch .selected.ics")
        write = Cal.writeFile(".selected.ics", result[0], compNum)
        if write == "OK":
            logtext.set(logtext.get()+"\n\n"+open(".selected.ics").read())
        else:
            logtext.set(logtext.get()+"\n\n"+write)
        os.system("rm -f .selected.ics")

def openfile():
    if not saved.get():
        saveprompt = openDialog(window)
        window.wait_window(saveprompt.top)
        if saveprompt.cancel.get():
            return
        Cal.freeFile(result[0])
    newfile = fd.askopenfilename(filetypes=[('calendar files', '.ics'), ('text files', '.txt')])
    if newfile == "" or newfile == None:
        return

    os.system("cat \""+newfile+"\" > .save.ics")
    os.system("./caltool -info < .save.ics > .xcal.log 2> .xcal.tmp")
    if open(".xcal.tmp").read() != "":
        logtext.set(open(".xcal.tmp").read())
        os.system("rm -f .xcal.*")
        return
    tup = Cal.readFile(".save.ics")
    result[0] = tup[0]
    result[1] = tup[1]
    if result[0] == "error":
        logtext.set(logtext.get()+"\n\n"+result[1])
        return
    string = list(newfile)
    for i in range(len(newfile)):
        if string[i] == '/':
            place = i+1
    window.title('xcal - ' + newfile[place:])
    filename.set(newfile)
    saved.set(True)

    logtext.set(open(".xcal.log").read())
    os.system("rm -f .xcal.*")

    global entry
    for e in entry:
        for f in e:
            f.destroy()
    entry = []

    for i in range(len(result[1])):
        s = StringVar()
        s.set("")
        entry.append([s, s, s, s, s])
        entry[i][0] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][0].insert(0, str(i+1))
        entry[i][0].configure(state='readonly')
        entry[i][0].grid(row=i+1, column=0)
        entry[i][1] = Entry(frame, width=25, validate="focusin", vcmd=holdfocus)
        entry[i][1].insert(0, result[1][i][0])
        entry[i][1].configure(state='readonly')
        entry[i][1].grid(row=i+1, column=1)
        entry[i][2] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][2].insert(0, str(result[1][i][1]))
        entry[i][2].configure(state='readonly')
        entry[i][2].grid(row=i+1, column=2)
        entry[i][3] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
        entry[i][3].insert(0, str(result[1][i][2]))
        entry[i][3].configure(state='readonly')
        entry[i][3].grid(row=i+1, column=3)
        entry[i][4] = Entry(frame, width=30, validate="focusin", vcmd=holdfocus)
        entry[i][4].insert(0, result[1][i][3])
        entry[i][4].configure(state='readonly')
        entry[i][4].grid(row=i+1, column=4)

    if not openfile.called:
        Button(buttonpane, text="Show Selected",   width=10, command=show).pack()
        Button(buttonpane, text="Extract Events",  width=10, command=exte).pack()
        Button(buttonpane, text="Extract X-Props", width=10, command=extx).pack()

        menubar = tk.Menu(window)
        filemenu = tk.Menu(menubar, tearoff=0)
        filemenu.add_command(label="Open...",    command=openfile, accelerator="Ctrl+O")
        filemenu.add_command(label="Save",       command=save,     accelerator="Ctrl+S")
        filemenu.add_command(label="Save As...", command=saveas)
        filemenu.add_command(label="Combine...", command=combine)
        filemenu.add_command(label="Filter...",  command=filter)
        filemenu.add_separator()
        filemenu.add_command(label="Exit", command=exit, accelerator="Ctrl+X")
        menubar.add_cascade (label="File", menu=filemenu)

        todomenu = tk.Menu(menubar, tearoff=0)
        todomenu.add_command(label="To-do List...", command=todo, accelerator="Ctrl+T")
        todomenu.add_command(label="Undo",          command=undo, accelerator="Ctrl+Z")
        menubar.add_cascade (label="Todo", menu=todomenu)

        datemenu = tk.Menu(menubar, tearoff=0)
        datemenu.add_command(label="Date Mask...",  command=datemsk)
        datemenu.add_command(label="About xcal...", command=aboutme)
        menubar.add_cascade (label="Help", menu=datemenu)

        basemenu = tk.Menu(menubar, tearoff=0)
        basemenu.add_command(label="Store All",      command=storeall)
        basemenu.add_command(label="Store Selected", command=storesel)
        basemenu.add_command(label="Clear",          command=clearsql)
        basemenu.add_command(label="Status",         command=dbstatus)
        basemenu.add_command(label="Query",          command=querysql)
        menubar.add_cascade (label="Database", menu=basemenu)

        window.bind_all("<Control-o>", openk)
        window.bind_all("<Control-s>", savek)
        window.bind_all("<Control-x>", exitk)
        window.bind_all("<Control-t>", todok)
        window.bind_all("<Control-z>", undok)

        window.config(menu=menubar)
        openfile.called = True
openfile.called = False

def exit():
    if not saved.get():
        saveprompt = saveDialog(window)
        window.wait_window(saveprompt.top)
        if saveprompt.cancel.get():
            return
    else:
        close()

def close():
    if result[0] != 1:
        Cal.freeFile(result[0])
    os.system("rm -f .xcal.* .*.ics")
    database.close()
    sys.exit();

def todo():
    todoprompt = todoDialog(window)

def undo():
    global completed
    for c in completed:
        c[0].grid(row=int(c[0].get()), column=0)
        c[1].grid(row=int(c[0].get()), column=1)
        c[2].grid(row=int(c[0].get()), column=2)
        c[3].grid(row=int(c[0].get()), column=3)
        c[4].grid(row=int(c[0].get()), column=4)
        entry.insert(int(c[0].get())-1, c)
    completed = []

def datemsk():
    datemaskvar = fd.askopenfilename()
    if datemaskvar != "" and datemaskvar != None:
        os.environ['DATEMSK'] = datemaskvar

def aboutme():
    top = Toplevel(window)
    top.wm_title("About xcal")
    
    title   = "xcal"
    author  = "Bronson Graansma"
    version = "iCalendar v2.0"

    Label(top, text=title,                      font=("Times", 22, "bold"), fg="navy").pack()
    Label(top, text="by "+author,               font=("Helvetica", 14),     fg="blue").pack()
    Label(top, text="compatible with "+version, font=("Helvetica", 10),     fg="blue").pack()
    
    center(top)
    top.resizable(0, 0)


def openk(self):
    openfile()

def savek(self):
    save()

def exitk(self):
    exit()

def todok(self):
    todo()

def undok(self):
    undo()

def clear():
    logtext.set("")

def center(toplevel):
    toplevel.update_idletasks()
    w = toplevel.winfo_screenwidth()
    h = toplevel.winfo_screenheight()
    size = tuple(int(_) for _ in toplevel.geometry().split('+')[0].split('x'))
    x = w/2 - size[0]/2
    y = h/2 - size[1]/2
    toplevel.geometry("%dx%d+%d+%d" % (size + (x, y)))

def initpane():
    e = Entry(frame, width=5)
    e.insert(0,'No.')
    e.configure(state='readonly', justify='center')
    e.grid(row=0, column=0)
    e = Entry(frame, width=25)
    e.insert(0,'Name')
    e.configure(state='readonly', justify='center')
    e.grid(row=0, column=1)
    e = Entry(frame, width=5)
    e.insert(0,'Props')
    e.configure(state='readonly', justify='center')
    e.grid(row=0, column=2)
    e = Entry(frame, width=5)
    e.insert(0,'Subs')
    e.configure(state='readonly', justify='center')
    e.grid(row=0, column=3)
    e = Entry(frame, width=30)
    e.insert(0,'Summary')
    e.configure(state='readonly', justify='center')
    e.grid(row=0, column=4)
    for i in range(8):
       Entry(frame, width=5,  state="readonly").grid(row=i+1, column=0)
       Entry(frame, width=25, state="readonly").grid(row=i+1, column=1)
       Entry(frame, width=5,  state="readonly").grid(row=i+1, column=2)
       Entry(frame, width=5,  state="readonly").grid(row=i+1, column=3)
       Entry(frame, width=30, state="readonly").grid(row=i+1, column=4)

def scrollto(event):
    canvas.configure(scrollregion=canvas.bbox("all"), width=590,height=190)
    
def scrolltolog(event):
    logcanvas.configure(scrollregion=logcanvas.bbox("all"), width=590,height=190)

def combine_funcs(*funcs):
    def combined_func(*args, **kwargs):
        for f in funcs:
            f(*args, **kwargs)
    return combined_func

class Dialog:
    def __init__(self, parent):
        top = self.top = Toplevel(parent)
        top.title("Date Mask")

        Label(top, text="Date Mask is not currently set.\nWould you like to set it now?").pack(padx=10, pady=10)

        Button(top, text="  Yes  ", command=combine_funcs(datemsk, top.destroy))\
            .pack(in_=top, side=LEFT, expand=YES)
        Button(top, text="Not now", command=top.destroy).pack(in_=top, side=LEFT, expand=YES)
        top.resizable(0,0)
        center(top)

class queryDialog:
    def scrollyto(self, event):
        self.rightcanvas.configure(scrollregion=self.rightcanvas.bbox("all"))

    def help(self):
        helpscrn = Toplevel(self.top)
        describe = StringVar()

        rows=0
        helpscrn.title("Help")
        winder = Frame(helpscrn)
        
        cursor.execute("DESCRIBE ORGANIZER")
        Label(winder, text="ORGANIZER").pack()
        frame = Frame(winder)
        for f in cursor.fetchall():
            rows += 1
            Label(frame, text=str(f[0])).grid(row=rows, column=0)
            Label(frame, text="|"      ).grid(row=rows, column=1)
            Label(frame, text=str(f[1])).grid(row=rows, column=2)
        frame.pack()

        Frame(winder, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        cursor.execute("DESCRIBE EVENT")
        Label(winder, text="EVENT").pack()
        frame = Frame(winder)
        for f in cursor.fetchall():
            rows += 1
            Label(frame, text=str(f[0])).grid(row=rows, column=0)
            Label(frame, text="|"      ).grid(row=rows, column=1)
            Label(frame, text=str(f[1])).grid(row=rows, column=2)
        frame.pack()

        Frame(winder, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        cursor.execute("DESCRIBE TODO")
        Label(winder, text="TODO").pack()
        frame = Frame(winder)
        for f in cursor.fetchall():
            rows += 1
            Label(frame, text=str(f[0])).grid(row=rows, column=0)
            Label(frame, text="|"      ).grid(row=rows, column=1)
            Label(frame, text=str(f[1])).grid(row=rows, column=2)
        frame.pack()

        winder.pack()

        helpscrn.resizable(0,0)
        center(helpscrn)

    def submit(self):
        if   self.v.get() == 1:
            cursor.execute("SELECT summary FROM EVENT INNER JOIN ORGANIZER \
                ON ORGANIZER.org_id=EVENT.organizer AND ORGANIZER.name=\"{}\"\
                ".format(self.orgEntry.get()))

            self.text.set(self.text.get()+\
                "------------------------------------------------------------\n")
            for f in cursor.fetchall():
                self.text.set(self.text.get()+str(f[0])+"\n")
            cursor.execute("SELECT summary FROM TODO INNER JOIN ORGANIZER \
                ON ORGANIZER.org_id=TODO.organizer AND ORGANIZER.name=\"{}\""\
                .format(self.orgEntry.get()))

        elif self.v.get() == 2:
            cursor.execute("SELECT summary FROM EVENT WHERE location=\"{}\"".format(self.locEntry.get()))
            self.text.set(self.text.get()+\
                "------------------------------------------------------------\n")

        elif self.v.get() == 3:
            if self.startEntry.get() != "" and self.endEntry.get() != "":
                cursor.execute("SELECT summary FROM EVENT WHERE start_time >=\"{}\" AND start_time <=\"{}\""\
                    .format(self.startEntry.get(), self.endEntry.get()))
            elif self.startEntry.get() != "":
                cursor.execute("SELECT summary FROM EVENT WHERE start_time >=\"{}\"".format(self.startEntry.get()))
            elif self.endEntry.get() != "":
                cursor.execute("SELECT summary FROM EVENT WHERE start_time <=\"{}\"".format(self.endEntry.get()))
            else:
                return
            self.text.set(self.text.get()+\
                "------------------------------------------------------------\n")

        elif self.v.get() == 4:
            cursor.execute("SELECT summary FROM TODO WHERE priority <={}".format(self.priorEntry.get()))
            self.text.set(self.text.get()+\
                "------------------------------------------------------------\n")

        elif self.v.get() == 5:
            string = ""
            try:
                cursor.execute(self.selEntry.get(1.0, "end"))
            except sql.Error:
                string = "Syntax Error\n"

            self.text.set(self.text.get()+\
                "------------------------------------------------------------\n")
            if string != "":
                self.text.set(self.text.get()+string)
                return
            for f in cursor.fetchall():
                self.text.set(self.text.get()+str(f)+"\n")
            return
        
        elif self.v.get() == 6:
            tables = ("EVENT", "ORGANIZER", "TODO")
            keys   = ("event_id", "org_id", "todo_id")
            rand = randint(0,2)
            cursor.execute("SELECT MIN({}), MAX({}) FROM {}"\
                .format(keys[rand], keys[rand], tables[rand]))
            fetched = cursor.fetchall()
            if fetched != None:
                cursor.execute("SELECT * FROM {} WHERE {}={}"\
                    .format(tables[rand], keys[rand], randint(fetched[0][0], fetched[0][1])))
                for f in cursor.fetchall():
                    self.text.set(self.text.get()+str(f[1])+"\n")
            return
        
        for f in cursor.fetchall():
            self.text.set(self.text.get()+str(f[0])+"\n")

    def clearResults(self):
        self.text.set("")

    def __init__(self, parent):
        top = self.top = Toplevel(parent)
        top.title("Query")

        v = self.v = IntVar()

        leftpane  = Frame(top)
        rightpane = Frame(top, relief="sunken", bd=1)

        frame = Frame(leftpane)
        subframe = Frame(frame)
        Radiobutton(subframe, text="Display the items of:", variable=v, value=1).pack(side="left")
        self.orgEntry = Entry(subframe)
        self.orgEntry.pack(side="right")
        subframe.pack(side="top")
        Label(frame, text="(organizer)").pack(side="bottom")
        frame.pack(anchor="w")

        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        frame = Frame(leftpane)
        Radiobutton(frame, text="Display the events that take place in:", variable=v, value=2).pack(side="top")
        self.locEntry = Entry(frame)
        Label(frame, text="(location)").pack(side="right")
        self.locEntry.pack(side="right")
        frame.pack(anchor="w")

        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        frame = Frame(leftpane)
        Radiobutton(frame, text="Display the events that take place between:", variable=v, value=3).pack(side="top", anchor="w")
        self.endEntry = Entry(frame, width=15)
        self.endEntry.pack(side="right")
        Label(frame, text="and").pack(side="right")
        self.startEntry = Entry(frame, width=15)
        self.startEntry.pack(side="right")
        frame.pack(anchor="w")

        Label(leftpane, text="YYYY-MM-DD HH:MM:SS").pack()
        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)
        
        frame = Frame(leftpane)
        Radiobutton(frame, text="Display to-do items prioritized:", variable=v, value=4).pack(side="left")
        self.priorEntry = Entry(frame, width=5)
        self.priorEntry.pack(side="left")
        Label(frame, text="or higher").pack(side="left")
        frame.pack(anchor="w")

        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        Radiobutton(leftpane, text="Randomize", variable=v, value=6).pack(anchor="w")

        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        frame = Frame(leftpane)
        Radiobutton(frame, variable=v, value=5).pack(side="left")
        self.selEntry = Text(frame, height=6, width=40)
        self.selEntry.insert("end", "SELECT ")
        self.selEntry.pack(side="left")
        frame.pack(anchor="w")

        Frame(leftpane, height=2, bd=1, relief="sunken").pack(fill="x", padx=5, pady=5)

        frame = Frame(leftpane)
        Button(frame, text=" Help ", command=self.help).pack(side="left" )
        Button(frame, text="Submit", command=self.submit).pack(side="right")
        frame.pack()

        leftpane.pack(side="left")

        self.text = StringVar()

        self.rightcanvas = Canvas(rightpane, bg="white")

        scrolly = Scrollbar(rightpane, orient="vertical", command=self.rightcanvas.yview)
        self.rightcanvas.configure(yscrollcommand=scrolly.set)
        scrolly.pack(side="left", fill="y")

        frame = Frame(self.rightcanvas, bg="white")
        Label(frame, textvariable=self.text, justify="left", bg="white")\
            .pack(fill="both", expand="yes")
        self.rightcanvas.pack(side="left", fill="both", expand="yes")

        self.rightcanvas.create_window((0, 0), window=frame, anchor='nw')
        frame.bind("<Configure>", self.scrollyto)

        Button(rightpane, text="Clear", command=self.clearResults).pack()

        rightpane.pack(side="right", padx=5, pady=5, fill="both")

        top.resizable(0,0)
        center(top)

class saveDialog:
    def cancelwin(self):
        self.cancel.set(True)
        self.top.destroy()

    def cancelk(self, parent):
        self.cancelwin()

    def __init__(self, parent):
        self.cancel = BooleanVar()
        self.cancel.set(False)
        top = self.top = Toplevel(parent)
        top.title("Quit")

        Label(top, text="Discard changes before quitting?").pack(padx=10, pady=10)

        Button(top, text="Cancel", command=self.cancelwin)\
            .pack(in_=top, side=LEFT, expand=YES)
        Button(top, text="Discard", command=combine_funcs(top.destroy, close))\
            .pack(in_=top, side=LEFT, expand=YES)
        Button(top, text="Save", command=combine_funcs(save, top.destroy, close))\
            .pack(in_=top, side=LEFT, expand=YES)
        
        top.protocol('WM_DELETE_WINDOW', self.cancelwin)
        top.bind_all("<Escape>", self.cancelk)

        top.resizable(0,0)
        center(top)

class todoDialog:
    def cancelwin(self):
        self.cancel.set(True)
        self.top.destroy()

    def done(self):
        j = 0
        removed = 0
        for i in range(len(result[1])):
            if result[1][i][0] == "VTODO":
                if j < len(self.vars) and self.vars[j].get():
                    entry[i-removed][0].grid_remove()
                    entry[i-removed][1].grid_remove()
                    entry[i-removed][2].grid_remove()
                    entry[i-removed][3].grid_remove()
                    entry[i-removed][4].grid_remove()
                    completed.append(entry[i-removed])
                    entry.remove(entry[i-removed])

                    removed+=1
                j+=1
        self.top.destroy()

    def cancelk(self, parent):
        self.cancelwin()
    
    def scrollto(self, event):
        self.canvas.configure(scrollregion=self.canvas.bbox("all"))

    def stateof(self):
        for var in self.vars:
            if var:
                self.b.config(state="normal")
                return
        self.b.config(state="disabled")

    def __init__(self, parent):
        self.cancel = BooleanVar()
        self.cancel.set(False)
        top = self.top = Toplevel(parent)
        top.title("To-do List")
        self.vars  = []
        self.check = []

        todos = Frame(top, relief="sunken", bd=1)
        todos.pack(ipadx=5, ipady=5)
        self.canvas = Canvas(todos)

        scrollBar = Scrollbar(todos, orient="vertical", command=self.canvas.yview)
        self.canvas.configure(yscrollcommand=scrollBar.set)
        scrollBar.pack(side="left", fill="y")

        frame = Frame(self.canvas)

        for i in range(len(entry)):
            if entry[i][1].get() == "VTODO":
                self.vars.append(BooleanVar())
                self.vars[-1].set(False)
                c = Checkbutton(frame, command=self.stateof, text=entry[i][4].get(),\
                    variable=self.vars[-1], onvalue=True, offvalue=False, justify="left")
                c.grid(row=i, sticky="w")
                self.check.append(c)

        self.canvas.pack(side="left", fill="x", expand="yes")

        self.canvas.create_window((0, 0), window=frame, anchor='nw')
        frame.bind("<Configure>", self.scrollto)
        
        self.b = Button(top, text="Done", command=self.done, state="disabled")
        self.b.pack()

        top.protocol('WM_DELETE_WINDOW', self.cancelwin)
        top.bind_all("<Escape>", self.cancelk)
        
        top.resizable(0,0)
        center(top)

class openDialog:
    def cancelwin(self):
        self.cancel.set(True)
        self.top.destroy()

    def cancelk(self, parent):
        self.cancelwin()

    def __init__(self, parent):
        self.cancel = BooleanVar()
        self.cancel.set(False)
        top = self.top = Toplevel(parent)
        top.title("Open")

        Label(top, text="Discard changes before opening a new file?").pack(padx=10, pady=10)

        Button(top, text="Cancel", command=self.cancelwin)\
            .pack(in_=top, side=LEFT, expand=YES)
        Button(top, text="Discard", command=top.destroy)\
            .pack(in_=top, side=LEFT, expand=YES)
        Button(top, text="Save", command=combine_funcs(save, top.destroy))\
            .pack(in_=top, side=LEFT, expand=YES)

        top.protocol('WM_DELETE_WINDOW', self.cancelwin)
        top.bind_all("<Escape>", self.cancelk)
        
        top.resizable(0,0)
        center(top)

class fDialog:
    def okbutton(self):
        Button(self.top, text="  OK  ", command=combine_funcs(self.callFilter, self.top.destroy))\
            .grid(row=2, column=2)

    def callFilter(self):
        fromvar = ""
        tovar   = ""

        if self.val.get() != "e" and self.val.get() != "t":
            return

        if self.fromdate.get() != "":
            fromvar = " from "
            self.fromdate.set("\""+self.fromdate.get()+"\"")
        if self.todate.get() != "":
            tovar = " to "
            self.todate.set("\""+self.todate.get()+"\"")

        os.system("./caltool -filter "+self.val.get()+fromvar+self.fromdate.get()+tovar+\
            self.todate.get()+" < .save.ics > .xcal.log 2> .xcal.tmp")
        logtext.set(logtext.get()+"\n\n"+open(".xcal.log").read()+open(".xcal.tmp").read())
        os.system("cat .xcal.log > .save.ics")
        os.system("rm -f .xcal.*")

        tup = Cal.readFile(".save.ics")
        result[0] = tup[0]
        result[1] = tup[1]
        if result[0] == "error":
            logtext.set(logtext.get()+"\n\n"+result[1])
            return

        global entry
        for e in entry:
            for r in e:
                r.destroy()
        entry = []

        for i in range(len(result[1])):
            s = StringVar()
            s.set("")
            entry.append([s, s, s, s, s])
            entry[i][0] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
            entry[i][0].insert(0, str(i+1))
            entry[i][0].configure(state='readonly')
            entry[i][0].grid(row=i+1, column=0)
            entry[i][1] = Entry(frame, width=25, validate="focusin", vcmd=holdfocus)
            entry[i][1].insert(0, result[1][i][0])
            entry[i][1].configure(state='readonly')
            entry[i][1].grid(row=i+1, column=1)
            entry[i][2] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
            entry[i][2].insert(0, str(result[1][i][1]))
            entry[i][2].configure(state='readonly')
            entry[i][2].grid(row=i+1, column=2)
            entry[i][3] = Entry(frame, width=5, validate="focusin", vcmd=holdfocus)
            entry[i][3].insert(0, str(result[1][i][2]))
            entry[i][3].configure(state='readonly')
            entry[i][3].grid(row=i+1, column=3)
            entry[i][4] = Entry(frame, width=30, validate="focusin", vcmd=holdfocus)
            entry[i][4].insert(0, result[1][i][3])
            entry[i][4].configure(state='readonly')
            entry[i][4].grid(row=i+1, column=4)
    
    def __init__(self, parent):
        self.val = StringVar()
        self.fromdate = StringVar()
        self.todate = StringVar()

        top = self.top = Toplevel(parent)
        top.title("Filter")

        Label(top, text="Content:").grid(row=0, column=0)
        
        Radiobutton(top, text="Todo" , variable=self.val, value="t", command=self.okbutton)\
            .grid(row=0, column=1)
        Radiobutton(top, text="Event", variable=self.val, value="e", command=self.okbutton)\
            .grid(row=0, column=2)

        Label(top, text="From:").grid(row=1, column=0)
        Label(top, text="To:"  ).grid(row=1, column=2)
        Entry(top, textvariable=self.fromdate).grid(row=1, column=1)
        Entry(top, textvariable=self.todate).grid(row=1, column=3)

        Button(top, text="Cancel", command=top.destroy).grid(row=2, column=1)

        top.resizable(0,0)
        center(top)

############################GUI building begins here######################################

username = ""
hostname = "dursley.socs.uoguelph.ca"
password = ""
database = None

if len(sys.argv) > 1:
    username = sys.argv[1]
    if len(sys.argv) > 2:
        hostname = sys.argv[2]
else:
    print("Usage: "+sys.argv[0]+" <username> <optional hostname>")
    sys.exit()

for i in range(3):
    try:
        password = getpass.getpass("password for "+username+": ")
        database = sql.connect(database=username, user=username, password=password, host=hostname)
    except sql.Error:
        print("Failed to connect to "+hostname+"\n"+str(sql.Error))
        if i == 2:
            sys.exit()
    else:
        break

cursor = database.cursor()

cursor.execute("CREATE TABLE IF NOT EXISTS ORGANIZER (\
    org_id INT AUTO_INCREMENT PRIMARY KEY,\
    name VARCHAR(60) NOT NULL, \
    contact VARCHAR(60) NOT NULL)")

cursor.execute("CREATE TABLE IF NOT EXISTS TODO (\
    todo_id INT AUTO_INCREMENT PRIMARY KEY,\
    summary VARCHAR(60) NOT NULL,\
    priority SMALLINT,\
    organizer INT,\
    FOREIGN KEY(organizer) REFERENCES ORGANIZER(org_id) ON DELETE CASCADE)")

cursor.execute("CREATE TABLE IF NOT EXISTS EVENT (\
    event_id INT AUTO_INCREMENT PRIMARY KEY,\
    summary VARCHAR(60) NOT NULL,\
    start_time DATETIME NOT NULL,\
    location VARCHAR(60) NOT NULL,\
    organizer INT,\
    FOREIGN KEY(organizer) REFERENCES ORGANIZER(org_id) ON DELETE CASCADE)")

window = tk.Tk()

filename = StringVar()
saved = BooleanVar()
saved.set(True)
focus = None
entry  = []
result = [1, 2]
completed = []

window.title('xcal')
window.minsize(width=605+110, height=415)
window.tk.call('wm', 'iconphoto', window._w, PhotoImage(file='ical.png'))
center(window)
window.protocol('WM_DELETE_WINDOW', exit)

menubar = tk.Menu(window)

filemenu = tk.Menu(menubar, tearoff=0)
filemenu.add_command(label="Open...",    command=openfile, accelerator="Ctrl+O")
filemenu.add_separator()
filemenu.add_command(label="Exit", command=exit, accelerator="Ctrl+X")
menubar.add_cascade (label="File", menu=filemenu)

datemenu = tk.Menu(menubar, tearoff=0)
datemenu.add_command(label="Date Mask...",  command=datemsk)
datemenu.add_command(label="About xcal...", command=aboutme)
menubar.add_cascade (label="Help", menu=datemenu)

window.bind_all("<Control-o>", openk)
window.bind_all("<Control-x>", exitk)

window.config(menu=menubar)

mainframe = Frame(window)
myframe = Frame(mainframe, relief="sunken", bd=1)

canvas = Canvas(myframe)
frame = Frame(canvas)
myscrollbar = Scrollbar(myframe, orient="vertical", command=canvas.yview)
canvas.configure(yscrollcommand=myscrollbar.set)

myscrollbar.pack(side="right", fill="y")
canvas.pack(side="left", fill="x", expand=YES)
canvas.create_window((0, 0), window=frame, anchor='nw')
frame.bind("<Configure>", scrollto)
initpane()

mylog = Frame(mainframe, relief="sunken", bd=1, bg="white")

logcanvas = Canvas(mylog, bg="white")
logframe = Frame(logcanvas)
logscrollbar = Scrollbar(mylog, orient="vertical", command=logcanvas.yview)
logcanvas.configure(yscrollcommand=logscrollbar.set)

logscrollbar.pack(side="right", fill="y")
logcanvas.pack(side="left", fill="x", expand=YES)
logcanvas.create_window((0, 0), window=logframe, anchor='w')
logframe.bind("<Configure>", scrolltolog)
logcanvas.configure(scrollregion=logcanvas.bbox("all"), width=590,height=190)
logtext = StringVar()
Label(logframe, justify=LEFT, textvariable=logtext, bg="white").grid(row=0, ipadx=5, ipady=5)

buttonpane = Frame(mainframe)
clearpane  = Frame(mainframe)
Button(clearpane, text="Clear", width=10, command=clear).pack()

myframe.grid(row=0, column=0)
buttonpane.grid(row=0, column=1)
mylog.grid(row=1, column=0)
clearpane.grid(row=1, column=1)
mainframe.grid(row=0)

window.update()

if os.environ.get('DATEMSK') == None:
    dialog = Dialog(window)
    window.wait_window(dialog.top)

window.mainloop()
