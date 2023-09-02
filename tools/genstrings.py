#!/usr/bin/env python3

# This generates a data structure and initialization code for names
# in PyObject* format.  It is used especially by vtable and vfs code
# to not have to make a PyUnicode on every call from C to Python code
# and the error checking each time.  See issue #477 for benchmarking.
#
# They are generated as struct members with an eye in the future
# to the struct being attached to the module to allow multiphase
# initialization

# The output is intentionally formatted so that vscode autoformat
# makes no changes

names = ""

# virtual table
names += """
Begin BestIndex BestIndexObject Close Column ColumnNoChange Commit
Connect Create Destroy Disconnect Eof Filter FindFunction Next Open
Release Rename Rollback RollbackTo Rowid Savepoint ShadowName Sync
UpdateChangeRow UpdateDeleteRow UpdateInsertRow
"""

# vfs
names += """
xAccess xCheckReservedLock xClose xCurrentTime xCurrentTimeInt64
xDeviceCharacteristics xFileControl xFileSize xGetLastError
xGetSystemCall xDelete xDlClose xDlError xDlOpen xDlSym xFullPathname
xLock xNextSystemCall xOpen xRandomness xRead xSectorSize
xSetSystemCall xSleep xSync xTruncate xUnlock xWrite
"""

# other
names +="""
close connection_hooks cursor
"""

# tokenize names

names = sorted(set(names.split()))

header = """\
/*
    Generated by genstrings.py

    Edit that - do not edit this file
*/
"""

print(header)

print("static struct _apsw_string_table\n{")
for n in names:
    print(f"    PyObject *{ n };")
print("""} apst = {0};""")

print("""
static void
fini_apsw_strings(void)
{""")
for n in names:
    print(f"    Py_CLEAR(apst.{ n });")
print("}")

print("""
/* returns zero on success, -1 on error */
static int
init_apsw_strings()
{""")
print("    if (", end="")
for i, n in enumerate(names):
    print(f'{ " || " if i else "" }(0 == (apst.{ n } = PyUnicode_FromString("{ n }")))', end="")
print(""")
    {
        fini_apsw_strings();
        return -1;
    }
    return 0;
}""")
