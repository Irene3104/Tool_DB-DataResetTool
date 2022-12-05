#include "DBreset.h"
#include "WebDBreset.h"
#include "SqliteDBreset.h"


DBreset::DBreset(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	setWindowFlags(Qt::Widget | Qt::MSWindowsFixedSizeDialogHint);

	m_TheiaWebDB = new TheiaWebDB;
	m_TheiaSqliteDB = new TheiaSqliteDB;

	QCoreApplication::setApplicationName("Theia DB & Data Reset Tool 1.0.0.0");
	setWindowTitle(QCoreApplication::applicationName());

	ui.radio_sqlite->setChecked(true);
	ui.radio_sqlite->clicked(true);

	ui.edit_serverPath->setText("");
	ui.edit_dataPath->setText("");

	ui.progressBar->setTextVisible(true);
	ui.progressBar->setFormat("Ready to Reset");
	ui.progressBar->setAlignment(Qt::AlignCenter);
	ui.progressBar->setValue(0);
	ui.progressBar->setMaximum(100);

	this->ConnectionSignalSlot();
}


DBreset::~DBreset()
{}

void DBreset::ConnectionSignalSlot()
{
	connect(ui.radio_sqlite, &QRadioButton::clicked, this, &DBreset::RadioSetting);
	connect(ui.radio_web, &QPushButton::clicked, this, &DBreset::RadioSetting);
	connect(ui.btn_serverPath, &QPushButton::clicked, this, &DBreset::BtnClicked_DirPath);
	connect(ui.btn_dataPath, &QPushButton::clicked, this, &DBreset::BtnClicked_DirPath);
	connect(ui.btn_clear, &QPushButton::clicked, this, &DBreset::DbClear);
	connect(this->m_TheiaWebDB, static_cast<void(TheiaWebDB::*)(QString, int)>(&TheiaWebDB::UpdateStep), this, &DBreset::ViewProgress);
	connect(this->m_TheiaSqliteDB, static_cast<void(TheiaSqliteDB::*)(QString, int)>(&TheiaSqliteDB::UpdateStep), this, &DBreset::ViewProgress);
}

void DBreset::RadioSetting()
{
	auto radio = qobject_cast<QRadioButton*>(sender());
	if (radio == ui.radio_sqlite)
	{
		Uiset(true);
	}
	else if (radio == ui.radio_web)
	{
		Uiset(false);
		ui.btn_clear->setEnabled(true);
		ui.btn_dataPath->setEnabled(true);
		ui.edit_dataPath->setEnabled(true);
	}
}

void DBreset::BtnClicked_DirPath()
{
	auto button = qobject_cast<QPushButton*>(sender());
	
	ui.textBrowser->setText("");

	QLineEdit* pEdit = nullptr;

	if (button == ui.btn_serverPath)
	{
		pEdit = ui.edit_serverPath;
	}
	else if (button == ui.btn_dataPath)
	{
		pEdit = ui.edit_dataPath;
	}
	if (pEdit != nullptr)
		pEdit->setText(GetPaths(pEdit->text()));
}

void DBreset::DbClear()
{
	ui.textBrowser->setText("");
	bool isSuccess = false;
	QString resultText;
	QString strMsgBox;
	QString DBpath = ui.edit_serverPath->text();
	QString DataPath = ui.edit_dataPath->text();

	if (ui.radio_sqlite->isChecked())
	{
		if (DBpath.isEmpty() || DataPath.isEmpty())
		{
			QMessageBox msgBox(QMessageBox::Warning, "Warning", "Please input the folder path", QMessageBox::Ok);
			msgBox.exec();

		}
		else
		{
			if (!QDir(DBpath).exists() || !QDir(DataPath).exists())
			{

				QMessageBox msgBox(QMessageBox::Warning, "Warning", "Not exists the folder. Please check right path.", QMessageBox::Ok);
				msgBox.exec();
			}
			else
			{
				isSuccess = this->m_TheiaSqliteDB->run(&resultText, ui.edit_serverPath->text(), ui.edit_dataPath->text());
				strMsgBox = (isSuccess == true) ? SucText : FailText;
				ui.textBrowser->setText(resultText);
				QMessageBox msgBox(QMessageBox::Information, "Information", strMsgBox + " to DB reset", QMessageBox::Ok);
				msgBox.exec();
			}
		}
	}
	else if (ui.radio_web->isChecked())
	{
		if (DataPath.isEmpty())
		{
			QMessageBox msgBox(QMessageBox::Warning, "Warning", "Please input the folder path", QMessageBox::Ok);
			msgBox.exec();

		}
		else {
			if (!QDir(DataPath).exists())
			{
				QMessageBox msgBox(QMessageBox::Warning, "Warning", "Not exists the folder. Please check right path.", QMessageBox::Ok);
				msgBox.exec();
			}
			else 
			{
				isSuccess = this->m_TheiaWebDB->run(&resultText, ui.edit_dataPath->text());
				strMsgBox = (isSuccess == true) ? SucText : FailText;
				ui.textBrowser->setText(resultText);
				QMessageBox msgBox(QMessageBox::Information, "Information", strMsgBox + " to DB reset", QMessageBox::Ok);
				msgBox.exec();
			}
			
		}
	}

	ui.progressBar->setValue(0);
	ui.progressBar->resetFormat();
	ui.edit_serverPath->setText("");
	ui.edit_dataPath->setText("");
}


QString DBreset::GetPaths(QString pathLine)
{
	QString inputPath = "";
	QDir selectedDir(pathLine);

	if (pathLine.isEmpty())
	{
		inputPath = QFileDialog::getExistingDirectory(NULL, ("Select Folder"), QApplication::applicationDirPath());
		return inputPath;
	}
	else
	{
		if (selectedDir.exists())
		{
			inputPath = QFileDialog::getExistingDirectory(NULL, ("Select Folder"), pathLine);
			if (inputPath == false)
				return pathLine;
			else
				return inputPath;
		}
		else
		{
			inputPath = QFileDialog::getExistingDirectory(NULL, ("Select Folder"), QApplication::applicationDirPath());
			return inputPath;
		}
	}
}

void DBreset::ViewProgress(QString text, int num)
{
	ui.progressBar->setFormat(text);
	ui.progressBar->setValue(num);
}


void DBreset::Uiset(bool result)
{
	ui.textBrowser->clear();
	ui.edit_serverPath->setText("");
	ui.edit_dataPath->setText("");
	ui.edit_serverPath->setEnabled(result);
	ui.btn_serverPath->setEnabled(result);
	ui.edit_dataPath->setEnabled(result);
	ui.btn_dataPath->setEnabled(result);
	ui.btn_clear->setEnabled(result);
}