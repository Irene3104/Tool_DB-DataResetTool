#pragma once

#ifdef GC_VERSION
#define GENORAYDB "gc.db"
#define GENORAYDB_WEB "aadvaDB";

#else
#define GENORAYDB "genoray.db"
#define GENORAYDB_WEB "TheiaDB";
#endif

const QString ObjectText_dbopen = " to open DB.";
const QString ObjectText_dbclose = " to close DB.";
const QString ObjectText_dbreset = " to reset DB.";

const QString ObjectText_patientTable = " to reset patient DB.";
const QString ObjectText_patientDeleteTable = " to reset patientDelete DB.";
const QString ObjectText_imgTable = " to reset Image DB.";

const QString ObjectText_CategoryTable = " to reset Category DB.";
const QString ObjectText_PhysicianTable = " to reset Physician DB.";
const QString ObjectText_VersionTable = " to reset Version DB.";
const QString ObjectText_sequenceTable = " to reset sqlite_sequence DB.";

const QString ObjectText_stmountTable = " to reset STmount DB.";
const QString ObjectText_tlitemTable = " to reset TLitem DB.";
const QString ObjectText_tlsplitterTable = " to reset TLsplitter DB.";

const QString ObjectText_PatientFolder = " to reset Patient Folder.";

const QString FailText = "Failed";
const QString SucText = "Successed";