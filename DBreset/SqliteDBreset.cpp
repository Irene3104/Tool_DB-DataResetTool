#include "SqliteDBreset.h"
#include "stdafx.h"
#include "DbStruct.h"
#include "DBSQLite.h"

TheiaSqliteDB::TheiaSqliteDB()
{
	m_SqliteDB = new CDBSQLite;
}

TheiaSqliteDB::~TheiaSqliteDB()
{
	if (m_SqliteDB != nullptr)
		delete m_SqliteDB;
}

static int callback(void *data, int argc, char **argv, char **azColName)
{
	int i;
	fprintf(stderr, "%s: ", (const char*)data);

	for (i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");
	return 0;
}

bool TheiaSqliteDB::DbOpen(QString strServerPath)
{
	bool result = false;
	std::string dbdirpath = strServerPath.toStdString().c_str();
	dbdirpath += "/";
	std::string strContianDir("Database");
	std::string dbname = GENORAYDB;
	if (dbdirpath.find(strContianDir) != string::npos)
	{
		if (m_SqliteDB->OpenConnection(dbname, dbdirpath) == SQLITE_OK)
		{
			qDebug() << " Successed to open Theia db." << endl;
			result = true;
		}
		else
		{
			qDebug() << " Failed to open Theia db." << endl;
			return false;
		}
	}
	else
	{
		qDebug() << "Database file Path is wrong. Check again the right path." << endl;
		return false;
	}


	emit UpdateStep("Processing", 20);
	return result;
}


bool TheiaSqliteDB::DbReset(QString tablename)
{
	bool result = false;
	int count = 0;
	sqlite3* pDB = this->m_SqliteDB->GetDB();
	if (pDB == NULL)
	{
		if (!m_SqliteDB->CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] DB Connection fail";
			return false;
		}
		pDB = this->m_SqliteDB->GetDB();
	}
	sqlite3_stmt* pStmt = this->m_SqliteDB->GetStatement();
	int nRet = SQLITE_OK;
	char* pszErrMsg = NULL;
	
	QString strquery = QString("DELETE FROM %1").arg(stringtoquery(tablename));
	nRet = sqlite3_exec(pDB, strquery.toStdString().c_str(), callback, NULL, &pszErrMsg);
	if (nRet== SQLITE_OK)
	{
		result = true;
		
	}
	else
	{
	/*	QString s1 = QString::fromUtf8(pszErrMsg);
		QString s2 = QString::fromLatin1(pszErrMsg);
		QString s3 = QString::fromLocal8Bit(pszErrMsg);
		QString s4 = QString::fromStdString(pszErrMsg);*/

		// 실패 원인 확인 1. Code
		int nErrCode = sqlite3_errcode(pDB);
		qDebug() << QString::number(nErrCode) << endl;

		// 실패 원인 확인 2. Message
		const char* pszErrMessage = sqlite3_errmsg(pDB);
		qDebug() << pszErrMessage << endl;

		return false;
	}

	emit UpdateStep("Processing", count++);
	return result;
}

QString TheiaSqliteDB::stringtoquery(QString strinput)
{
	return QString("%1").arg(strinput);
}


bool TheiaSqliteDB::DbClose()
{
	bool result = false;
	if (m_SqliteDB->isConnected())
	{
		int nRet = SQLITE_OK;
		nRet = this->m_SqliteDB->CloseConnection();
		if (SQLITE_OK != nRet)
		{
			nRet = sqlite3_finalize(NULL);
			qDebug() <<  "[CloseTheiaDB] Failed to close Theia db." + QString::fromStdString(this->m_SqliteDB->GetLatestError()) << endl;
			return false;
		}
		else
		{
			qDebug() << "[CloseTheiaDB] Successed to close Theia db." << endl;
			result = true;
		}
		emit UpdateStep("Processing", 60);
		return result;
	}
}

bool TheiaSqliteDB::ResetPatientFolder(QString strDataPath)
{
	bool result = false;
	QString dataFolder = strDataPath + "/";
	dataFolder.replace("/", "\\");
	std::string strContainDir = "Patient";
	QDir PatientDir(dataFolder);
	QString theiaFolder = dataFolder.chopped(8);
	QDir TheiaDir(theiaFolder);

	if (dataFolder.toStdString().find(strContainDir) != string::npos)
	{
		if (PatientDir.exists())
		{
			PatientDir.removeRecursively();
			TheiaDir.mkdir("Patient");
			result = true;
		}
	}
	else
	{
		qDebug() << "Not exists Patient Folder. Check again the right path." << endl;
		return false;
	}

	emit UpdateStep("Processing",90);
	return result;
}

bool TheiaSqliteDB::run(QString* strlog, QString strServerPath, QString strDataPath)
{
	bool result = false;
	QString failLog;
	QString resultText = "";

	QString tbPateint = "patient";
	QString tbImg = "ImageDB";
	QString tbPateintDelete = "patientDelete";
	QString tbCategory = "Category";
	QString tbPhysician = "Physician";
	QString tbVersion = "Version";
	QString tbsqlite_sequence = "sqlite_sequence";


	result = DbOpen(strServerPath);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_dbopen + "\n";
	if (result == false)
		return false;

	result = DbReset(tbPateint);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_patientTable + "\n";
	if (result == false)
		return false;

	result = DbReset(tbImg);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_imgTable + "\n";
	if (result == false)
		return false;

	result = DbReset(tbPateintDelete);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_patientDeleteTable + "\n";
	if (result == false)
		return false;


	result = DbReset(tbCategory);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_CategoryTable + "\n";
	if (result == false)
		return false;


	result = DbReset(tbPhysician);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_PhysicianTable + "\n";
	if (result == false)
		return false;


	result = DbReset(tbVersion);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_VersionTable + "\n";
	if (result == false)
		return false;

	result = DbReset(tbsqlite_sequence);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_sequenceTable + "\n";
	if (result == false)
		return false;
	
	result = DbClose();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_dbclose + "\n";
	if (result == false)
		return false;

	result = ResetPatientFolder(strDataPath);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_PatientFolder + "\n";
	if (result == false)
		return false;


	emit UpdateStep("100%", 100);
	return result;
}