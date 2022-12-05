#pragma once

#include <QtWidgets/QMainWindow>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QPushButton>
#include <QVariant>
#include <QDebug>
#include "qdebug.h"
#include "ui_DBreset.h"
#include "qpushbutton.h"
#include "qmessagebox.h"
#include "qfiledialog.h"

class TheiaWebDB;
class TheiaSqliteDB;

class DBreset : public QMainWindow
{
    Q_OBJECT

public:
    DBreset(QWidget *parent = nullptr);
    ~DBreset();

private:
	void ConnectionSignalSlot();
	void RadioSetting();
	void BtnClicked_DirPath();
	void DbClear();
	void ViewProgress(QString text, int num);
	void Uiset(bool result);
	QString GetPaths(QString pathLine);

private:
	Ui::DBresetClass ui;	
	   	  
	TheiaWebDB* m_TheiaWebDB = nullptr;
	TheiaSqliteDB* m_TheiaSqliteDB = nullptr;
};
