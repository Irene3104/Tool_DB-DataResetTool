#include "WebDBreset.h"
#include "DBSQLite.h"

bool TheiaWebDB::DbOpen()
{
	bool result = false;

	QCoreApplication::applicationDirPath();
	QStringList listpath = QCoreApplication::libraryPaths();

	for each(auto var in listpath)
	{
		qDebug() << "Qt Library Path: " << var << endl;
	}

	m_db = QSqlDatabase::addDatabase("QMYSQL");
	m_db.setHostName("127.0.0.1");
	m_db.setPort(5306);

	QString dbname = GENORAYDB_WEB;

	m_db.setDatabaseName(dbname);
	m_db.setUserName("root");
	m_db.setPassword("GenorayTheia");
	m_db.setConnectOptions("MYSQL_OPT_RECONNECT=1");
	bool ok = m_db.open();


	m_db.exec("PRAGMA synchronous = OFF");
	m_db.exec("PRAGMA journal_mode = MEMORY");

	if (ok) {
		qDebug() << "Database open\n";
		result = true;
	}
	else
	{
		m_db.close();
		qDebug() << "Database close\n";
		result = false;
	}

	return result;
}

bool TheiaWebDB::PatientTableReset()
{
	QSqlQuery query = QSqlQuery::QSqlQuery(m_db);
	bool result = query.exec("DELETE FROM patient");
	emit UpdateStep("Processing", 20);
	return result;
}

bool TheiaWebDB::ImagedbTableReset()
{
	QSqlQuery query = QSqlQuery::QSqlQuery(m_db);
	bool result = query.exec("DELETE FROM imagedb");
	emit UpdateStep("Processing",40);
	return result;
}

bool TheiaWebDB::StmountTableReset()
{
	QSqlQuery query = QSqlQuery::QSqlQuery(m_db);
	bool result = query.exec("DELETE FROM  stmount");
	emit UpdateStep("Processing", 50);
	return result;
}

bool TheiaWebDB::tlitemTableReset()
{
	QSqlQuery query = QSqlQuery::QSqlQuery(m_db);
	bool result = query.exec("DELETE FROM  tlitem");
	emit UpdateStep("Processing", 60);
	return result;
}

bool TheiaWebDB::tlsplitterTableReset()
{
	QSqlQuery query = QSqlQuery::QSqlQuery(m_db);
	bool result = query.exec("DELETE FROM tlsplitter");
	emit UpdateStep("Processing", 70);
	return result;
}

bool TheiaWebDB::ResetPatientFolder(QString strDataPath)
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
		emit UpdateStep("100%", 90);
		return result;
}

bool TheiaWebDB::run(QString* strlog, QString strDataPath)
{
	bool result = false;
	QString failLog;
	QString resultText = "";

	result = DbOpen();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_dbopen + "\n";
	if (result == false)
		return false;

	result = PatientTableReset();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_patientTable + "\n";
	if (result == false)
		return false;

	result = ImagedbTableReset();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_imgTable + "\n";
	if (result == false)
		return false;

	result = StmountTableReset();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_stmountTable + "\n";
	if (result == false)
		return false;

	result = tlitemTableReset();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_tlitemTable + "\n";
	if (result == false)
		return false;

	result = tlsplitterTableReset();
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_tlsplitterTable + "\n";
	if (result == false)
		return false;

	result = ResetPatientFolder(strDataPath);
	*strlog += ((result == true) ? SucText : FailText) + ObjectText_PatientFolder + "\n";
	if (result == false)
		return false;

	emit UpdateStep("100%", 100);
	return result;
}


