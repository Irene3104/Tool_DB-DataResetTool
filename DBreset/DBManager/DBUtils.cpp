#include <time.h>
#include <qvector.h>
#include <QDebug>

#include "stdafx.h"
#include "DBUtils.h"
#include "Query.h"

char* CutUnicodeString(char* sz, int len)
{
	int i = 0;
	for (i = 0; i < strlen(sz);)
	{
		if (sz[i] & 0x80) // 한글 
		{
			i = i + 3;
			if (i > len)
			{
				i = i - 3;
				break;
			}
		}
		else
		{
			i++;
			if (i > len)
			{
				i--;
				break;
			}
		}
	}

	sz[i] = 0;
	return sz;
};

static int callback(void *count, int argc, char **argv, char **azColName) {
	int *c = (int *)count;
	*c = atoi(argv[0]);
	return 0;
}

CDBUtils::CDBUtils()
{
	memset(m_cpKeyword, 0x00, sizeof(char) * 64);
}
CDBUtils::CDBUtils(CServerInfo* pServerInfo)
{
	SetServerInfo(pServerInfo);	
}
CDBUtils::~CDBUtils()
{
	CloseDB();
}

int CDBUtils::SetServerInfo(CServerInfo* pServerInfo)
{
	return 1;
}

void CDBUtils::CloseDB()
{
	if (IsOpen())
	{
		int nRet = SQLITE_OK;
		nRet = m_dbSqlite.CloseConnection();
		if (SQLITE_OK != nRet)
		{
			std::string error_msg = m_dbSqlite.GetLatestError();
			nRet = sqlite3_finalize(NULL);
			if(SQLITE_OK != nRet)
				qDebug() << "[CDBUtils] CloseDB fail" << error_msg.c_str();
		}
	}
}

int CDBUtils::OpenDB_Conv(string strPath, string strName)
{
	memset(m_cpDBFileFullPath, 0x00, sizeof(char) * 256);
	strcpy(m_cpDBFileFullPath, strPath.c_str());
	strcat(m_cpDBFileFullPath, strName.c_str());

	int ret = SQLITE_ERROR;
	ret = m_dbSqlite.OpenConnection(strName, strPath);
	if (ret == SQLITE_OK)
		ret = CreateTable_Patient(false);

	if (ret != SQLITE_OK)
		qDebug() << "[CDBUtils] OpenDB fail : retcode=" << ret;

	return ret;
}

int CDBUtils::LoadDB(string strPath, string strName)
{
	int ret = SQLITE_ERROR;
	ret = m_dbSqlite.OpenConnection(strName, strPath);
	if (ret != SQLITE_OK)
		qDebug() << "[CDBUtils] LoadDB fail : retcode=" << ret;

	return ret;
}

int CDBUtils::OpenDB(string strPath, string strName)
{
	memset(m_cpDBFileFullPath, 0x00, sizeof(char) * 256);
	strcpy(m_cpDBFileFullPath, strPath.c_str());
	strcat(m_cpDBFileFullPath, strName.c_str());

	int ret = SQLITE_ERROR;
	qDebug() << "[CDBUtils] OpenDB start";
	ret = m_dbSqlite.OpenConnection(strName, strPath); 	
	qDebug() << "[CDBUtils] OpenDB end";
	if (ret == SQLITE_OK)
	{
		ret = CreateTable_Patient();
		qDebug() << "[CDBUtils] OpenDB > CreateTable_Patient" << sqlite3_errstr(ret);
	}
	else
	{
		m_dbSqlite.CloseConnection();
		qDebug() << "[CDBUtils] OpenDB fail1 : retcode=" << sqlite3_errstr(ret);
		return ret;
	}
	m_dbSqlite.CloseConnection();

	if (ret != SQLITE_OK)
		qDebug() << "[CDBUtils] OpenDB fail2 : retcode=" << sqlite3_errstr(ret);
		
	return ret;
}
bool CDBUtils::BackupDB()
{
	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] BackupDB fail";
		return false;
	}

	// 폴더 없으면 생성하고
	if (!QDir(QString(m_cpBackupFolder)).exists())
		QDir().mkpath(QString(m_cpBackupFolder));

	// 현재 날짜명으로 backup파일 복사
	QString strBackFileName = QString(m_cpBackupFolder) + "/" + "Backup_DB_" + QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".bak";

	char zFilename[100];        /* Name of file to back up to */
	strcpy(zFilename, strBackFileName.toStdString().c_str());
	
	if (m_dbSqlite.BackupDB(zFilename) == SQLITE_OK)
		return true;
	
	return false;
}

void CDBUtils::SetBackupPath(char* cpPath)
{
	memset(m_cpBackupFolder, 0x00, sizeof(char) * 256);

	strcpy(m_cpBackupFolder, cpPath);
}

bool CDBUtils::RestoreDB(QString strBak)
{
	int nRet = SQLITE_ERROR;
	nRet = m_dbSqlite.CloseConnection();
	if (nRet == SQLITE_OK)
	{
		QString strDBPath = QString(m_cpDBFileFullPath);
		if (QFile::exists(strDBPath))
		{
			if (!QFile::remove(strDBPath))
				return false;

			if (QFile::copy(strBak, strDBPath))
				return true;
		}
	}
	return false;
}

bool CDBUtils::MergeDB(QString strAddDB)
{
	int ret = SQLITE_ERROR;
	
	// step1) db attach 
	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "ATTACH '%s' AS AddDB;", strAddDB.toStdString().c_str());

	string sql = cpQuery;
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return false;
	
	// step2) insert table
	this->BeginTransaction();
	
	sql = "INSERT INTO Category SELECT * FROM AddDB.Category";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) 
	{
		this->CommitTransection();

		return false;
	}

	sql = "INSERT INTO Physician SELECT * FROM AddDB.Physician";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) 
	{
		this->CommitTransection();

		return false;
	}

	sql = "INSERT INTO patient SELECT * FROM AddDB.patient";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK)
	{
		this->CommitTransection();

		return false;
	}
	
	sql = "INSERT INTO patientDelete SELECT * FROM AddDB.patientDelete";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK)
	{
		this->CommitTransection();

		// step3) db deattach 
		sql = "DETACH AddDB";
		ret = m_dbSqlite.Excute(sql);

		return false;
	}

	sql = "INSERT INTO ImageDB SELECT * FROM AddDB.ImageDB";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK)
	{
		this->CommitTransection();

		// step3) db deattach 
		sql = "DETACH AddDB";
		ret = m_dbSqlite.Excute(sql);

		return false;
	}
	
	this->CommitTransection();
	
	// step3) db deattach 
	sql = "DETACH AddDB";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return false;

	return true;
}

int CDBUtils::CreateImgDBTable(string strPath, string strName)
{
	int ret = SQLITE_ERROR;
	ret = m_dbSqlite.OpenConnection(strName, strPath);
	if (ret != SQLITE_OK)
	{
		return ret;
	}
	
	string sql = "DROP TABLE ImageDB;";
	ret = m_dbSqlite.Excute(sql);

	sql =	"CREATE TABLE ImageDB ("\
		"[ImgId] INTEGER PRIMARY KEY AUTOINCREMENT, "\
		"[Id] NCHAR(60),"\
		"[FileName] NCHAR(100),"\
		"[ExposureDate] DATETIME,"\
		"[Modality] NCHAR(40),"\
		"[ImgFormat] NCHAR(40),"\
		"[MountIdx] INT,"\
		"[ExposureBy] NCHAR(60),"\
		"[Comment] NCHAR(100),"\
		"[ImgState] INT,"\
		"[DeleteDate] DATETIME,"\
		"[DeleteBy] NCHAR(60),"\
		"[Manufacturer] NCHAR(40),"\
		"[ManufacturerModelName] NCHAR(40),"\
		"[PixelSize] DOUBLE,"\
		"[BitPerPixel] DOUBLE,"\
		"[Width] INT,"\
		"[Height] INT,"\
		"[Voltage] DOUBLE,"\
		"[Current] DOUBLE,"\
		"[ExposureTime] DOUBLE,"\
		"[Dose] DOUBLE, "\
		"[Exposure] DOUBLE," \
		"[NumberOfFrames] INT);";

	ret = m_dbSqlite.Excute(sql);

	m_dbSqlite.CloseConnection();
	
	return ret;
}
int CDBUtils::AddImageField()
{
	string sql;
	int ret = -1;

	if (!m_dbSqlite.ColumnExists("ImageDB", "Diagnosis"))
	{
		sql = "ALTER TABLE ImageDB ADD Diagnosis NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "Reading"))
	{
		sql = "ALTER TABLE ImageDB ADD Reading NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	//////////////////////////////////////////////////////////////
	if (!m_dbSqlite.ColumnExists("ImageDB", "band"))
	{
		sql = "ALTER TABLE ImageDB ADD band INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "bit"))
	{
		sql = "ALTER TABLE ImageDB ADD bit INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "pixelRepresentation"))
	{
		sql = "ALTER TABLE ImageDB ADD pixelRepresentation INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "minVal"))
	{
		sql = "ALTER TABLE ImageDB ADD minVal INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "maxVal"))
	{
		sql = "ALTER TABLE ImageDB ADD maxVal INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "bitAlloc"))
	{
		sql = "ALTER TABLE ImageDB ADD bitAlloc INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "bitStore"))
	{
		sql = "ALTER TABLE ImageDB ADD bitStore INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "highBit"))
	{
		sql = "ALTER TABLE ImageDB ADD highBit INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "instanceNumber"))
	{
		sql = "ALTER TABLE ImageDB ADD instanceNumber INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "fileSize"))
	{
		sql = "ALTER TABLE ImageDB ADD fileSize INT;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	////////////////////////////////////////////////////////////
	if (!m_dbSqlite.ColumnExists("ImageDB", "intercept"))
	{
		sql = "ALTER TABLE ImageDB ADD intercept DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "slope"))
	{
		sql = "ALTER TABLE ImageDB ADD slope DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "SliceThickness"))
	{
		sql = "ALTER TABLE ImageDB ADD SliceThickness DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "pixelSizeY"))
	{
		sql = "ALTER TABLE ImageDB ADD pixelSizeY DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "pixelSizeX"))
	{
		sql = "ALTER TABLE ImageDB ADD pixelSizeX DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "dpiY"))
	{
		sql = "ALTER TABLE ImageDB ADD dpiY DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "dpiX"))
	{
		sql = "ALTER TABLE ImageDB ADD dpiX DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "windowCenter"))
	{
		sql = "ALTER TABLE ImageDB ADD windowCenter DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "windowWidth"))
	{
		sql = "ALTER TABLE ImageDB ADD windowWidth DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "sliceLocation"))
	{
		sql = "ALTER TABLE ImageDB ADD sliceLocation DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "fluoroscopyAreaDoseProduct"))
	{
		sql = "ALTER TABLE ImageDB ADD fluoroscopyAreaDoseProduct DOUBLE;";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	//////////////////////////////////////////////////////////////////
	if (!m_dbSqlite.ColumnExists("ImageDB", "photometricInterpretation"))
	{
		sql = "ALTER TABLE ImageDB ADD photometricInterpretation NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "patientPosition"))
	{
		sql = "ALTER TABLE ImageDB ADD patientPosition NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "imagePosition"))
	{
		sql = "ALTER TABLE ImageDB ADD imagePosition NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "imageOrientation"))
	{
		sql = "ALTER TABLE ImageDB ADD imageOrientation NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "bodyPart"))
	{
		sql = "ALTER TABLE ImageDB ADD bodyPart NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "deviceSerialNumber"))
	{
		sql = "ALTER TABLE ImageDB ADD deviceSerialNumber NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "deviceModelName"))
	{
		sql = "ALTER TABLE ImageDB ADD deviceModelName NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "imageType"))
	{
		sql = "ALTER TABLE ImageDB ADD imageType NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "studyDate"))
	{
		sql = "ALTER TABLE ImageDB ADD studyDate NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "seriesDate"))
	{
		sql = "ALTER TABLE ImageDB ADD seriesDate NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "institutionName"))
	{
		sql = "ALTER TABLE ImageDB ADD institutionName NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "institutionAddr"))
	{
		sql = "ALTER TABLE ImageDB ADD institutionAddr NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "studyInstanceUid"))
	{
		sql = "ALTER TABLE ImageDB ADD studyInstanceUid NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "seriesInstanceUid"))
	{
		sql = "ALTER TABLE ImageDB ADD seriesInstanceUid NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "sopInstanceUid"))
	{
		sql = "ALTER TABLE ImageDB ADD sopInstanceUid NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "TransferSyntaxUID"))
	{
		sql = "ALTER TABLE ImageDB ADD TransferSyntaxUID NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("ImageDB", "ImplementationClassUID"))
	{
		sql = "ALTER TABLE ImageDB ADD ImplementationClassUID NCHAR(100);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	
	return ret;
}

int CDBUtils::CreateTable_ImageDB()
{
	// hjpark@20200203: Image DB Table 영상 DB조회 기능 추가	 
	string sql = "CREATE TABLE IF NOT EXISTS ImageDB ("\
		"[ImgId] INTEGER PRIMARY KEY AUTOINCREMENT, "\
		"[Id] NCHAR(60),"\
		"[FileName] NCHAR(100),"\
		"[ExposureDate] DATETIME,"\
		"[Modality] NCHAR(40),"\
		"[ImgFormat] NCHAR(40),"\
		"[MountIdx] INT,"\
		"[ExposureBy] NCHAR(60),"\
		"[Comment] NCHAR(100),"\
		"[ImgState] INT,"\
		"[DeleteDate] DATETIME,"\
		"[DeleteBy] NCHAR(60),"\
		"[Manufacturer] NCHAR(40),"\
		"[ManufacturerModelName] NCHAR(40),"\
		"[PixelSize] DOUBLE,"\
		"[BitPerPixel] DOUBLE,"\
		"[Width] INT,"\
		"[Height] INT,"\
		"[Voltage] DOUBLE,"\
		"[Current] DOUBLE,"\
		"[ExposureTime] DOUBLE,"\
		"[Dose] DOUBLE, "\
		"[Exposure] DOUBLE," \
		"[NumberOfFrames] INT, " \
		"[Diagnosis]	NCHAR(100)	,"\
		"[Reading]	NCHAR(60)	,"\
		"[band]	INT	,"\
		"[bit]	INT	,"\
		"[pixelRepresentation]	INT	,"\
		"[minVal]	INT	,"\
		"[maxVal]	INT	,"\
		"[bitAlloc]	INT	,"\
		"[bitStore]	INT	,"\
		"[highBit]	INT	,"\
		"[instanceNumber]	INT	,"\
		"[fileSize]	INT	,"\
		"[intercept]	DOUBLE	,"\
		"[slope]	DOUBLE	,"\
		"[SliceThickness]	DOUBLE	,"\
		"[pixelSizeY]	DOUBLE	,"\
		"[pixelSizeX]	DOUBLE	,"\
		"[dpiY]	DOUBLE	,"\
		"[dpiX]	DOUBLE	,"\
		"[windowCenter]	DOUBLE	,"\
		"[windowWidth]	DOUBLE	,"\
		"[sliceLocation]	DOUBLE	,"\
		"[fluoroscopyAreaDoseProduct]	DOUBLE	,"\
		"[photometricInterpretation]	NCHAR(60)	,"\
		"[patientPosition]	NCHAR(60)	,"\
		"[imagePosition]	NCHAR(60)	,"\
		"[imageOrientation]	NCHAR(60)	,"\
		"[bodyPart]	NCHAR(60)	,"\
		"[deviceSerialNumber]	NCHAR(60)	,"\
		"[deviceModelName]	NCHAR(60)	,"\
		"[imageType]	NCHAR(60)	,"\
		"[studyDate]	NCHAR(60)	,"\
		"[seriesDate]	NCHAR(60)	,"\
		"[institutionName]	NCHAR(60)	,"\
		"[institutionAddr]	NCHAR(60)	,"\
		"[studyInstanceUid]	NCHAR(100)	,"\
		"[seriesInstanceUid]	NCHAR(100)	,"\
		"[sopInstanceUid]	NCHAR(100)	,"\
		"[TransferSyntaxUID]	NCHAR(100)	,"\
		"[ImplementationClassUID]	NCHAR(100), "\
		"[ChartNum] NCHAR(60));";

	int ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return ret;

	//hjpark@20200602: IMAGE_INFORMATION and Diagnosis, Reading add
	if (!m_dbSqlite.ColumnExists("ImageDB", "Diagnosis"))
	{
		ret = AddImageField();
		if (ret != SQLITE_OK) return ret;
	}
	
	//hjpark@20200609:ChartNum 추가 및 업데이트 
	if (!m_dbSqlite.ColumnExists("ImageDB", "ChartNum"))
	{
		// step1) add ChartNum
		if (!m_dbSqlite.ColumnExists("ImageDB", "ChartNum"))
		{
			sql = "ALTER TABLE ImageDB ADD ChartNum NCHAR(60);";
			ret = m_dbSqlite.Excute(sql);
			if (ret != SQLITE_OK) return ret;
		}

		// Step2) Update ChartNum
		ret = UpdatePatientTable((char*)("ImageDB"), (char*)("Id"), (char*)("ChartNum"));
		if (ret != SQLITE_OK) return ret;
	}

	return ret;
}

int CDBUtils::CreateTable_PhysicianCategory()
{
	string sql = "CREATE TABLE  IF NOT EXISTS Category("\
		"[Name] NCHAR(60) NOT NULL,"\
		"[Description] NCHAR(60),"\
		"[Reserved1] NCHAR(60),"\
		"[Reserved2] NCHAR(60),"\
		"PRIMARY KEY(Name)); ";
	int ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return ret;

	sql = "CREATE TABLE  IF NOT EXISTS Physician("\
		"[Name] NCHAR(60) NOT NULL,"\
		"[Tel] NCHAR(30),"\
		"[Major] NCHAR(30),"\
		"[Description] NCHAR(60),"\
		"[Reserved1] NCHAR(60),"\
		"[Reserved2] NCHAR(60),"\
		"PRIMARY KEY(Name)); ";
	ret = m_dbSqlite.Excute(sql);

	return ret;
}

int CDBUtils::UpdatePatientTable(char* cpTable, char* cpId, char* cpChart)
{
	int ret = SQLITE_ERROR;

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[256] = { 0, };
	sprintf(cpQuery, "UPDATE %s SET %s = %s WHERE 1 = 1 AND ( LENGTH(%s) = 0 OR %s is NULL);", 
		cpTable, cpChart, cpId, cpChart, cpChart, cpChart);

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			qDebug() << "[CDBUtils] UpdatePatientTable fail. " << sqlite3_errstr(ret);
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
		{
			printf("ERROR UpdateImageDBForDelete data: %s\n", m_dbSqlite.GetLatestError().c_str());
			qDebug() << "[CDBUtils] UpdatePatientTable fail2. " << sqlite3_errstr(ret);
			return ret;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForDelete fail3" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);
		
	ret = SQLITE_OK;

	return ret;
}

int CDBUtils::AddPatientField()
{
	string sql;
	int ret;
	// step1) add "ChartNum" Field
	if (!m_dbSqlite.ColumnExists("patient", "ChartNum"))
	{
		sql = "ALTER TABLE patient ADD ChartNum NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}
	if (!m_dbSqlite.ColumnExists("patientDelete", "ChartNum"))
	{
		sql = "ALTER TABLE patientDelete ADD ChartNum NCHAR(60);";
		ret = m_dbSqlite.Excute(sql);
		if (ret != SQLITE_OK) return ret;
	}

	return ret;
}

int CDBUtils::CreateTable_Version()
{
	// hjpark@20200617: DB Version 1.1
	//string sql = "DROP TABLE Version;";
	//int ret = m_dbSqlite.Excute(sql);
	string sql;
	int ret;

	// step1) Create Verison
	sql = "CREATE TABLE  IF NOT EXISTS Version ("\
		"[DBVersion] NCHAR(30) NOT NULL,"\
		"[AppVersion] NCHAR(30),"\
		"[Description] NCHAR(60),"\
		"[Reserved1] NCHAR(60),"\
		"[Reserved2] NCHAR(60),"\
		"PRIMARY KEY(DBVersion)); ";
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return ret;

	// Insert Version
	/*sqlite3		 *pDB = m_dbSqlite.GetDB();
	string strSql = "INSERT INTO Version ( DBVersion, AppVersion, Description ) VALUES ( ?,?,?);";
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			qDebug() << "[CDBUtils] Update DB Version fail1" << ret;
			return ret;
		}

		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, "1.0.0.1", -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, "1.0.0.0", -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, "ChartNum field added to patient DB", -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
		{
			qDebug() << "[CDBUtils] Update DB Version fail2" << ret;
			return SQLITE_ERROR;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] Update DB Version fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);
	*/
	
	return SQLITE_OK;
}

int CDBUtils::CreateTable_Patient(bool bCheckDB /*= true*/)
{
	//create genoray.db-patient.table
	string sql = "CREATE TABLE  IF NOT EXISTS patient("\
		"[Id] NCHAR(60) NOT NULL PRIMARY KEY,"\
		"[Name] NCHAR(100) NOT NULL,"\
		"[FirstName] NCHAR(40), "\
		"[LastName]	NCHAR(60), "\
		"[Birthdate] NCHAR(20),"\
		"[Gender] INT,"\
		"[UpdateTime] DATETIME,"\
		"[DeleteTime] DATETIME,"\
		"[Status] INT,"\
		"[PersonalNumber] NCHAR(30),"\
		"[Accession] NCHAR(40),"\
		"[OtherId] NCHAR(40),"\
		"[Category] NCHAR(40),"\
		"[Physician] NCHAR(40),"\
		"[Description] NCHAR(100),"\
		"[Email] NCHAR(30),"\
		"[Address] NCHAR(100),"\
		"[ZipCode] NCHAR(30),"\
		"[Phone] NCHAR(30),"\
		"[MobilePhone] NCHAR(30),"\
		"[ProfileImage] NCHAR(60),"\
		"[PX] INT,"\
		"[CX] INT,"\
		"[OX] INT,"\
		"[OV] INT,"\
		"[DC] INT,"\
		"[CT] INT,"\
		"[PX_UpdateTime] DATETIME,"\
		"[CX_UpdateTime] DATETIME,"\
		"[OX_UpdateTime] DATETIME,"\
		"[OV_UpdateTime] DATETIME,"\
		"[DC_UpdateTime] DATETIME,"\
		"[CT_UpdateTime] DATETIME,"\
		"[Invisible] INT,"\
		"[Reserve1] NCHAR(60),"\
		"[Reserve2] NCHAR(60),"\
		"[ChartNum] NCHAR(60)); ";

	int ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) 	return ret;
		
	sql = "CREATE TABLE  IF NOT EXISTS patientDelete("\
		"[Id] NCHAR(60)  NOT NULL PRIMARY KEY,"\
		"[Name] NCHAR(60) NOT NULL,"\
		"[FirstName] NCHAR(40), "\
		"[LastName]	NCHAR(60), "\
		"[Birthdate] NCHAR(20),"\
		"[Gender] INT,"\
		"[UpdateTime] DATETIME,"\
		"[DeleteTime] DATETIME,"\
		"[Status] INT,"\
		"[PersonalNumber] NCHAR(30),"\
		"[Accession] NCHAR(40),"\
		"[OtherId] NCHAR(40),"\
		"[Category] NCHAR(40),"\
		"[Physician] NCHAR(40),"\
		"[Description] NCHAR(100),"\
		"[Email] NCHAR(30),"\
		"[Address] NCHAR(100),"\
		"[ZipCode] NCHAR(30),"\
		"[Phone] NCHAR(30),"\
		"[MobilePhone] NCHAR(30),"\
		"[ProfileImage] NCHAR(60),"\
		"[PX] INT,"\
		"[CX] INT,"\
		"[OX] INT,"\
		"[OV] INT,"\
		"[DC] INT,"\
		"[CT] INT,"\
		"[PX_UpdateTime] DATETIME,"\
		"[CX_UpdateTime] DATETIME,"\
		"[OX_UpdateTime] DATETIME,"\
		"[OV_UpdateTime] DATETIME,"\
		"[DC_UpdateTime] DATETIME,"\
		"[CT_UpdateTime] DATETIME,"\
		"[Invisible] INT,"\
		"[Reserve1] NCHAR(60),"\
		"[Reserve2] NCHAR(60),"\
		"[ChartNum] NCHAR(60)); ";
	
	ret = m_dbSqlite.Excute(sql);
	if (ret != SQLITE_OK) return ret;

	//hjpark@20200609:ChartNum 추가 및 업데이트 
	if (!m_dbSqlite.ColumnExists("patient", "ChartNum"))
	{
		ret = AddPatientField();
		if (ret != SQLITE_OK) return ret;
	}

	//hjpark@20200610:Update ChartNum
	ret = UpdatePatientTable((char*)("patient"), (char*)("Id"), (char*)("ChartNum"));
	if (ret != SQLITE_OK) return ret;

	ret = UpdatePatientTable((char*)("patientDelete"), (char*)("Id"), (char*)("ChartNum"));
	if (ret != SQLITE_OK) return ret;

	ret = CreateTable_PhysicianCategory();
	if (ret != SQLITE_OK) return ret;

	qDebug() << "[CDBUtils] CreateTable_ImageDB";
			
	ret = CreateTable_ImageDB();
	if (ret != SQLITE_OK) return ret;

	ret = CreateTable_Version();
	if (ret != SQLITE_OK) return ret;

	//hjpark@20210321
	/*
	char cpQuery[1024] = { 0, };
	strcpy(cpQuery, "UPDATE Patient SET PX_UpdateTime = UpdateTime WHERE 1 = 1 AND ( LENGTH(PX_UpdateTime) = 0 OR PX_UpdateTime is NULL OR PX_UpdateTime = 0 );");
	if (this->Execute(cpQuery) != SQLITE_OK)
	{
		return ret;
	}
	memset(cpQuery, 0x00, 1024);
	strcpy(cpQuery, "UPDATE PatientDelete SET PX_UpdateTime = UpdateTime WHERE 1 = 1 AND ( LENGTH(PX_UpdateTime) = 0 OR PX_UpdateTime is NULL OR PX_UpdateTime = 0 );");
	if (this->Execute(cpQuery) != SQLITE_OK)
	{
		return ret;
	}
	*/
	
	if (bCheckDB)
	{
		CloseDB();
	}
			
	return ret;
}

int  CDBUtils::AddPatient(PATIENT_INFO* pPatient, bool bCheckValid /*= true*/)
{
	int nRet = SQLITE_ERROR;

	if (bCheckValid)
	{
		// DB 연결 끊어질 경우 대비
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] AddPatient fail1" << sqlite3_errstr(nRet);
			return nRet;
		}				
	}

	string strSql = "INSERT INTO patient ( Id, Name, FirstName, LastName, Birthdate, Gender, UpdateTime, Status, \
                                           PersonalNumber, Accession, OtherId, Category, Physician, \
										   Description, Email, Address, ZipCode,\
										   Phone, MobilePhone, ProfileImage, Invisible, Reserve1, Reserve2, ChartNum, PX_UpdateTime )\
								  VALUES ( ?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,? );";

	sqlite3* pDB = m_dbSqlite.GetDB();
	if (pDB == NULL)
	{
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] AddPatient fail";
			return nRet;
		}
		pDB = m_dbSqlite.GetDB();
	}
	sqlite3_stmt* pStmt = m_dbSqlite.GetStatement();

	try {		
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);

		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->FirstName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->LastName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Status);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->OtherId, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Category, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Physician, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Description, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Email, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Address, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ZipCode, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Phone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->MobilePhone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ImageFileName, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->nInvisible);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve1, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve2, -1, SQLITE_STATIC);
		//hjpark@20200609:ChartNum Add
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);
		//hjpark@20200811:최근 영상 획득 시간으로 사용, 신규 환자 추가시에는 최근 업데이트 시간과 동일함.
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE) {
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
			qDebug() << "[CDBUtils] AddPatient ERROR inserting data" << m_dbSqlite.GetLatestError().c_str() << sqlite3_errstr(nRet);
		}		
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] AddPatient fail" << error_msg.c_str();		
	}

	sqlite3_finalize(pStmt);
			
	if (bCheckValid)
	{		
		CloseDB();
	}

	return nRet;
}

// 모든 영상DB Table의 ChartNum 변경 
int CDBUtils::UpdateImageDBForChartNum(QString strId, QString strChartNum)
{
	int ret = SQLITE_ERROR;
	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] UpdateImageDBForChartNum fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "UPDATE ImageDB SET ChartNum = '%s' WHERE Id = '%s';", strChartNum.toStdString().c_str(), strId.toStdString().c_str());

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			sqlite3_finalize(pStmt);
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR UpdateImageDBForChartNum : %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForChartNum fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

// Image Comment 영상DBSync
int CDBUtils::UpdateImageDBForComment(IMG_DB_INFO* pInfo) 
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] UpdateImageDBForComment fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[4096] = { 0, };
	sprintf(cpQuery, "UPDATE ImageDB SET Comment = '%s', Diagnosis = '%s', Reading = '%s' WHERE Id = '%s' AND FileName = '%s';", pInfo->Comment, pInfo->Diagnosis, pInfo->Reading, pInfo->Id, pInfo->FileName);

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR UpdateImageDBForComment : %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForComment fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

int CDBUtils::LastUpdateImageDBForReading(QVector<IMG_DB_READING> pInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] LastUpdateImageDBForReading fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	// 기존 템프 DB삭제 
	string sql = "DROP TABLE TempDB;";
	ret = m_dbSqlite.Excute(sql);
	
	// Create Temp Table
	sql = "CREATE TABLE TempDB ("\
		"[ImgId] INTEGER PRIMARY KEY AUTOINCREMENT, "\
		"[Id] NCHAR(60),"\
		"[FileName] NCHAR(100),"\
		"[Reading] NCHAR(60));";
	ret = m_dbSqlite.Excute(sql);

	char cpQuery[60000] = { 0, };
	strcpy(cpQuery, "INSERT INTO TempDB ('Id', 'FileName', 'Reading') VALUES ");
	// Insert Temp Table
	for (int ii = 0; ii < pInfo.size(); ii++)
	{
		char cpTemp[1024] = { 0, };
		if (ii == pInfo.size() - 1)
			sprintf(cpTemp, "('%s','%s','%s');", pInfo[ii].Id, pInfo[ii].FileName, pInfo[ii].Reading);
		else 
			sprintf(cpTemp, "('%s','%s','%s'),", pInfo[ii].Id, pInfo[ii].FileName, pInfo[ii].Reading);	

		strcat(cpQuery, cpTemp);
	}
	sql = cpQuery;
	ret = m_dbSqlite.Excute(sql);

	// Update By ModifyTable
	memset(cpQuery, 0x00, sizeof(char) * 60000);
	sprintf(cpQuery, "UPDATE ImageDB SET Reading = (SELECT Reading FROM TempDB WHERE ImageDB.FileName = TempDB.FileName ) WHERE FileName IN (SELECT FileName FROM TempDB);");

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR LastUpdateImageDBForReading : %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] LastUpdateImageDBForReading fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

// Image Comment 영상DBSync
int CDBUtils::UpdateImageDBForReading(IMG_DB_INFO* pInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] UpdateImageDBForReading fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[4096] = { 0, };
	sprintf(cpQuery, "UPDATE ImageDB SET Reading = '%s' WHERE Id = '%s' AND FileName = '%s';", pInfo->Reading, pInfo->Id, pInfo->FileName);

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR UpdateImageDBForReading : %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForReading fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}


int CDBUtils::UpdateImageDBForRecovery(IMG_DB_INFO* pInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] UpdateImageDBForRecovery fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "UPDATE ImageDB SET ImgState = 0 WHERE Id = '%s' AND FileName = '%s';",
		pInfo->Id, pInfo->FileName);

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR UpdateImageDBForRecovery data: %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForRecovery fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

// 영상 Inactive 상태 DB 업데이트
int CDBUtils::UpdateImageDBForDelete(IMG_DB_INFO* pInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] UpdateImageDBForDelete fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "UPDATE ImageDB SET ImgState = 1, DeleteDate = %d, DeleteBy = '%s' WHERE Id = '%s' AND FileName = '%s';", 
		pInfo->DeleteTime, pInfo->DeleteBy, pInfo->Id, pInfo->FileName) ;

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return ret;
		}
				
		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR UpdateImageDBForDelete data: %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageDBForDelete fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}


// 실제 영상 DB 삭제 (Patient ID, FileName으로 찾기)
int CDBUtils::DeleteImageDB(IMG_DB_INFO* pInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] DeleteImageDB fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from ImageDB where Id = ? AND FileName = ?";

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pInfo->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, 2, pInfo->FileName, -1, SQLITE_STATIC);
		
		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR DeleteImageDB data: %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeleteImageDB fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();
	
	return ret;
}

int CDBUtils::DeleteImageDB(char* cpId, bool bCheckDB /*=true*/)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (bCheckDB)
	{
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] DeleteImageDB fail";
			return ret;
		}		
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from ImageDB where Id = ?";

	if (bCheckDB)
		this->BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, cpId, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR DeleteImageDB data: %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeleteImageDB fail" << error_msg.c_str();
	}

	if (bCheckDB)
		this->CommitTransection();

	sqlite3_finalize(pStmt);
	
	if (bCheckDB)
	{
		CloseDB();
	}

	return ret;
}

int CDBUtils::EditImageInfo(IMG_DB_INFO* pImgInfo)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditImageInfo fail";
		return ret;
	}

	string strSql = "UPDATE ImageDB SET ExposureDate = ?,  Modality = ?, ImgFormat = ?, MountIdx = ?,  ExposureBy = ?,\
							 Comment = ?, ImgState = ?, DeleteDate = ?, DeleteBy = ?,  Manufacturer = ?, ManufacturerModelName = ?, PixelSize = ?, BitPerPixel = ?, \
							 Width = ?, Height = ?, Voltage = ?, Current = ?, ExposureTime = ?, Dose = ?, Exposure = ?, NumberOfFrames = ?, Diagnosis = ? , Reading = ? \
							 WHERE Id = ? And FileName = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ExposureDate);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Modality, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ImgFormat, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->MountIdx);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ExposureBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Comment, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ImgState);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->DeleteTime);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->DeleteBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Manufacturer, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ManufacturerModelName, -1, SQLITE_STATIC);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->PixelSize);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->BitPerPixel);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Width);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Height);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Voltage);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Current);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->ExposureTime);

		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Dose);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Exposure);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->NumberOfFrames);

		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Diagnosis, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Reading, -1, SQLITE_STATIC);

		// 검색 조건 
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Id, - 1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->FileName, -1, SQLITE_STATIC);
				
		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());	
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditImageInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

int CDBUtils::LastUpdateAddImageDB(QVector<IMG_DB_INFO> pInfos, bool bAcq /*=true*/)
{
	int nRet = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > AddImageInfo fail";
		return nRet;
	}

	sqlite3		 * pDB = m_dbSqlite.GetDB();
	sqlite3_stmt * pStmt = m_dbSqlite.GetStatement();

	string strSql = "INSERT INTO ImageDB VALUES (NULL, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, \
													   ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?); ";
	

	BeginTransaction();

	int nAcqTime = 0;
	for (int ii = 0; ii < pInfos.count(); ii++)
	{
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return nRet;
		}

		int nIdx = 1;
		if (nAcqTime < pInfos[ii].ExposureDate) nAcqTime = pInfos[ii].ExposureDate;

		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].FileName, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].ExposureDate);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Modality, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].ImgFormat, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].MountIdx);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].ExposureBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Comment, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].ImgState);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].DeleteTime);

		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].DeleteBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Manufacturer, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].ManufacturerModelName, -1, SQLITE_STATIC);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].PixelSize);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].BitPerPixel);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].Width);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].Height);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].Voltage);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].Current);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].ExposureTime);

		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].Dose);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].Exposure);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].NumberOfFrames);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Diagnosis, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].Reading, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].band);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].bit);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].pixelRepresentation);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].min);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].max);

		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].bitAlloc);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].bitStore);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].highBit);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].instanceNumber);
		sqlite3_bind_int(pStmt, nIdx++, pInfos[ii].fileSize);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].intercept);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].slope);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].SliceThickness);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].pixelSizeY);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].pixelSizeX);

		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].dpiY);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].dpiX);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].windowCenter);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].windowWidth);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].sliceLocation);
		sqlite3_bind_double(pStmt, nIdx++, pInfos[ii].fluoroscopyAreaDoseProduct);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].photometricInterpretation, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].patientPosition, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].imagePosition, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].imageOrientation, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].bodyPart, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].deviceSerialNumber, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].deviceModelName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].imageType, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].studyDate, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].seriesDate, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].institutionName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].institutionAddr, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].studyInstanceUid, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].seriesInstanceUid, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].sopInstanceUid, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].TransferSyntaxUID, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].ImplementationClassUID, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pInfos[ii].cpChartNum, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE) {
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
		}
	}

	CommitTransection();

	sqlite3_finalize(pStmt);

	// 환자의 최종 촬영시간 갱신
	char cpQuery[2048] = { 0, };
	sprintf(cpQuery, "UPDATE patient SET PX_UpdateTime = %d, UpdateTime = %d WHERE Id = '%s'; ",  nAcqTime, nAcqTime, pInfos[0].Id);
	string sql = cpQuery;
	nRet = m_dbSqlite.Excute(sql);

	CloseDB();

	return nRet;
}

int  CDBUtils::AddImageInfo(IMG_DB_INFO* pImgInfo)
{
	int nRet = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > AddImageInfo fail";
		return nRet;
	}
	
	string strSql = "INSERT INTO ImageDB VALUES (NULL, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, \
													   ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?); ";

	sqlite3		 * pDB = m_dbSqlite.GetDB();
	sqlite3_stmt * pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->FileName, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ExposureDate);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Modality, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ImgFormat, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->MountIdx);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ExposureBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Comment, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ImgState);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->DeleteTime);

		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->DeleteBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Manufacturer, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ManufacturerModelName, -1, SQLITE_STATIC);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->PixelSize);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->BitPerPixel);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Width);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Height);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Voltage);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Current);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->ExposureTime);

		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Dose);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Exposure);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->NumberOfFrames);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Diagnosis, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Reading, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->band);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bit);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->pixelRepresentation);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->min);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->max);

		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bitAlloc);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bitStore);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->highBit);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->instanceNumber);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->fileSize);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->intercept);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->slope);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->SliceThickness);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->pixelSizeY);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->pixelSizeX);

		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->dpiY);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->dpiX);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->windowCenter);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->windowWidth);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->sliceLocation);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->fluoroscopyAreaDoseProduct);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->photometricInterpretation, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->patientPosition, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->imagePosition, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->imageOrientation, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->bodyPart, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->deviceSerialNumber, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->deviceModelName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->imageType, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->studyDate, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->seriesDate, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->institutionName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->institutionAddr, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->studyInstanceUid, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->seriesInstanceUid, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->sopInstanceUid, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->TransferSyntaxUID, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ImplementationClassUID, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->cpChartNum, -1, SQLITE_STATIC);
				
		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE) {
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
		}
	} catch (std::exception& e) 	{
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] AddImageInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);	

	CloseDB();

	return nRet;
}

// Conversion 전용 
int  CDBUtils::AddImageInfo_Conv(IMG_DB_INFO* pImgInfo)
{
	int nRet = SQLITE_ERROR;

	string strSql = "INSERT INTO ImageDB ( Id, FileName, ExposureDate, Modality, ImgFormat, MountIdx, ExposureBy, Comment, ImgState, DeleteDate, DeleteBy, Manufacturer, ManufacturerModelName, PixelSize, BitPerPixel, Width, Height, Voltage, Current, ExposureTime,\
										   Dose, Exposure, NumberOfFrames, Diagnosis, Reading, band, bit, pixelRepresentation, minVal, maxVal, bitAlloc, bitStore, highBit, instanceNumber, fileSize, intercept, slope, SliceThickness, pixelSizeY, pixelSizeX \
										  ) VALUES (?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?, ?,?,?,?,?,?,?,?,?,?); ";

	sqlite3		 * pDB = m_dbSqlite.GetDB();
	sqlite3_stmt * pStmt = m_dbSqlite.GetStatement();
	
	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
		}

		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->FileName, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ExposureDate);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Modality, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ImgFormat, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->MountIdx);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ExposureBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Comment, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->ImgState);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->DeleteTime);

		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->DeleteBy, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Manufacturer, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->ManufacturerModelName, -1, SQLITE_STATIC);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->PixelSize);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->BitPerPixel);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Width);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->Height);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Voltage);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Current);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->ExposureTime);

		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Dose);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->Exposure);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->NumberOfFrames);

		//hjpark@20200604: Add
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Diagnosis, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pImgInfo->Reading, -1, SQLITE_STATIC);

		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->band);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bit);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->pixelRepresentation);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->min);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->max);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bitAlloc);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->bitStore);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->highBit);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->instanceNumber);
		sqlite3_bind_int(pStmt, nIdx++, pImgInfo->fileSize);

		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->intercept);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->slope);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->SliceThickness);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->pixelSizeY);
		sqlite3_bind_double(pStmt, nIdx++, pImgInfo->pixelSizeX);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE) {
			string str;
			str = ("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] AddImageInfo fail" << error_msg.c_str();		
	}

	sqlite3_finalize(pStmt);
	
	return nRet;
}

bool CDBUtils::IsCheckValidDBConnection()
{
	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] SearchImgage fail";
		return false;
	}

	return true;
}

// Get Image Comment : Paient's ID, Image File Name == > Comment
int CDBUtils::SearchImageInfo(QString strId, QString strFileName, IMG_DB_INFO* pInfo, bool bCheckConnect /*=true*/)
{
	int rc = SQLITE_ERROR;
	// DB 연결 끊어질 경우 대비
	if (bCheckConnect)
	{
		// hjpark@20210311:Read Only Flag
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] SearchImgage fail";
			return rc;
		}
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "select rowid, * from ImageDB WHERE Id='%s' AND FileName ='%s';",
		strId.toUtf8().toStdString().c_str(), strFileName.toUtf8().toStdString().c_str());

	try {
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return rc;
		}
		
		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{			
			int nIdx = 1;
			char cpTemp[1024] = { 0, };

			sqlite3_column_int(pStmt, ROWID);

			pInfo->ImgId = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(pInfo->Id, "%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(pInfo->FileName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			pInfo->ExposureDate = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(pInfo->Modality, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(pInfo->ImgFormat, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			pInfo->MountIdx = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(pInfo->ExposureBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100)
				strcpy(pInfo->Comment, CutUnicodeString(cpTemp, 99));
			else 
				strcpy(pInfo->Comment, cpTemp);

			pInfo->ImgState = sqlite3_column_int(pStmt, nIdx++);
			pInfo->DeleteTime = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(pInfo->DeleteBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(pInfo->Manufacturer, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(pInfo->ManufacturerModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			pInfo->PixelSize = sqlite3_column_double(pStmt, nIdx++);
			pInfo->BitPerPixel = sqlite3_column_double(pStmt, nIdx++);
			pInfo->Width = sqlite3_column_int(pStmt, nIdx++);
			pInfo->Height = sqlite3_column_int(pStmt, nIdx++);
			pInfo->Voltage = sqlite3_column_double(pStmt, nIdx++);
			pInfo->Current = sqlite3_column_double(pStmt, nIdx++);
			pInfo->ExposureTime = sqlite3_column_double(pStmt, nIdx++);
			pInfo->Dose = sqlite3_column_double(pStmt, nIdx++);
			pInfo->Exposure = sqlite3_column_double(pStmt, nIdx++);
			pInfo->NumberOfFrames = sqlite3_column_int(pStmt, nIdx++);

			//hjpark@20200603:add
			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100) 	strcpy(pInfo->Diagnosis, CutUnicodeString(cpTemp, 99));
			else						strcpy(pInfo->Diagnosis, cpTemp);

			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 60)	strcpy(pInfo->Reading, CutUnicodeString(cpTemp, 59));
			else						strcpy(pInfo->Reading, cpTemp);

			//hjpark@20200605: Comment만 가져갈때는 다른 정보 Get 하지 말자.
			if (bCheckConnect)
			{
				pInfo->band = sqlite3_column_int(pStmt, nIdx++);
				pInfo->bit = sqlite3_column_int(pStmt, nIdx++);
				pInfo->pixelRepresentation = sqlite3_column_int(pStmt, nIdx++);
				pInfo->min = sqlite3_column_int(pStmt, nIdx++);
				pInfo->max = sqlite3_column_int(pStmt, nIdx++);
				pInfo->bitAlloc = sqlite3_column_int(pStmt, nIdx++);
				pInfo->bitStore = sqlite3_column_int(pStmt, nIdx++);
				pInfo->highBit = sqlite3_column_int(pStmt, nIdx++);
				pInfo->instanceNumber = sqlite3_column_int(pStmt, nIdx++);
				pInfo->fileSize = sqlite3_column_int(pStmt, nIdx++);

				pInfo->intercept = sqlite3_column_double(pStmt, nIdx++);
				pInfo->slope = sqlite3_column_double(pStmt, nIdx++);
				pInfo->SliceThickness = sqlite3_column_double(pStmt, nIdx++);
				pInfo->pixelSizeY = sqlite3_column_double(pStmt, nIdx++);
				pInfo->pixelSizeX = sqlite3_column_double(pStmt, nIdx++);
				pInfo->dpiY = sqlite3_column_double(pStmt, nIdx++);
				pInfo->dpiX = sqlite3_column_double(pStmt, nIdx++);
				pInfo->windowCenter = sqlite3_column_double(pStmt, nIdx++);
				pInfo->windowWidth = sqlite3_column_double(pStmt, nIdx++);
				pInfo->sliceLocation = sqlite3_column_double(pStmt, nIdx++);
				pInfo->fluoroscopyAreaDoseProduct = sqlite3_column_double(pStmt, nIdx++);

				sprintf_s(pInfo->photometricInterpretation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->patientPosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));

				//hjpark@20200907:Size Overflow
				//sprintf_s(pInfo->imagePosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				//sprintf_s(pInfo->imageOrientation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				memset(cpTemp, 0x00, sizeof(char) * 1024);
				sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				if (strcmp(cpTemp, "(null)") != 0)
				{
					if (strlen(cpTemp) >= 60)
						strcpy(pInfo->imagePosition, CutUnicodeString(cpTemp, 59));
					else
						strcpy(pInfo->imagePosition, cpTemp);
				}
				memset(cpTemp, 0x00, sizeof(char) * 1024);
				sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				if (strcmp(cpTemp, "(null)") != 0)
				{
					if (strlen(cpTemp) >= 60)
						strcpy(pInfo->imageOrientation, CutUnicodeString(cpTemp, 59));
					else
						strcpy(pInfo->imageOrientation, cpTemp);
				}

				sprintf_s(pInfo->bodyPart, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->deviceSerialNumber, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->deviceModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->imageType, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->studyDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->seriesDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));

				sprintf_s(pInfo->institutionName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->institutionAddr, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->studyInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->seriesInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->sopInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->TransferSyntaxUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				sprintf_s(pInfo->ImplementationClassUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			}

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchImgage fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	if (bCheckConnect)
		CloseDB();

	return rc;
}

QMap<QString, QString> CDBUtils::LoadImageComments(QString strId)
{
	QMap<QString, QString> resultList;
	
	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] LoadImageComments fail";
		return resultList;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "select rowid, FileName, Comment from ImageDB WHERE Id='%s';", strId.toUtf8().toStdString().c_str());

	QString strFileName = "";
	QString strComments = "";

	try {
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return resultList;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			int nIdx = 1;
			char cpTemp[1024] = { 0, };

			sqlite3_column_int(pStmt, ROWID);						
			// FileName 
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			strFileName = cpTemp;
			// Comment 
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			strComments = cpTemp;

			rc = sqlite3_step(pStmt);

			resultList.insert(strFileName, strComments);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] LoadImageComments fail" << error_msg.c_str();
	}
	sqlite3_finalize(pStmt);
		
	CloseDB();

	return resultList;
}


QVector<IMG_DB_INFO> CDBUtils::SearchImage(CQuery* pQuery, bool bCheckDB /*= true*/, bool bDelete /*= false*/)
{
	QVector<IMG_DB_INFO> resultList;

	if (bCheckDB)
	{
		// hjpark@20210311:Read Only Flag
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] SearchImgage fail";
			return resultList;
		}
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string str = pQuery->GetQueryString();

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return resultList;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			IMG_DB_INFO info;
			memset(&info, 0x00, sizeof(IMG_DB_INFO));
			
			int nIdx = 1;

			char cpTemp[1024] = { 0, };

			sqlite3_column_int(pStmt, ROWID);
			
			info.ImgId = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.FileName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.ExposureDate = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Modality, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImgFormat, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.MountIdx = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.ExposureBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100)
				strcpy(info.Comment, CutUnicodeString(cpTemp, 99));
			else 
				strcpy(info.Comment, cpTemp);
			
			info.ImgState = sqlite3_column_int(pStmt, nIdx++);
			info.DeleteTime = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.DeleteBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.Manufacturer, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ManufacturerModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.PixelSize = sqlite3_column_double(pStmt, nIdx++);
			info.BitPerPixel = sqlite3_column_double(pStmt, nIdx++);
			info.Width = sqlite3_column_int(pStmt, nIdx++);
			info.Height= sqlite3_column_int(pStmt, nIdx++);
			info.Voltage = sqlite3_column_double(pStmt, nIdx++);
			info.Current = sqlite3_column_double(pStmt, nIdx++);
			info.ExposureTime = sqlite3_column_double(pStmt, nIdx++);
			info.Dose= sqlite3_column_double(pStmt, nIdx++);
			info.Exposure = sqlite3_column_double(pStmt, nIdx++);
			info.NumberOfFrames = sqlite3_column_int(pStmt, nIdx++);

			//hjpark@20200603:add
			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 100)
					strcpy(info.Diagnosis, CutUnicodeString(cpTemp, 99));
				else
					strcpy(info.Diagnosis, cpTemp);
			}

			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) < 1 || strcmp(cpTemp, "Y") != 0)
					strcpy(info.Reading, "N");
				else 
					strcpy(info.Reading, cpTemp);
			}
			else
			{
				strcpy(info.Reading, "N");
			}

			info.band = sqlite3_column_int(pStmt, nIdx++);
			info.bit = sqlite3_column_int(pStmt, nIdx++);
			info.pixelRepresentation = sqlite3_column_int(pStmt, nIdx++);
			info.min = sqlite3_column_int(pStmt, nIdx++);
			info.max = sqlite3_column_int(pStmt, nIdx++);
			info.bitAlloc = sqlite3_column_int(pStmt, nIdx++);
			info.bitStore = sqlite3_column_int(pStmt, nIdx++);
			info.highBit = sqlite3_column_int(pStmt, nIdx++);
			info.instanceNumber = sqlite3_column_int(pStmt, nIdx++);
			info.fileSize = sqlite3_column_int(pStmt, nIdx++);

			info.intercept = sqlite3_column_double(pStmt, nIdx++);
			info.slope = sqlite3_column_double(pStmt, nIdx++);
			info.SliceThickness = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeY = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeX = sqlite3_column_double(pStmt, nIdx++);
			info.dpiY = sqlite3_column_double(pStmt, nIdx++);
			info.dpiX = sqlite3_column_double(pStmt, nIdx++);
			info.windowCenter = sqlite3_column_double(pStmt, nIdx++);
			info.windowWidth = sqlite3_column_double(pStmt, nIdx++);
			info.sliceLocation = sqlite3_column_double(pStmt, nIdx++);
			info.fluoroscopyAreaDoseProduct = sqlite3_column_double(pStmt, nIdx++);

			sprintf_s(info.photometricInterpretation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.patientPosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			//hjpark@20200907:overlflow
			//sprintf_s(info.imagePosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			//sprintf_s(info.imageOrientation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imagePosition, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imagePosition, cpTemp);
			}
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imageOrientation, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imageOrientation, cpTemp);
			}
			sprintf_s(info.bodyPart, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceSerialNumber, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.imageType, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.institutionName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.institutionAddr, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.sopInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.TransferSyntaxUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImplementationClassUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
				
			//hjpark@20200904: 삭제된 영상은 보이지 않기
			if(bDelete && info.ImgState == 1)
				resultList.push_back(info);
			else if(!bDelete && info.ImgState == 0)
				resultList.push_back(info);

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchImgage fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	if (bCheckDB)
	{
		CloseDB();
	}

	return resultList;
}

QVector<IMG_DB_INFO> CDBUtils::SearchImageJoinQuery(CQuery* pQuery, int& nRetCode, bool bDelete /*= false*/)
{
	QVector<IMG_DB_INFO> resultList;

	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] SearchImgage fail";
		nRetCode = SQLITE_BUSY;
		return resultList;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string str = pQuery->GetQueryString();

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			nRetCode = SQLITE_BUSY;
			return resultList;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			IMG_DB_INFO info;
			memset(&info, 0x00, sizeof(IMG_DB_INFO));

			int nIdx = 0;

			char cpTemp[1024] = { 0, };

			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100)
				strcpy(info.cpChartNum, CutUnicodeString(cpTemp, 100));
			else
				strcpy(info.cpChartNum, cpTemp);
			//sprintf_s(info.cpChartNum, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.m_cpPatientName, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			info.ImgId = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.FileName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.ExposureDate = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Modality, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImgFormat, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.MountIdx = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.ExposureBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100)
				strcpy(info.Comment, CutUnicodeString(cpTemp, 99));
			else
				strcpy(info.Comment, cpTemp);

			info.ImgState = sqlite3_column_int(pStmt, nIdx++);
			info.DeleteTime = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.DeleteBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.Manufacturer, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ManufacturerModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.PixelSize = sqlite3_column_double(pStmt, nIdx++);
			info.BitPerPixel = sqlite3_column_double(pStmt, nIdx++);
			info.Width = sqlite3_column_int(pStmt, nIdx++);
			info.Height = sqlite3_column_int(pStmt, nIdx++);
			info.Voltage = sqlite3_column_double(pStmt, nIdx++);
			info.Current = sqlite3_column_double(pStmt, nIdx++);
			info.ExposureTime = sqlite3_column_double(pStmt, nIdx++);
			info.Dose = sqlite3_column_double(pStmt, nIdx++);
			info.Exposure = sqlite3_column_double(pStmt, nIdx++);
			info.NumberOfFrames = sqlite3_column_int(pStmt, nIdx++);

			//hjpark@20200603:add
			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 100)
					strcpy(info.Diagnosis, CutUnicodeString(cpTemp, 99));
				else
					strcpy(info.Diagnosis, cpTemp);
			}

			memset(cpTemp, 0x00, sizeof(char) * 256);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) < 1 || strcmp(cpTemp, "Y") != 0)
					strcpy(info.Reading, "N");
				else
					strcpy(info.Reading, cpTemp);
			}
			else
			{
				strcpy(info.Reading, "N");
			}

			info.band = sqlite3_column_int(pStmt, nIdx++);
			info.bit = sqlite3_column_int(pStmt, nIdx++);
			info.pixelRepresentation = sqlite3_column_int(pStmt, nIdx++);
			info.min = sqlite3_column_int(pStmt, nIdx++);
			info.max = sqlite3_column_int(pStmt, nIdx++);
			info.bitAlloc = sqlite3_column_int(pStmt, nIdx++);
			info.bitStore = sqlite3_column_int(pStmt, nIdx++);
			info.highBit = sqlite3_column_int(pStmt, nIdx++);
			info.instanceNumber = sqlite3_column_int(pStmt, nIdx++);
			info.fileSize = sqlite3_column_int(pStmt, nIdx++);

			info.intercept = sqlite3_column_double(pStmt, nIdx++);
			info.slope = sqlite3_column_double(pStmt, nIdx++);
			info.SliceThickness = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeY = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeX = sqlite3_column_double(pStmt, nIdx++);
			info.dpiY = sqlite3_column_double(pStmt, nIdx++);
			info.dpiX = sqlite3_column_double(pStmt, nIdx++);
			info.windowCenter = sqlite3_column_double(pStmt, nIdx++);
			info.windowWidth = sqlite3_column_double(pStmt, nIdx++);
			info.sliceLocation = sqlite3_column_double(pStmt, nIdx++);
			info.fluoroscopyAreaDoseProduct = sqlite3_column_double(pStmt, nIdx++);

			sprintf_s(info.photometricInterpretation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.patientPosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			//hjpark@20200907:overlflow
			//sprintf_s(info.imagePosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			//sprintf_s(info.imageOrientation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imagePosition, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imagePosition, cpTemp);
			}
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imageOrientation, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imageOrientation, cpTemp);
			}
			sprintf_s(info.bodyPart, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceSerialNumber, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.imageType, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.institutionName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.institutionAddr, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.sopInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.TransferSyntaxUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImplementationClassUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));

		
			//hjpark@20200904: 삭제된 영상은 보이지 않기
			if (bDelete && info.ImgState == 1)
				resultList.push_back(info);
			else if (!bDelete && info.ImgState == 0)
				resultList.push_back(info);

			rc = sqlite3_step(pStmt);
		}
	}
	catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchImgage fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	nRetCode = SQLITE_DONE;

	return resultList;
}

int CDBUtils::GetLastUpdateTime()
{
	int nLastTime = QDateTime::currentDateTime().toTime_t();
	
	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] GetLastUpdateTime fail ";
		return nLastTime;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;
	
	string str = "SELECT rowid, * FROM patient ORDER BY UpdateTime DESC";

	try
	{
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return nLastTime;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			PATIENT_INFO info;
			info.RowId = sqlite3_column_int(pStmt, ROWID);

			char cpMaxBuff[256] = { 0, };
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.Name, u8"%s", sqlite3_column_text(pStmt, NAME));
			sprintf_s(info.FirstName, u8"%s", sqlite3_column_text(pStmt, FIRSTNAME));
			sprintf_s(info.LastName, u8"%s", sqlite3_column_text(pStmt, LASTNAME));
			sprintf_s(info.Birthdate, "%s", sqlite3_column_text(pStmt, BIRTHDATE));

			info.Gender = sqlite3_column_int(pStmt, GENDER);
			info.UpdateTime = sqlite3_column_int(pStmt, UPDATETIME);

			if (nLastTime < info.UpdateTime)
			{
				qDebug() << "[CDBUtils] GetLastUpdateTime() > info.UpdateTime=" << info.UpdateTime << ", currentTime=" << nLastTime;
				nLastTime = info.UpdateTime + 10;
			}

			rc = sqlite3_step(pStmt);

			break;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] GetLastUpdateTime fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return nLastTime;
}

QVector<PATIENT_INFO> CDBUtils::RecentPatient(int days, int nMaxCount, int& nRetCode)
{
	QVector<PATIENT_INFO> listPatient;
	
	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] RecentPatient fail ";
		nRetCode = SQLITE_BUSY;
		return listPatient;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;
	
	string str;
	str = "SELECT rowid, * FROM patient WHERE UpdateTime > ? ORDER BY UpdateTime DESC ";

	try {

		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			qDebug() << "[CDBUtils] RecentPatient fail " << m_dbSqlite.GetLatestError().c_str();
			CloseDB();
			nRetCode = SQLITE_BUSY;
			return listPatient;
		}

		rc = sqlite3_reset(pStmt);

		time_t ltime;
		struct tm today, searchday;
		time(&ltime); //crnt time
		localtime_s(&today, &ltime);

		searchday = today;
		searchday.tm_mday -= days;
		time_t searchtime;
		searchtime = mktime(&searchday);

		time_t currTime;
		time(&currTime);

		struct tm *ptmTemp;
		ptmTemp = localtime(&currTime);

		int nCurrentYear = ptmTemp->tm_year;
		nCurrentYear += 1900;

		rc = sqlite3_bind_int(pStmt, 1, searchtime);
		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			if (listPatient.size() >= nMaxCount) break;

			char cpTemp[1024] = { 0, };

			PATIENT_INFO info;
			// all column
			info.RowId = sqlite3_column_int(pStmt, ROWID);

			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.Name, "%s", sqlite3_column_text(pStmt, NAME));
			sprintf_s(info.FirstName, "%s", sqlite3_column_text(pStmt, FIRSTNAME));
			sprintf_s(info.LastName, "%s", sqlite3_column_text(pStmt, LASTNAME));
			sprintf_s(info.Birthdate, "%s", sqlite3_column_text(pStmt, BIRTHDATE));

			info.Gender = sqlite3_column_int(pStmt, GENDER);
			info.UpdateTime = sqlite3_column_int(pStmt, UPDATETIME);
			info.DeleteTime = sqlite3_column_int(pStmt, DELETETIME);
			info.Status = sqlite3_column_int(pStmt, STATUS);

			sprintf_s(info.PersonalNum, "%s", sqlite3_column_text(pStmt, PERSONALNUM));
			sprintf_s(info.AccessNum, "%s", sqlite3_column_text(pStmt, ACCESSNUM));
			sprintf_s(info.OtherId, "%s", sqlite3_column_text(pStmt, OTHERID));

			sprintf_s(info.Category, "%s", sqlite3_column_text(pStmt, CATEGORY));
			sprintf_s(info.Physician, "%s", sqlite3_column_text(pStmt, PHYSICIAN));
			sprintf_s(info.Description, "%s", sqlite3_column_text(pStmt, DESCRIPTION));
			
			sprintf_s(info.Email, "%s", sqlite3_column_text(pStmt, EMAIL));
			sprintf_s(info.Address, "%s", sqlite3_column_text(pStmt, HOME_ADDRESS));
			sprintf_s(info.ZipCode, "%s", sqlite3_column_text(pStmt, ZIPCODE));
			sprintf_s(info.Phone, "%s", sqlite3_column_text(pStmt, PHONE));
			sprintf_s(info.MobilePhone, "%s", sqlite3_column_text(pStmt, MOBILE));
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, PHOTO_NAME));
			if (strlen(cpTemp) >= 60)
				strcpy(info.ImageFileName, CutUnicodeString(cpTemp, 58));
			else
				strcpy(info.ImageFileName, cpTemp);

			sprintf_s(info.Reserve1, "%s", sqlite3_column_text(pStmt, RESERVED1));
			sprintf_s(info.Reserve2, "%s", sqlite3_column_text(pStmt, RESERVED2));
			//hjpark@20200609:ChartNum Add
			sprintf_s(info.ChartNum, "%s", sqlite3_column_text(pStmt, CHARTNUM));

			info.PX = sqlite3_column_int(pStmt, PX);
			info.CX = sqlite3_column_int(pStmt, CX);
			info.OX = sqlite3_column_int(pStmt, OX);
			info.OV = sqlite3_column_int(pStmt, OV);
			info.DC = sqlite3_column_int(pStmt, DC);
			info.CT = sqlite3_column_int(pStmt, CT);

			info.PX_UpdateTime = sqlite3_column_int(pStmt, PX_UPDATE); // 최근영상 획득 시간으로 사용 2020/08/11
			info.CX_UpdateTime = sqlite3_column_int(pStmt, CX_UPDATE);
			info.OX_UpdateTime = sqlite3_column_int(pStmt, OX_UPDATE);
			info.OV_UpdateTime = sqlite3_column_int(pStmt, OV_UPDATE);
			info.DC_UpdateTime = sqlite3_column_int(pStmt, DC_UPDATE);
			info.CT_UpdateTime = sqlite3_column_int(pStmt, CT_UPDATE);
			info.nInvisible = sqlite3_column_int(pStmt, INVISIBLE);

			// 나이 갱신
			if (strlen(info.Birthdate) >= 8)
			{
				time_t currTime;
				time(&currTime);
				struct tm *ptmTemp;
				ptmTemp = localtime(&currTime);

				int nYY, nMM, nDD;
				char cpTemp[16] = { 0, };
				strncpy(cpTemp, info.Birthdate, 4);
				nYY = atoi(cpTemp);

				memset(cpTemp, 0x00, sizeof(char) * 16);
				strncpy(cpTemp, info.Birthdate + 5, 2);
				nMM = atoi(cpTemp);

				memset(cpTemp, 0x00, sizeof(char) * 16);
				strncpy(cpTemp, info.Birthdate + 8, 2);
				nDD = atoi(cpTemp);

				int nAge = ptmTemp->tm_year + 1900 - nYY;
				if (nMM - (ptmTemp->tm_mon + 1) < 0)
					nAge = nAge;
				else if (nMM - (ptmTemp->tm_mon + 1) > 0)
					nAge = nAge - 1;
				else
				{
					if (nDD - (ptmTemp->tm_mday) < 0)
						nAge = nAge;
					else
						nAge = nAge - 1;
				}

				if (nAge < 0) nAge = 0;

				info.Age = nAge;
			}

			listPatient.push_back(info);

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] RecentPatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	nRetCode = SQLITE_DONE;

	return listPatient;
}

QVector<PATIENT_INFO> CDBUtils::SearchPatientByImage(CQuery* pQuery, bool bDelete /*= false*/)
{
	QVector<PATIENT_INFO> listPatient;

	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] SearchPatient fail";
		return listPatient;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string str = pQuery->GetQueryString();

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return listPatient;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			PATIENT_INFO info;
			// all column
			info.RowId = sqlite3_column_int(pStmt, ROWID);
						
			char cpMaxBuff[256] = { 0, };
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.Name, u8"%s", sqlite3_column_text(pStmt, NAME));
			sprintf_s(info.FirstName, u8"%s", sqlite3_column_text(pStmt, FIRSTNAME));
			sprintf_s(info.LastName, u8"%s", sqlite3_column_text(pStmt, LASTNAME));
			sprintf_s(info.Birthdate, "%s", sqlite3_column_text(pStmt, BIRTHDATE));

			info.Gender = sqlite3_column_int(pStmt, GENDER);
			info.UpdateTime = sqlite3_column_int(pStmt, UPDATETIME);
			info.DeleteTime = sqlite3_column_int(pStmt, DELETETIME);
			info.Status = sqlite3_column_int(pStmt, STATUS);

			sprintf_s(info.PersonalNum, "%s", sqlite3_column_text(pStmt, PERSONALNUM));
			sprintf_s(info.AccessNum, "%s", sqlite3_column_text(pStmt, ACCESSNUM));
			sprintf_s(info.OtherId, "%s", sqlite3_column_text(pStmt, OTHERID));
			sprintf_s(info.Category, "%s", sqlite3_column_text(pStmt, CATEGORY));
			sprintf_s(info.Physician, "%s", sqlite3_column_text(pStmt, PHYSICIAN));

			sprintf_s(info.Description, "%s", sqlite3_column_text(pStmt, DESCRIPTION));
			sprintf_s(info.Email, "%s", sqlite3_column_text(pStmt, EMAIL));
			sprintf_s(info.Address, "%s", sqlite3_column_text(pStmt, HOME_ADDRESS));
			sprintf_s(info.ZipCode, "%s", sqlite3_column_text(pStmt, ZIPCODE));
			sprintf_s(info.Phone, "%s", sqlite3_column_text(pStmt, PHONE));
			sprintf_s(info.MobilePhone, "%s", sqlite3_column_text(pStmt, MOBILE));
			sprintf_s(info.ImageFileName, "%s", sqlite3_column_text(pStmt, PHOTO_NAME));

			info.PX = sqlite3_column_int(pStmt, PX);
			info.CX = sqlite3_column_int(pStmt, CX);
			info.OX = sqlite3_column_int(pStmt, OX);
			info.OV = sqlite3_column_int(pStmt, OV);
			info.DC = sqlite3_column_int(pStmt, DC);
			info.CT = sqlite3_column_int(pStmt, CT);

			info.PX_UpdateTime = sqlite3_column_int(pStmt, PX_UPDATE);
			info.CX_UpdateTime = sqlite3_column_int(pStmt, CX_UPDATE);
			info.OX_UpdateTime = sqlite3_column_int(pStmt, OX_UPDATE);
			info.OV_UpdateTime = sqlite3_column_int(pStmt, OV_UPDATE);
			info.DC_UpdateTime = sqlite3_column_int(pStmt, DC_UPDATE);
			info.CT_UpdateTime = sqlite3_column_int(pStmt, CT_UPDATE);
			info.nInvisible = sqlite3_column_int(pStmt, INVISIBLE);

			sprintf_s(info.Reserve1, "%s", sqlite3_column_text(pStmt, RESERVED1));
			sprintf_s(info.Reserve2, "%s", sqlite3_column_text(pStmt, RESERVED2));

			listPatient.push_back(info);

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return listPatient;
}

int CDBUtils::IsExistPatientID(QString strId, int &nCount, bool bDeleteTable /*= false*/)
{
	int rc = SQLITE_ERROR;

	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] IsExistPatientID fail";
		return rc;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	if (bDeleteTable)
		sprintf(cpQuery, "select rowid from patientDelete WHERE ID='%s';", strId.toUtf8().toStdString().c_str());
	else
		sprintf(cpQuery, "select rowid from patient WHERE ID='%s';", strId.toUtf8().toStdString().c_str());

	try
	{
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return rc;
		}

		rc = sqlite3_step(pStmt);
		while (rc == SQLITE_ROW)
		{
			int nRowId = sqlite3_column_int(pStmt, ROWID);

			nCount += 1;

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] IsExistPatientID fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return rc;
}

int CDBUtils::IsExistPatientChartNum(QString strChartNum, int &nCount, bool bDeleteTable /*= false*/)
{
	int rc = SQLITE_ERROR;

	// hjpark@20210311:Read Only Flag
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] IsExistPatientChartNum fail";
		return rc;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	if(bDeleteTable)
		sprintf(cpQuery, "select rowid from patientDelete WHERE ChartNum='%s';", strChartNum.toUtf8().toStdString().c_str());
	else 
		sprintf(cpQuery, "select rowid from patient WHERE ChartNum='%s';", strChartNum.toUtf8().toStdString().c_str());

	try
	{
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return rc;
		}

		rc = sqlite3_step(pStmt);
		while (rc == SQLITE_ROW)
		{
			int nRowId = sqlite3_column_int(pStmt, ROWID);
			nCount += 1;
			rc = sqlite3_step(pStmt);
		}
	}  catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] IsExistPatientChartNum fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return rc;
}
// ID는 같고, 이름 혹은 생년월일이 같은 경우 동일환자로 판단.
int CDBUtils::IsExistSamePatient_Conv(QString strId, QString strName, QString strBirth, int &nCount, bool bCheckDB /*=true*/)
{
	int rc = SQLITE_ERROR;

	if (bCheckDB)
	{
		// hjpark@20210311:Read Only Flag
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] IsExistSamePatient_Conv fail";
			return 0;
		}
	}
	
	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "select rowid from patient WHERE Id='%s' AND (Name = '%s' OR Birthdate = '%s');", 
		strId.toUtf8().toStdString().c_str(), strName.toUtf8().toStdString().c_str(), strBirth.toStdString().c_str());

	try
	{
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return rc;
		}

		rc = sqlite3_step(pStmt);
		while (rc == SQLITE_ROW)
		{
			int nRowId = sqlite3_column_int(pStmt, ROWID);

			nCount += 1;

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	if (bCheckDB)
	{
		CloseDB();
	}

	return rc;
}

void CDBUtils::SetQuickSearchKeyword(QString strKey)
{
	memset(m_cpKeyword, 0x00, 64);
	strcpy(m_cpKeyword, strKey.toStdString().c_str());
}

QVector<PATIENT_INFO> CDBUtils::SearchPatient(CQuery* pQuery, int& nRet, bool bCheckDB /*= true*/)
{
	QVector<PATIENT_INFO> listPatient;

	if (bCheckDB)
	{
		// hjpark@20210311:Read Only Flag
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] SearchPatient Open DB fail";
			nRet = SQLITE_BUSY;
			return listPatient;
		}
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();				  
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;
	
	string str = pQuery->GetQueryString();
	
	try
	{
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			nRet = rc;
			CloseDB();
			return listPatient;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			PATIENT_INFO info;
			// all column
			info.RowId = sqlite3_column_int(pStmt, ROWID);

			char cpMaxBuff[256] = { 0, };
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.Name, u8"%s", sqlite3_column_text(pStmt, NAME));
			sprintf_s(info.FirstName, u8"%s", sqlite3_column_text(pStmt, FIRSTNAME));
			sprintf_s(info.LastName, u8"%s", sqlite3_column_text(pStmt, LASTNAME));
			sprintf_s(info.Birthdate, "%s", sqlite3_column_text(pStmt, BIRTHDATE));

			info.Gender = sqlite3_column_int(pStmt, GENDER);
			info.UpdateTime = sqlite3_column_int(pStmt, UPDATETIME);
			info.DeleteTime = sqlite3_column_int(pStmt, DELETETIME);
			info.Status = sqlite3_column_int(pStmt, STATUS);

			sprintf_s(info.PersonalNum, "%s", sqlite3_column_text(pStmt, PERSONALNUM));
			sprintf_s(info.AccessNum, "%s", sqlite3_column_text(pStmt, ACCESSNUM));
			sprintf_s(info.OtherId, "%s", sqlite3_column_text(pStmt, OTHERID));
			sprintf_s(info.Category, "%s", sqlite3_column_text(pStmt, CATEGORY));
			sprintf_s(info.Physician, "%s", sqlite3_column_text(pStmt, PHYSICIAN));

			sprintf_s(info.Description, "%s", sqlite3_column_text(pStmt, DESCRIPTION));
			sprintf_s(info.Email, "%s", sqlite3_column_text(pStmt, EMAIL));
			sprintf_s(info.Address, "%s", sqlite3_column_text(pStmt, HOME_ADDRESS));
			sprintf_s(info.ZipCode, "%s", sqlite3_column_text(pStmt, ZIPCODE));
			sprintf_s(info.Phone, "%s", sqlite3_column_text(pStmt, PHONE));
			sprintf_s(info.MobilePhone, "%s", sqlite3_column_text(pStmt, MOBILE));
			sprintf_s(info.ImageFileName, "%s", sqlite3_column_text(pStmt, PHOTO_NAME));

			info.PX = sqlite3_column_int(pStmt, PX);
			info.CX = sqlite3_column_int(pStmt, CX);
			info.OX = sqlite3_column_int(pStmt, OX);
			info.OV = sqlite3_column_int(pStmt, OV);
			info.DC = sqlite3_column_int(pStmt, DC);
			info.CT = sqlite3_column_int(pStmt, CT);

			info.PX_UpdateTime = sqlite3_column_int(pStmt, PX_UPDATE);
			info.CX_UpdateTime = sqlite3_column_int(pStmt, CX_UPDATE);
			info.OX_UpdateTime = sqlite3_column_int(pStmt, OX_UPDATE);
			info.OV_UpdateTime = sqlite3_column_int(pStmt, OV_UPDATE);
			info.DC_UpdateTime = sqlite3_column_int(pStmt, DC_UPDATE);
			info.CT_UpdateTime = sqlite3_column_int(pStmt, CT_UPDATE);
			info.nInvisible = sqlite3_column_int(pStmt, INVISIBLE);

			sprintf_s(info.Reserve1, "%s", sqlite3_column_text(pStmt, RESERVED1));
			sprintf_s(info.Reserve2, "%s", sqlite3_column_text(pStmt, RESERVED2));
			//hjpark@20200609:CharNum Add
			sprintf_s(info.ChartNum, "%s", sqlite3_column_text(pStmt, CHARTNUM));

			// QuickSearch Hit 조사 
			if (strlen(m_cpKeyword) > 0)
			{
				if (strcmp(m_cpKeyword, info.ChartNum) == 0 || strcmp(m_cpKeyword, info.Name) == 0)
					info.nHitValue = 100;
			}
			
			listPatient.push_back(info);

			rc = sqlite3_step(pStmt);			
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	memset(m_cpKeyword, 0x00, sizeof(char) * 64);

	if (bCheckDB)
	{
		CloseDB();
	}

	nRet = SQLITE_DONE;

	return listPatient;
}

int CDBUtils::DeletePatientDB(char* cpID, bool bCheckDB /*=true*/)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (bCheckDB)
	{
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] DeletePatient fail";
			return ret;
		}
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql;
	strSql = "DELETE from patient where Id =?";

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);
		sqlite3_bind_text(pStmt, 1, cpID, -1, SQLITE_STATIC);
		ret = sqlite3_step(pStmt);
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeletePatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	if (bCheckDB)
		CloseDB();

	if (ret != SQLITE_DONE)
		return ret;

	return ret;
}

int CDBUtils::DeletePatient(PATIENT_INFO* pPatient, bool bDeleteTable /*=false*/)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] DeletePatient fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();				   
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql;
	if(bDeleteTable)
		strSql = "DELETE from patientDelete where Id =?";
	else 
		strSql = "DELETE from patient where Id =?";

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);
		sqlite3_bind_text(pStmt, 1, pPatient->Id, -1, SQLITE_STATIC);
		ret = sqlite3_step(pStmt);
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeletePatient fail" << error_msg.c_str();		
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	if (ret != SQLITE_DONE )
		return ret;
	else
	{
		// 영구삭제는 삭제 환자 테이블에서 삭제만 진행
		if (!bDeleteTable)
			ret = MovePatientToDeleteTable(pPatient);
	}

	return ret;
}

int CDBUtils::PermanentlyDeletePatient(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] PermanentlyDeletePatient fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from patientDelete where Id =?";

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);
		sqlite3_bind_text(pStmt, 1, pPatient->Id, -1, SQLITE_STATIC);
		ret = sqlite3_step(pStmt);
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] PermanentlyDeletePatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();
	
	return ret;
}

int  CDBUtils::MovePatientToDeleteTable(PATIENT_INFO* pPatient)
{
	int nRet = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] MovePatientToDeleteTable fail ";
		return nRet;
	}

	BeginTransaction();

	string strSql = "INSERT INTO patientDelete ( Id, Name, FirstName, LastName, Birthdate, Gender, UpdateTime, DeleteTime, Status, PersonalNumber, \
                                           Accession, OtherId, Category, Physician, Description, Email, Address, ZipCode, Phone, MobilePhone, \
										   ProfileImage, PX, CX, OX, OV, DC, CT, PX_UpdateTime, CX_UpdateTime, OX_UpdateTime, \
										   OV_UpdateTime, DC_UpdateTime, CT_UpdateTime, Invisible, Reserve1, Reserve2, ChartNum )\
								    VALUES ( ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?);";

	sqlite3		 *pDB = m_dbSqlite.GetDB();				  
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		nRet = sqlite3_reset(pStmt);

		int nIdx = 1;

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->FirstName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->LastName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);

		sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DeleteTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Status);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->OtherId, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Category, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Physician, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Description, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Email, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Address, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ZipCode, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Phone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->MobilePhone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ImageFileName, -1, SQLITE_STATIC);

		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OV);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DC);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CT);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OV_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DC_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CT_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->nInvisible);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve1, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve2, -1, SQLITE_STATIC);

		//hjpark@20200828:삭제시 ChartNum변경
		//sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);
		char cpChartNum[128] = { 0, };
		strcpy(cpChartNum, pPatient->ChartNum);
		strcat(cpChartNum, "_del");
		sqlite3_bind_text(pStmt, nIdx++, cpChartNum, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			qDebug() << "[CDBUtils] MovePatientToDeleteTable fail";
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] MovePatientToDeleteTable fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return nRet;
}

// 환자복구
int CDBUtils::RecoveryPatient(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] RecoveryPatient fail";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from patientDelete where Id =?";

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);
		sqlite3_bind_text(pStmt, 1, pPatient->Id, -1, SQLITE_STATIC);
		ret = sqlite3_step(pStmt);
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeletePatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);	

	CloseDB();
	
	return ret;
}
// 삭제 테이블에서 환자테이블로 이동
int  CDBUtils::MoveDeleteToPatientTable(PATIENT_INFO* pPatient)
{
	int nRet = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] MoveDeleteToPatientTable fail";
		return nRet;
	}

	BeginTransaction();

	string strSql = "INSERT INTO patient ( Id, Name, FirstName, LastName, Birthdate, Gender, UpdateTime, DeleteTime, Status, PersonalNumber, \
                                           Accession, OtherId, Category, Physician, Description, Email, Address, ZipCode, Phone, MobilePhone, \
										   ProfileImage, PX, CX, OX, OV, DC, CT, PX_UpdateTime, CX_UpdateTime, OX_UpdateTime, \
										   OV_UpdateTime, DC_UpdateTime, CT_UpdateTime, Invisible, Reserve1, Reserve2, ChartNum )\
								    VALUES ( ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?,?,?,?, \
											 ?,?,?,?,?, ?,?);";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		nRet = sqlite3_reset(pStmt);

		int nIdx = 1;

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->FirstName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->LastName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);

		sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DeleteTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Status);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->OtherId, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Category, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Physician, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Description, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Email, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Address, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ZipCode, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Phone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->MobilePhone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ImageFileName, -1, SQLITE_STATIC);

		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OX);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OV);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DC);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CT);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OX_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->OV_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->DC_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->CT_UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->nInvisible);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve1, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Reserve2, -1, SQLITE_STATIC);

		//hjpark@20200609:ChartNum add
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
			printf("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] MovePatientToDeleteTable fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return nRet;
}
int CDBUtils::EditPhysicianInfo(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditPhysicianInfo fail";
		return ret;
	}

	string strSql = "UPDATE patient SET Physician = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Physician, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditPhysicianInfo fail";
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditPhysicianInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}
int CDBUtils::EditCategoryInfo(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditCategoryInfo fail";
		return ret;
	}

	string strSql = "UPDATE patient SET Category = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Category, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditCategoryInfo fail";
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditCategoryInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}
int CDBUtils::EditCommentInfo(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditCommentInfo fail";
		return ret;
	}

	string strSql = "UPDATE patient SET Description = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Description, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditCommentInfo fail";
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditCommentInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

int CDBUtils::EditPatientInfo(PATIENT_INFO* pPatient, int nType, bool bDeleteTable /*= false*/)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditPatientInfo fail";
		return ret;
	}

	char cpQuery[256] = { 0, };
	char cpTableName[256] = { 0, };
	if (bDeleteTable) 
		strcpy(cpTableName, "patientDelete");
	else 
		strcpy(cpTableName, "patient");

	if (nType == 0) // Name
		sprintf(cpQuery, "UPDATE %s SET Name = ?, FirstName = ?, LastName = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 1) // BirthDate
		sprintf(cpQuery, "UPDATE %s SET Birthdate = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 2) // Gender
		sprintf(cpQuery, "UPDATE %s SET Gender = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 3) // Accession Num
		sprintf(cpQuery, "UPDATE %s SET Accession = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 4) // Social Num
		sprintf(cpQuery, "UPDATE %s SET PersonalNumber = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 5) // Chart Num
		sprintf(cpQuery, "UPDATE %s SET ChartNum = ? WHERE Id = ?; ", cpTableName);
	
	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;

		if (nType == 0) // Name
		{
			sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
			sqlite3_bind_text(pStmt, nIdx++, pPatient->FirstName, -1, SQLITE_STATIC);
			sqlite3_bind_text(pStmt, nIdx++, pPatient->LastName, -1, SQLITE_STATIC);
		}
		else if (nType == 1) // BirthDate
			sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);
		else if (nType == 2) // Gender
			sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		else if (nType == 3) // Accession Num
			sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		else if (nType == 4) // Social Num
			sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		else if (nType == 5) // ChartNum
			sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditAccessionInfo fail";
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditAccessionInfo fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	return ret;
}

int CDBUtils::EditPatientInfo_Conv(PATIENT_INFO* pPatient, int nType)
{
	int ret = SQLITE_ERROR;

	char cpQuery[256] = { 0, };
	char cpTableName[256] = { 0, };
	
	strcpy(cpTableName, "patient");

	if (nType == 0) // Name
		sprintf(cpQuery, "UPDATE %s SET Name = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 1) // BirthDate
		sprintf(cpQuery, "UPDATE %s SET Birthdate = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 2) // Gender
		sprintf(cpQuery, "UPDATE %s SET Gender = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 3) // Accession Num
		sprintf(cpQuery, "UPDATE %s SET Accession = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 4) // Social Num
		sprintf(cpQuery, "UPDATE %s SET PersonalNumber = ? WHERE Id = ?; ", cpTableName);
	else if (nType == 5) // Chart Num
		sprintf(cpQuery, "UPDATE %s SET ChartNum = ? WHERE Id = ?; ", cpTableName);

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		ret = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;

		if (nType == 0) // Name
			sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
		else if (nType == 1) // BirthDate
			sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);
		else if (nType == 2) // Gender
			sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		else if (nType == 3) // Accession Num
			sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		else if (nType == 4) // Social Num
			sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		else if (nType == 5) // ChartNum
			sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditAccessionInfo fail";

	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditAccessionInfo fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	return ret;

}

int CDBUtils::RestorePXUpdateTime()
{
	int ret = SQLITE_ERROR;

	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditUpdateTime > CheckValidDBConnection fail" << ret;
		return ret;
	}

	string serachsql = "SELECT Id, UpdateTime, PX_UpdateTime from patient WHERE PX_UpdateTime = '' or PX_UpdateTime is NULL or PX_UpdateTime = '0';";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	vector<PATIENT_INFO> store_patient;

	try
	{
		ret = sqlite3_prepare_v2(pDB, serachsql.c_str(), -1, &pStmt, NULL);

		if (ret != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return ret;
		}

		ret = sqlite3_step(pStmt);

		while (ret == SQLITE_ROW)
		{
			PATIENT_INFO temp;
			sprintf_s(temp.Id, "%s", sqlite3_column_text(pStmt, 0));
			temp.UpdateTime = sqlite3_column_int(pStmt, 1);
			temp.PX_UpdateTime = sqlite3_column_int(pStmt, 2);

			store_patient.push_back(temp);

			ret = sqlite3_step(pStmt);
		}
	}
	catch (const std::exception&)
	{
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);


	for (int i = 0; i < store_patient.size(); i++)
	{
		if (store_patient[i].PX_UpdateTime == 0)
		{
			store_patient[i].PX_UpdateTime = store_patient[i].UpdateTime;

			this->EditUpdateTime(&store_patient[i]);
		}
	}

	return ret;
}
int CDBUtils::EditUpdateTime(PATIENT_INFO* pPatient, bool bCheckDB/*=true*/)
{
	int ret = SQLITE_ERROR;

	if(bCheckDB)
	{
		// DB 연결 끊어질 경우 대비
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] EditUpdateTime > CheckValidDBConnection fail"<< ret;
			return ret;
		}
	}

	string strSql = "UPDATE patient SET UpdateTime = ?, PX_UpdateTime = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX_UpdateTime);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] EditUpdateTime Process fail." << ret;
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditUpdateTime fail" << error_msg.c_str();
	}
	
	sqlite3_finalize(pStmt);
	
	if (bCheckDB)
		CloseDB();

	return ret;
}

int CDBUtils::UpdateLastAcqTime(PATIENT_INFO* pPatient, bool bCheckDB/*=true*/)
{
	int ret = SQLITE_ERROR;

	if (bCheckDB)
	{
		// DB 연결 끊어질 경우 대비
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] UpdateLastAcqTime fail";
			return ret;
		}
	}

	string strSql = "UPDATE patient SET PX_UpdateTime = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	if (bCheckDB)
		BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_int(pStmt, nIdx++, pPatient->PX_UpdateTime);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] UpdateLastAcqTime fail";

	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateLastAcqTime fail" << error_msg.c_str();
	}

	if (bCheckDB)
		CommitTransection();

	sqlite3_finalize(pStmt);

	if (bCheckDB)
		CloseDB();

	return ret;
}

int CDBUtils::EditPatient(PATIENT_INFO* pPatient)
{
	int ret = SQLITE_ERROR;

	// DB 연결 끊어질 경우 대비
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] EditPatient fail";
		return ret;
	}
	
	string strSql = "UPDATE patient SET Name = ?, FirstName = ?, LastName = ?,  Birthdate = ?, Gender = ?, UpdateTime = ?, Status = ?,  PersonalNumber = ?, Accession = ?, OtherId = ?,\
							 Category = ?, Physician = ?, Description = ?, Email = ?, Address = ?, ZipCode = ?, Phone = ?, MobilePhone = ?,  ProfileImage = ?, ChartNum = ? \
								  WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();				   
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Name, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->FirstName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->LastName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Birthdate, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Gender);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->UpdateTime);
		sqlite3_bind_int(pStmt, nIdx++, pPatient->Status);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->PersonalNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->AccessNum, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->OtherId, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Category, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Physician, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Description, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Email, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Address, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ZipCode, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Phone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->MobilePhone, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ImageFileName, -1, SQLITE_STATIC);
		//hjpark@20200609:ChartNum add
		sqlite3_bind_text(pStmt, nIdx++, pPatient->ChartNum, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			printf("ERROR inserting data: %s\n", m_dbSqlite.GetLatestError().c_str());

		// 나이 갱신		
		if (strlen(pPatient->Birthdate) >= 8)
		{
			time_t currTime;
			time(&currTime);
			struct tm *ptmTemp;
			ptmTemp = localtime(&currTime);

			int nYY, nMM, nDD;
			char cpTemp[16] = { 0, };
			strncpy(cpTemp, pPatient->Birthdate, 4);
			nYY = atoi(cpTemp);

			memset(cpTemp, 0x00, sizeof(char) * 16);
			strncpy(cpTemp, pPatient->Birthdate + 5, 2);
			nMM = atoi(cpTemp);
			
			memset(cpTemp, 0x00, sizeof(char) * 16);
			strncpy(cpTemp, pPatient->Birthdate + 8, 2);
			nDD = atoi(cpTemp);

			int nAge = ptmTemp->tm_year + 1900 - nYY;
			if (nMM - (ptmTemp->tm_mon + 1) < 0)
				nAge = nAge;
			else if (nMM - (ptmTemp->tm_mon + 1) > 0)
				nAge = nAge - 1;
			else
			{
				if (nDD - (ptmTemp->tm_mday) < 0)
					nAge = nAge;
				else
					nAge = nAge - 1;
			}	

			if (nAge < 0) nAge = 0;
			
			pPatient->Age = nAge;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] EditPatient fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);

	CloseDB();
	
	return ret;
}

int CDBUtils::BeginTransaction()
{
	int ret = SQLITE_ERROR;
	ret = m_dbSqlite.BeginTransaction();
	return ret;
}

int CDBUtils::CommitTransection()
{
	int ret = SQLITE_ERROR;
	ret = m_dbSqlite.CommitTransection();
	return ret;
}

int CDBUtils::Execute(char* cpQuery)
{
	string strQuery;
	strQuery = cpQuery;
	int ret = m_dbSqlite.Excute(strQuery);
	if (ret != SQLITE_OK)
	{
		//error
		qDebug() << "[CDBUtils] Execute fail" << strQuery.c_str();
	}
	return ret;
}

int CDBUtils::UpdateImageCounts_Conv(PATIENT_INFO* pPatient, QVector<int> oCountList)
{
	int ret = SQLITE_ERROR;

	if (oCountList.size() != 6)
		return ret;

	string strSql = "";
	strSql = "UPDATE patient SET PX = ?, CX = ? ,OX = ?, OV = ?, DC = ?, CT = ? WHERE Id = ?; ";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		for(int ii=0; ii < oCountList.size(); ii++)
			sqlite3_bind_int(pStmt, nIdx++, oCountList[ii]);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] UpdateDeleteImage fail";
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateDeleteImage fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	return ret;
}

int CDBUtils::UpdateImageCounts(PATIENT_INFO* pPatient, QVector<int> oCountList)
{
	int ret = SQLITE_ERROR;

	string strSql = "";
	if(oCountList.size() == 7)  // Dental 
		strSql = "UPDATE patient SET PX = ?, CX = ? ,OX = ?, OV = ?, CT = ?, DC = ? WHERE Id = ?; ";
	else if (oCountList.size() == 4) // ENT
		strSql = "UPDATE patient SET CX = ?, CT = ?, DC = ? WHERE Id = ?; ";
	else
		return ret;

	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > UpdateImageCounts fail" << ret;
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		for (int ii = 0; ii < oCountList.size() - 1; ii++)
			sqlite3_bind_int(pStmt, nIdx++, oCountList[ii]);

		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] UpdateImageCounts fail" << ret;
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateImageCounts fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return ret;
}

// 영상 Count 관련
int CDBUtils::UpdateDeleteImage(PATIENT_INFO* pPatient, QString strModality, int nCount)
{
	int ret = SQLITE_ERROR;

	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > UpdateDeleteImage fail";
		return ret;
	}

	char cpModality[16] = { 0, };
	strcpy(cpModality, strModality.toStdString().c_str());

	string strSql = "";
	if (strcmp(cpModality, "PX") == 0)
		strSql = "UPDATE patient SET PX = ? WHERE Id = ?; ";
	else if (strcmp(cpModality, "CX") == 0)
		strSql = "UPDATE patient SET CX = ? WHERE Id = ?; ";
	else if (strcmp(cpModality, "OX") == 0)
		strSql = "UPDATE patient SET OX = ? WHERE Id = ?; ";
	else if (strcmp(cpModality, "OV") == 0)
		strSql = "UPDATE patient SET OV = ? WHERE Id = ?; ";
	else if (strcmp(cpModality, "DC") == 0)
		strSql = "UPDATE patient SET DC = ? WHERE Id = ?; ";
	else if (strcmp(cpModality, "CT") == 0)
		strSql = "UPDATE patient SET CT = ? WHERE Id = ?; ";
	else
		return -1;
	
	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		ret = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_int(pStmt, nIdx++, nCount);
		sqlite3_bind_text(pStmt, nIdx++, pPatient->Id, -1, SQLITE_STATIC);

		ret = sqlite3_step(pStmt);
		if (ret != SQLITE_DONE)
			qDebug() << "[CDBUtils] UpdateDeleteImage fail";
	} catch (std::exception& e) 	{
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] UpdateDeleteImage fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);

	CloseDB();

	return ret;
}

///////////////////////////////////////////////////////////////////////////
// Physician
QVector<DB_TABLE_PHYSICIAN> CDBUtils::GetPhysicianList()
{
	int rc = SQLITE_ERROR;
	QVector<DB_TABLE_PHYSICIAN> vecResult;

	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] > GetPhysicianList fail";
		return vecResult;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	
	string str = "SELECT rowid, * FROM Physician";

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return vecResult;
		}

		while (true)
		{
			DB_TABLE_PHYSICIAN info;
			memset(&info, 0x00, sizeof(DB_TABLE_PHYSICIAN));
			rc = sqlite3_step(pStmt);
			if (rc == SQLITE_ROW)
			{
				// all column
				int nRowId = sqlite3_column_int(pStmt, PHYSICIAN_ROWID);

				sprintf_s(info.cpName, u8"%s", sqlite3_column_text(pStmt, PHYSICIAN_NAME));
				sprintf_s(info.cpTel, u8"%s", sqlite3_column_text(pStmt, PHYSICIAN_TEL));
				sprintf_s(info.cpMajor, u8"%s", sqlite3_column_text(pStmt, PHYSICIAN_MAJOR));
				sprintf_s(info.cpDesc, u8"%s", sqlite3_column_text(pStmt, PHYSICIAN_DESCRIPTION));

				vecResult.push_back(info);
			}
			else
			{
				break;
			}
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] GetPhysicianList fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return vecResult;
}

int CDBUtils::AddPhysician(DB_TABLE_PHYSICIAN* pCat, bool bCheckDB /*=true*/)
{
	int nRet = SQLITE_ERROR;

	if (bCheckDB)
	{
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] > AddPhysician fail";
			return nRet;
		}
	}
	
	string strSql = "INSERT INTO Physician ( Name, Tel, Major, Description ) VALUES ( ?,?,?,?);";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	if (bCheckDB)
		BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			if (bCheckDB)
			{
				CommitTransection();
				CloseDB();
			}
			return nRet;
		}

		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, pCat->cpName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pCat->cpTel, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pCat->cpMajor, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, nIdx++, pCat->cpDesc, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);		
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] AddPhysician fail" << error_msg.c_str();
	}

	if (bCheckDB)
		CommitTransection();
	
	sqlite3_finalize(pStmt);
	
	if (bCheckDB)
	{
		CloseDB();
	}

	if (nRet == SQLITE_DONE)
		return SQLITE_OK;

	return nRet;
}
int CDBUtils::ModifyPhysician(DB_TABLE_PHYSICIAN* pCat)
{
	int nRet = SQLITE_ERROR;

	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > ModifyPhysician fail";
		return nRet;
	}

	string strSql = "UPDATE Physician SET Tel = ?, Major = ?, Description = ? WHERE Name = ? ;";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();
	
	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpTel, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, 2, pCat->cpMajor, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, 3, pCat->cpDesc, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, 4, pCat->cpName, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			if(pStmt) sqlite3_finalize(pStmt);
			CommitTransection();
			CloseDB();
			return SQLITE_ERROR;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] ModifyPhysician fail" << error_msg.c_str();
	}

	CommitTransection();
	
	sqlite3_finalize(pStmt);

	CloseDB();

	return SQLITE_OK;
}
int CDBUtils::DeletePhysician(DB_TABLE_PHYSICIAN* pCat)
{
	int nRet = SQLITE_ERROR;
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > DeletePhysician fail";
		return nRet;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from Physician where Name =?";

	BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpName, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			CommitTransection();
			CloseDB();
			return SQLITE_ERROR;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeletePhysician fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);

	CloseDB();
		
	return SQLITE_OK;
}

//////////////////////////////////////////////////////////////////////////////////////
// Category
QVector<DB_TABLE_CATEGORY> CDBUtils::GetCategoryList()
{
	int rc = SQLITE_ERROR;
	QVector<DB_TABLE_CATEGORY> vecResult;
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] > GetCategoryList fail";
		return vecResult;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	
	string str = "SELECT rowid, * FROM Category";

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			printf("ERROR : %s\n", m_dbSqlite.GetLatestError().c_str());
			CloseDB();
			return vecResult;
		}

		while (true)
		{
			DB_TABLE_CATEGORY info;
			memset(&info, 0x00, sizeof(DB_TABLE_CATEGORY));
			rc = sqlite3_step(pStmt);
			if (rc == SQLITE_ROW)
			{
				// all column
				int nRowId = sqlite3_column_int(pStmt, CATEGORY_ROWID);

				sprintf_s(info.cpName, u8"%s", sqlite3_column_text(pStmt, CATEGORY_NAME));
				sprintf_s(info.cpDesc, u8"%s", sqlite3_column_text(pStmt, CATEGORY_DESCRIPTION));

				vecResult.push_back(info);
			}
			else
			{
				break;
			}
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] GetCategoryList fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	return vecResult;
}


int CDBUtils::AddCategory(DB_TABLE_CATEGORY* pCat, bool bCheckDB /*=false*/)
{
	int nRet = SQLITE_ERROR;
	if (bCheckDB)
	{
		if (!m_dbSqlite.CheckValidDBConnection())
		{
			qDebug() << "[CDBUtils] > AddCategory fail";
			return nRet;
		}
	}

	string strSql = "INSERT INTO Category ( Name, Description ) VALUES ( ?,? );";
	
	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	if (bCheckDB)
		BeginTransaction();
	
	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpName, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, 2, pCat->cpDesc, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] AddCategory fail" << error_msg.c_str();
	}

	if (bCheckDB)
		CommitTransection();

	sqlite3_finalize(pStmt);
		
	if (bCheckDB)
	{
		CloseDB();
	}

	if (nRet == SQLITE_DONE)
		return SQLITE_OK;
	
	return nRet;
}

int CDBUtils::ModifyCategory(DB_TABLE_CATEGORY* pCat)
{
	int nRet = SQLITE_ERROR;
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > ModifyCategory fail";
		return nRet;
	}

	string strSql = "UPDATE Category SET Description = ? WHERE Name = ? ;";

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpDesc, -1, SQLITE_STATIC);

		sqlite3_bind_text(pStmt, 2, pCat->cpName, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			CommitTransection();
			CloseDB();
			return SQLITE_ERROR;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] ModifyCategory fail" << error_msg.c_str();
	}

	CommitTransection();
	
	sqlite3_finalize(pStmt);

	CloseDB();
	
	return SQLITE_OK;
}

int CDBUtils::DeleteCategory(DB_TABLE_CATEGORY* pCat)
{
	int nRet = SQLITE_ERROR;
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > DeleteCategory fail";
		return nRet;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	string strSql = "DELETE from Category where Name =?";

	BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpName, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			CommitTransection();
			sqlite3_finalize(pStmt);
			CloseDB();
			return SQLITE_ERROR;
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] DeleteCategory fail" << error_msg.c_str();
	}

	CommitTransection();
	
	sqlite3_finalize(pStmt);

	CloseDB();
	
	return SQLITE_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// by jggang 
/////////////////////////////////////////////////////////////////////////////
int CDBUtils::ModifyPhysicianLogInStatus(DB_TABLE_PHYSICIAN* pCat, int use)
{
	int nRet = SQLITE_ERROR;

	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << "[CDBUtils] > ModifyPhysicianLogInStatus fail";
		return nRet;
	}
		
	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	string strSql = "UPDATE Physician SET Reserved1 = ? WHERE Name = ? ;";

	char dummy[0x10]; memset(dummy, 0x00, sizeof(dummy));
	////pCat->cpReserved1 = itoa(use, dummy, 10);
	memcpy(pCat->cpReserved1, itoa(use, dummy, 10), sizeof(dummy));

	BeginTransaction();

	try {
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (nRet != SQLITE_OK)
		{
			CommitTransection();
			CloseDB();
			return nRet;
		}

		sqlite3_reset(pStmt);

		sqlite3_bind_text(pStmt, 1, pCat->cpReserved1, -1, SQLITE_STATIC);
		sqlite3_bind_text(pStmt, 2, pCat->cpName, -1, SQLITE_STATIC);

		nRet = sqlite3_step(pStmt);
		if (nRet != SQLITE_DONE)
		{
			CommitTransection();
			sqlite3_finalize(pStmt);
			CloseDB();
			return SQLITE_ERROR;
		}
	}  catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] ModifyPhysicianLogInStatus fail" << error_msg.c_str();
	}

	CommitTransection();
	
	sqlite3_finalize(pStmt);

	CloseDB();

	return SQLITE_OK;
}

/////////////////////////////////////////////////////////////////////////////
int CDBUtils::GetPhysicianLogInStatus(DB_TABLE_PHYSICIAN* pCat)
{
	int nRet = SQLITE_ERROR; 
	
	if (!m_dbSqlite.CheckValidDBConnection(true))
	{
		qDebug() << "[CDBUtils] > GetPhysicianLogInStatus fail";
		return nRet;
	}

	char reserved1[20] = { 0, };

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	try {
		string strSql = "SELECT Reserved1 FROM Physician WHERE Name = ? ;";
		nRet = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		nRet = sqlite3_reset(pStmt);		
		nRet = sqlite3_bind_text(pStmt, 1, pCat->cpName, -1, SQLITE_STATIC);
		nRet = sqlite3_step(pStmt);
		qDebug() << sqlite3_errstr(nRet);
		
		sprintf(reserved1, "%s", sqlite3_column_text(pStmt, 0));

	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] GetPhysicianLogInStatus fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);
	
	CloseDB();
	
	return atoi(reserved1);
}

///////////////////////////////////////////////////////////////////////////////
PATIENT_INFO CDBUtils::GetPatientRowID(char *PatientID, int flag)
{
	PATIENT_INFO patInfo;
	patInfo.RowId = -1;
	if (!m_dbSqlite.CheckValidDBConnection(true)){
		qDebug() << __FUNCTION__ << "fail[CheckValidDBConnection]";
		return patInfo;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string strSql;
	if (flag == 1) strSql = "SELECT rowid, Id, ChartNum FROM patient WHERE Id = ? OR ChartNum=?;";
	else if (flag == 0) strSql = "SELECT rowid, Id, ChartNum FROM patientDelete WHERE Id = ? OR ChartNum=?;";

	QVector<PATIENT_INFO > listPatient;
	listPatient.clear();

	BeginTransaction();

	try {
		rc = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK){
			qDebug() << "ERROR: " << m_dbSqlite.GetLatestError().c_str();

			CommitTransection();
			CloseDB();
			return patInfo;
		}
		
		rc = sqlite3_reset(pStmt);		
		rc = sqlite3_bind_text(pStmt, 1, PatientID, -1, SQLITE_STATIC);
		rc = sqlite3_bind_text(pStmt, 2, PatientID, -1, SQLITE_STATIC);
		rc = sqlite3_step(pStmt);	
		
		while (rc == SQLITE_ROW){
			PATIENT_INFO info;			
			info.RowId = sqlite3_column_int(pStmt, ROWID);		
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.ChartNum, "%s", sqlite3_column_text(pStmt, 2));
			listPatient.push_back(info);
			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << __FUNCTION__ << "fail" << error_msg.c_str();
	}

	CommitTransection();
	
	sqlite3_finalize(pStmt);
	
	CloseDB();

	for (QVector<PATIENT_INFO >::iterator iter = listPatient.begin(); iter != listPatient.end(); iter++) {
		patInfo = *iter;
		break;
	}

	return patInfo;
}

///////////////////////////////////////////////////////////////////////////////
int CDBUtils::UpdatePatientDBPid(int rowid, char *PatientID, int flag)
{
	int ret = false;

	if (!m_dbSqlite.CheckValidDBConnection()) {
		qDebug() << __FUNCTION__ << "fail[CheckValidDBConnection]";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string strSql;
	if(flag) strSql = "UPDATE patient SET ChartNum = ? WHERE rowid = ? ;";
	else strSql = "UPDATE patientDelete SET ChartNum = ? WHERE rowid = ? ;";

	BeginTransaction();

	try {
		rc = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;		
		sqlite3_bind_text(pStmt, nIdx++, PatientID, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, rowid);

		rc = sqlite3_step(pStmt);
		if (rc != SQLITE_DONE) qDebug() << __FUNCTION__ << "ERROR:" << m_dbSqlite.GetLatestError().c_str();
	}
	catch (std::exception& e) {		
		qDebug() << __FUNCTION__ << "fail" << m_dbSqlite.GetLatestError().c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	if (rc == SQLITE_DONE) ret = true;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
QVector<IMG_DB_INFO > CDBUtils::GetImageDBImgID(char *PatientID)
{
	QVector<IMG_DB_INFO > listImgInfo;
	listImgInfo.clear();	

	if (!m_dbSqlite.CheckValidDBConnection(true)) {
		qDebug() << __FUNCTION__ << "fail[CheckValidDBConnection]";
		return listImgInfo;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string strSql;
	strSql = "SELECT ImgId, Id FROM ImageDB WHERE Id = ? ;";	

	BeginTransaction();
	
	try {
		rc = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK) {
			qDebug() << "ERROR: " << m_dbSqlite.GetLatestError().c_str();
			CommitTransection();
			CloseDB();
			return listImgInfo;
		}

		rc = sqlite3_reset(pStmt);
		rc = sqlite3_bind_text(pStmt, 1, PatientID, -1, SQLITE_STATIC);
		rc = sqlite3_step(pStmt);

		while (rc == SQLITE_ROW) {
			IMG_DB_INFO info;
			info.ImgId = sqlite3_column_int(pStmt, 0);
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, 1));
			listImgInfo.push_back(info);
			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << __FUNCTION__ << "fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();
	
	return listImgInfo;
}

///////////////////////////////////////////////////////////////////////////////
int CDBUtils::UpdateImageDBPid(int ImgId, char *PatientID)
{
	int ret = false;

	if (!m_dbSqlite.CheckValidDBConnection()) {
		qDebug() << __FUNCTION__ << "fail[CheckValidDBConnection]";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string strSql;
	strSql = "UPDATE ImageDB SET Id = ? WHERE ImgId = ? ;";	

	BeginTransaction();

	try {
		rc = sqlite3_prepare_v2(pDB, strSql.c_str(), -1, &pStmt, NULL);
		sqlite3_reset(pStmt);

		int nIdx = 1;
		sqlite3_bind_text(pStmt, nIdx++, PatientID, -1, SQLITE_STATIC);
		sqlite3_bind_int(pStmt, nIdx++, ImgId);

		rc = sqlite3_step(pStmt);
		if (rc != SQLITE_DONE) qDebug() << __FUNCTION__ << "ERROR:" << m_dbSqlite.GetLatestError().c_str();
	} catch (std::exception& e) {
		qDebug() << __FUNCTION__ << "fail" << m_dbSqlite.GetLatestError().c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	if (rc == SQLITE_DONE) ret = true;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
std::string CDBUtils::IsExistPatientIDAndChartNum(QString strId, int &nCount, int chartnum, int flag)
{
	std::string strID; strID.clear();
	int rc = SQLITE_ERROR;
	
	if (!m_dbSqlite.CheckValidDBConnection(true)){
		qDebug() << "[CDBUtils] SearchImgage fail";
		////return rc;
		return strID;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();

	char cpQuery[1024] = { 0, };
	sprintf(cpQuery, "select rowid, Id, ChartNum from patient WHERE Id='%s' OR ChartNum='%s';", strId.toUtf8().toStdString().c_str(), strId.toUtf8().toStdString().c_str());

	try	{
		rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
		if (rc != SQLITE_OK){
			qDebug() << "ERROR :"<< m_dbSqlite.GetLatestError().c_str();
			CloseDB();
			////return rc;
			return strID;
		}

		rc = sqlite3_step(pStmt);
		while (rc == SQLITE_ROW){
			int nRowId = sqlite3_column_int(pStmt, ROWID);
			char id[120]; memset(id, 0x00, sizeof(id));
			if(chartnum==0) sprintf_s(id, "%s", sqlite3_column_text(pStmt, ID));
			else sprintf_s(id, "%s", sqlite3_column_text(pStmt, 2));
			strID = id;

			nCount += 1;

			rc = sqlite3_step(pStmt);
		}
	}
	catch (std::exception& e)	{
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
	}

	if(nCount==0 && flag==false){
		char cpQuery[1024] = { 0, };
		sprintf(cpQuery, "select rowid, Id from patientDelete WHERE Id='%s' OR ChartNum='%s';", strId.toUtf8().toStdString().c_str(), strId.toUtf8().toStdString().c_str());

		try {
			rc = sqlite3_prepare_v2(pDB, cpQuery, -1, &pStmt, NULL);
			if (rc != SQLITE_OK) {
				qDebug() << "ERROR :" << m_dbSqlite.GetLatestError().c_str();
				CloseDB();
				////return rc;
				return strID;
			}

			rc = sqlite3_step(pStmt);
			while (rc == SQLITE_ROW) {
				int nRowId = sqlite3_column_int(pStmt, ROWID);
				char id[120]; memset(id, 0x00, sizeof(id));
				if (chartnum == 0) sprintf_s(id, "%s", sqlite3_column_text(pStmt, ID));
				else sprintf_s(id, "%s", sqlite3_column_text(pStmt, 2));

				strID = id;

				nCount += 1;

				rc = sqlite3_step(pStmt);
			}
		}
		catch (std::exception& e) {
			std::string error_msg = m_dbSqlite.GetLatestError();
			qDebug() << "[CDBUtils] SearchPatient fail" << error_msg.c_str();
		}
	}

	sqlite3_finalize(pStmt);

	CloseDB();

	////return rc;
	return strID;
}

///////////////////////////////////////////////////////////////////////////////
int CDBUtils::CopyPatientTable(PATIENT_INFO &info)
{
	int ret = false;
	if (!m_dbSqlite.CheckValidDBConnection())
	{
		qDebug() << __FUNCTION__ <<"[CDBUtils] RecentPatient fail ";
		return ret;
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	char query[0x1000]; memset(query, 0x00, sizeof(query));	
	sprintf_s(query, sizeof(query), "SELECT rowid, * from patient WHERE Id='%s';", info.Id);

	BeginTransaction();

	try {
		rc = sqlite3_prepare_v2(pDB, query, -1, &pStmt, NULL);
		if (rc != SQLITE_OK) {
			CommitTransection();
			qDebug() << "ERROR: " << m_dbSqlite.GetLatestError().c_str();
			CloseDB();
			return ret;
		}

		rc = sqlite3_reset(pStmt);


		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW) {
			// all column
			info.RowId = sqlite3_column_int(pStmt, ROWID);
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, ID));
			sprintf_s(info.Name, "%s", sqlite3_column_text(pStmt, NAME));
			sprintf_s(info.FirstName, "%s", sqlite3_column_text(pStmt, FIRSTNAME));
			sprintf_s(info.LastName, "%s", sqlite3_column_text(pStmt, LASTNAME));
			sprintf_s(info.Birthdate, "%s", sqlite3_column_text(pStmt, BIRTHDATE));

			info.Gender = sqlite3_column_int(pStmt, GENDER);
			info.UpdateTime = sqlite3_column_int(pStmt, UPDATETIME);
			info.DeleteTime = sqlite3_column_int(pStmt, DELETETIME);
			info.Status = sqlite3_column_int(pStmt, STATUS);

			sprintf_s(info.PersonalNum, "%s", sqlite3_column_text(pStmt, PERSONALNUM));
			sprintf_s(info.AccessNum, "%s", sqlite3_column_text(pStmt, ACCESSNUM));
			sprintf_s(info.OtherId, "%s", sqlite3_column_text(pStmt, OTHERID));

			sprintf_s(info.Category, "%s", sqlite3_column_text(pStmt, CATEGORY));
			sprintf_s(info.Physician, "%s", sqlite3_column_text(pStmt, PHYSICIAN));
			sprintf_s(info.Description, "%s", sqlite3_column_text(pStmt, DESCRIPTION));

			sprintf_s(info.Email, "%s", sqlite3_column_text(pStmt, EMAIL));
			sprintf_s(info.Address, "%s", sqlite3_column_text(pStmt, HOME_ADDRESS));
			sprintf_s(info.ZipCode, "%s", sqlite3_column_text(pStmt, ZIPCODE));
			sprintf_s(info.Phone, "%s", sqlite3_column_text(pStmt, PHONE));
			sprintf_s(info.MobilePhone, "%s", sqlite3_column_text(pStmt, MOBILE));
			sprintf_s(info.ImageFileName, "%s", sqlite3_column_text(pStmt, PHOTO_NAME));
			sprintf_s(info.Reserve1, "%s", sqlite3_column_text(pStmt, RESERVED1));
			sprintf_s(info.Reserve2, "%s", sqlite3_column_text(pStmt, RESERVED2));
			//hjpark@20200609:ChartNum Add
			sprintf_s(info.ChartNum, "%s", sqlite3_column_text(pStmt, CHARTNUM));

			info.PX = sqlite3_column_int(pStmt, PX);
			info.CX = sqlite3_column_int(pStmt, CX);
			info.OX = sqlite3_column_int(pStmt, OX);
			info.OV = sqlite3_column_int(pStmt, OV);
			info.DC = sqlite3_column_int(pStmt, DC);
			info.CT = sqlite3_column_int(pStmt, CT);

			info.PX_UpdateTime = sqlite3_column_int(pStmt, PX_UPDATE);
			info.CX_UpdateTime = sqlite3_column_int(pStmt, CX_UPDATE);
			info.OX_UpdateTime = sqlite3_column_int(pStmt, OX_UPDATE);
			info.OV_UpdateTime = sqlite3_column_int(pStmt, OV_UPDATE);
			info.DC_UpdateTime = sqlite3_column_int(pStmt, DC_UPDATE);
			info.CT_UpdateTime = sqlite3_column_int(pStmt, CT_UPDATE);
			info.nInvisible = sqlite3_column_int(pStmt, INVISIBLE);

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e){
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] RecentPatient fail" << error_msg.c_str();
	}

	CommitTransection();

	sqlite3_finalize(pStmt);
	
	CloseDB();

	ret = true;
	return ret;
}
///////////////////////////////////////////////////////////////////////////////

QVector<IMG_DB_INFO> CDBUtils::SearchImageExclusiveDeleteImage(CQuery* pQuery, bool bCheckDB/*=true*/)
{
	QVector<IMG_DB_INFO> resultList;

	if (bCheckDB)
	{
		// DB 연결 끊어질 경우 대비
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] SearchImgage fail";
			return resultList;
		}
	}

	sqlite3		 *pDB = m_dbSqlite.GetDB();
	if (pDB == NULL)
	{
		if (!m_dbSqlite.CheckValidDBConnection(true))
		{
			qDebug() << "[CDBUtils] SearchImgage fail";
			return resultList;
		}
		pDB = m_dbSqlite.GetDB();
	}
	sqlite3_stmt *pStmt = m_dbSqlite.GetStatement();
	int rc = SQLITE_ERROR;

	string str = pQuery->GetQueryString();

	try {
		rc = sqlite3_prepare_v2(pDB, str.c_str(), -1, &pStmt, NULL);
		if (rc != SQLITE_OK)
		{
			qDebug() << "ERROR" << m_dbSqlite.GetLatestError().c_str();
			if (bCheckDB)
				CloseDB();
			return resultList;
		}

		rc = sqlite3_step(pStmt);
		int nCount = 0;
		while (rc == SQLITE_ROW)
		{
			IMG_DB_INFO info;
			memset(&info, 0x00, sizeof(IMG_DB_INFO));

			int nIdx = 1;

			char cpTemp[1024] = { 0, };

			sqlite3_column_int(pStmt, ROWID);

			info.ImgId = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Id, "%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.FileName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.ExposureDate = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.Modality, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImgFormat, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.MountIdx = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.ExposureBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strlen(cpTemp) >= 100)
				strcpy(info.Comment, CutUnicodeString(cpTemp, 99));
			else
				strcpy(info.Comment, cpTemp);

			info.ImgState = sqlite3_column_int(pStmt, nIdx++);
			info.DeleteTime = sqlite3_column_int(pStmt, nIdx++);
			sprintf_s(info.DeleteBy, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.Manufacturer, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ManufacturerModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			info.PixelSize = sqlite3_column_double(pStmt, nIdx++);
			info.BitPerPixel = sqlite3_column_double(pStmt, nIdx++);
			info.Width = sqlite3_column_int(pStmt, nIdx++);
			info.Height = sqlite3_column_int(pStmt, nIdx++);
			info.Voltage = sqlite3_column_double(pStmt, nIdx++);
			info.Current = sqlite3_column_double(pStmt, nIdx++);
			info.ExposureTime = sqlite3_column_double(pStmt, nIdx++);
			info.Dose = sqlite3_column_double(pStmt, nIdx++);
			info.Exposure = sqlite3_column_double(pStmt, nIdx++);
			info.NumberOfFrames = sqlite3_column_int(pStmt, nIdx++);

			//hjpark@20200603:add
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 100)
					strcpy(info.Diagnosis, CutUnicodeString(cpTemp, 99));
				else
					strcpy(info.Diagnosis, cpTemp);
			}

			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.Reading, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.Reading, cpTemp);
			}

			info.band = sqlite3_column_int(pStmt, nIdx++);
			info.bit = sqlite3_column_int(pStmt, nIdx++);
			info.pixelRepresentation = sqlite3_column_int(pStmt, nIdx++);
			info.min = sqlite3_column_int(pStmt, nIdx++);
			info.max = sqlite3_column_int(pStmt, nIdx++);
			info.bitAlloc = sqlite3_column_int(pStmt, nIdx++);
			info.bitStore = sqlite3_column_int(pStmt, nIdx++);
			info.highBit = sqlite3_column_int(pStmt, nIdx++);
			info.instanceNumber = sqlite3_column_int(pStmt, nIdx++);
			info.fileSize = sqlite3_column_int(pStmt, nIdx++);

			info.intercept = sqlite3_column_double(pStmt, nIdx++);
			info.slope = sqlite3_column_double(pStmt, nIdx++);
			info.SliceThickness = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeY = sqlite3_column_double(pStmt, nIdx++);
			info.pixelSizeX = sqlite3_column_double(pStmt, nIdx++);
			info.dpiY = sqlite3_column_double(pStmt, nIdx++);
			info.dpiX = sqlite3_column_double(pStmt, nIdx++);
			info.windowCenter = sqlite3_column_double(pStmt, nIdx++);
			info.windowWidth = sqlite3_column_double(pStmt, nIdx++);
			info.sliceLocation = sqlite3_column_double(pStmt, nIdx++);
			info.fluoroscopyAreaDoseProduct = sqlite3_column_double(pStmt, nIdx++);

			sprintf_s(info.photometricInterpretation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.patientPosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			//hjpark@20200907:overflow field size
			//sprintf_s(info.imagePosition, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			//sprintf_s(info.imageOrientation, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imagePosition, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imagePosition, cpTemp);
			}
			memset(cpTemp, 0x00, sizeof(char) * 1024);
			sprintf_s(cpTemp, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			if (strcmp(cpTemp, "(null)") != 0)
			{
				if (strlen(cpTemp) >= 60)
					strcpy(info.imageOrientation, CutUnicodeString(cpTemp, 59));
				else
					strcpy(info.imageOrientation, cpTemp);
			}
			
			sprintf_s(info.bodyPart, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceSerialNumber, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.deviceModelName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.imageType, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesDate, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			sprintf_s(info.institutionName, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.institutionAddr, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.studyInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.seriesInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.sopInstanceUid, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.TransferSyntaxUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));
			sprintf_s(info.ImplementationClassUID, u8"%s", sqlite3_column_text(pStmt, nIdx++));

			if(info.ImgState==0) resultList.push_back(info);

			rc = sqlite3_step(pStmt);
		}
	} catch (std::exception& e) {
		std::string error_msg = m_dbSqlite.GetLatestError();
		qDebug() << "[CDBUtils] SearchImgage fail" << error_msg.c_str();
	}

	sqlite3_finalize(pStmt);

	if (bCheckDB)
	{
		CloseDB();
	}

	return resultList;
}

