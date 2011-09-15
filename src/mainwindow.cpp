#include <qglobal.h>
#include <QtGui>
//#include <QtNetwork/QNetworkAccessManager>
#include "mainwindow.h"
#include "imagewindow.h"
#include "canm2d.h"
#include "optiondialog.h"
#include "util.h"
#include "colorpickerform.h"

#define FILE_EXT_ANM2D_XML	".xml"
#define FILE_EXT_ANM2D_BIN	".anm2"
#define FILE_EXT_JSON		".json"

#define kVersion	"1.1.0"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
	m_pSubWindow_Anm = NULL ;
	m_pSubWindow_Img = NULL ;
	m_pSubWindow_Loupe = NULL ;

	m_pMdiArea = new CDropableMdiArea ;
	m_pMdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	m_pMdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	setCentralWidget(m_pMdiArea);

	m_pTimer = new QTimer(this) ;
	m_pTimer->start(10*1000);

	createActions() ;
	createMenus() ;

	readRootSetting() ;

	setWindowTitle(tr("Animation Creator"));
	setUnifiedTitleAndToolBarOnMac(true);

	connect(m_pMdiArea, SIGNAL(dropFiles(QString)), this, SLOT(slot_dropFiles(QString))) ;
	connect(m_pTimer, SIGNAL(timeout()), this, SLOT(slot_checkFileModified())) ;
	QUndoStack *pUndoStack = m_EditData.getUndoStack() ;
	connect(pUndoStack, SIGNAL(indexChanged(int)), this, SLOT(slot_checkDataModified(int))) ;

	m_UndoIndex = 0 ;
	m_bSaveImage = false ;

	m_pImageWindow = NULL ;
	m_pLoupeWindow = NULL ;
	m_pAnimationForm = NULL ;
	m_pExportPNGForm = NULL ;

	m_bCtrl = false ;

	setObjectName("AnimationCreator MainWindow");
/*
	QNetworkAccessManager *pManager = new QNetworkAccessManager(this) ;
	connect(pManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(slot_reqFinished(QNetworkReply *))) ;
	pManager->get(QNetworkRequest(QUrl("http://chocobowl.biz:8800/AnimationCreator/version.txt"))) ;
*/
}

MainWindow::~MainWindow()
{
	delete m_pMdiArea ;
}

// ウィンドウ閉じイベント
void MainWindow::closeEvent(QCloseEvent *event)
{
	printf( "closeEvent\n" ) ;
	if ( !checkChangedFileSave() ) {
		event->ignore();
		return ;
	}

	m_pMdiArea->closeAllSubWindows();
	if ( m_pMdiArea->currentSubWindow() ) {
		event->ignore() ;
	}
	else {
		writeRootSetting() ;
		event->accept() ;
	}
}

// キー押しイベント
void MainWindow::keyPressEvent(QKeyEvent *event)
{
	if ( event->key() == Qt::Key_Control ) {
		m_bCtrl = true ;
	}
	if ( m_bCtrl ) {
		if ( event->key() == Qt::Key_L ) {
			m_pLoupeWindow->toggleLock() ;
		}
	}
}

// キー放しイベント
void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
	if ( event->key() == Qt::Key_Control ) {
		m_bCtrl = false ;
	}
}


// ファイルオープン
void MainWindow::slot_open( void )
{
	if ( !checkChangedFileSave() ) {
		// キャンセルした
		return ;
	}

	QString fileName = QFileDialog::getOpenFileName(
											this,
											tr("Open File"),
											setting.getOpenDir(),
											tr("All Files (*);;Image Files (*.png *.bmp *.jpg);;Text (*"FILE_EXT_ANM2D_XML");;Bin (*"FILE_EXT_ANM2D_BIN")")) ;
	if ( fileName.isEmpty() ) {
		return ;
	}

	fileOpen(fileName) ;
}

// 上書き保存
void MainWindow::slot_save( void )
{
	if ( m_StrSaveFileName.isEmpty() ) {
		slot_saveAs();
	}
	else {
		saveFile(m_StrSaveFileName) ;
	}
}

// 名前を付けて保存
void MainWindow::slot_saveAs( void )
{
	QString str = QFileDialog::getSaveFileName(this,
											   trUtf8("名前を付けて保存"),
											   setting.getSaveDir(),
											   tr("Text (*"FILE_EXT_ANM2D_XML");;Bin (*"FILE_EXT_ANM2D_BIN")")) ;
	if ( str.isEmpty() ) { return ; }

	if ( saveFile(str) ) {
		m_StrSaveFileName = str ;

		QString tmp(str) ;
		int index = tmp.lastIndexOf( '/' ) ;
		if ( index < 0 ) { return ; }

		tmp.remove( index + 1, tmp.size() ) ;
		setting.setSaveDir(tmp);
	}
}

// ファイルドロップされたら
void MainWindow::slot_dropFiles(QString fileName)
{
	if ( !checkChangedFileSave() ) {
		// キャンセルした
		return ;
	}

	fileOpen(fileName) ;
}

// 読み込んでるイメージデータの最終更新日時をチェック
void MainWindow::slot_checkFileModified( void )
{
	for ( int i = 0 ; i < m_EditData.getImageDataListSize() ; i ++ ) {
#if 1
		CEditData::ImageData *p = m_EditData.getImageData(i) ;

		QString fullPath = p->fileName ;
		QFileInfo info(fullPath) ;
		if ( !info.isFile() ) { continue ; }
		if ( !info.lastModified().isValid() ) { continue ; }
		if ( info.lastModified().toUTC() <= p->lastModified ) { continue ; }

		QDateTime time = info.lastModified().toUTC() ;
		p->lastModified = time ;

		QMessageBox::StandardButton reply = QMessageBox::question(this,
																  trUtf8("質問"),
																  trUtf8("%1が更新されています。読み込みますか？").arg(info.fileName()),
																  QMessageBox::Yes | QMessageBox::No) ;
		if ( reply != QMessageBox::Yes ) {
			continue ;
		}

		QImage image ;
		if ( !image.load(fullPath) ) {
			QMessageBox::warning(this, trUtf8("エラー"), trUtf8("読み込みに失敗しました:%1").arg(info.fileName())) ;
			continue ;
		}
		p->origImageW = image.width() ;
		p->origImageH = image.height() ;
		util::resizeImage(image) ;
		p->Image = image ;
		emit sig_modifiedImageFile(i) ;
#else
		QString fullPath = m_EditData.getImageFileName(i) ;
		QFileInfo info(fullPath) ;
		if ( !info.isFile() ) { continue ; }
		if ( !info.lastModified().isValid() ) { continue ; }
		if ( info.lastModified().toUTC() <= m_EditData.getImageDataLastModified(i) ) { continue ; }

		QDateTime time = info.lastModified().toUTC() ;
		m_EditData.setImageDataLastModified(i, time);

		QMessageBox::StandardButton reply = QMessageBox::question(this,
																  trUtf8("質問"),
																  trUtf8("%1が更新されています。読み込みますか？").arg(info.fileName()),
																  QMessageBox::Yes | QMessageBox::No) ;
		if ( reply != QMessageBox::Yes ) {
			continue ;
		}

		QImage image ;
		if ( !image.load(fullPath) ) {
			QMessageBox::warning(this, trUtf8("エラー"), trUtf8("読み込みに失敗しました:%1").arg(info.fileName())) ;
			continue ;
		}
		m_EditData.setImage(i, image) ;
		emit sig_modifiedImageFile(i) ;
#endif
	}
}

// データを編集したか
void MainWindow::slot_checkDataModified(int index)
{
	if ( m_UndoIndex == index ) {
		setWindowTitle(tr("Animation Creator[%1]").arg(m_StrSaveFileName));
	}
	else {	// 編集してる
		setWindowTitle(tr("Animation Creator[%1]*").arg(m_StrSaveFileName));
	}
}

// ヘルプ選択時
void MainWindow::slot_help( void )
{
	HelpWindow *p = new HelpWindow ;
	if ( !p->isLoaded() ) {
		delete p ;
	}
}

// イメージウィンドウOn/Off
void MainWindow::slot_triggeredImageWindow( bool flag )
{
	if ( m_pImageWindow ) {
		m_pImageWindow->setVisible(flag);
	}
}

// ルーペウィンドウOn/Off
void MainWindow::slot_triggeredLoupeWindow( bool flag )
{
	if ( m_pLoupeWindow ) {
		m_pLoupeWindow->setVisible(flag);
	}
}

// アニメーションウィンドウOn/Off
void MainWindow::slot_triggeredAnimeWindow( bool flag )
{
	if ( m_pAnimationForm ) {
		m_pAnimationForm->setVisible(flag);
	}
}

// オプション選択時
void MainWindow::slot_option( void )
{
	OptionDialog dialog(&setting, this) ;
	dialog.exec() ;

	emit sig_endedOption() ;
}

// 連番PNG保存
void MainWindow::slot_exportPNG( void )
{
	if ( !m_pMdiArea->findChild<ExportPNGForm *>(trUtf8("ExportPNGForm")) ) {
		if ( !m_pMdiArea->findChild<AnimationForm *>(trUtf8("AnimationForm")) ) {
			QMessageBox::warning(this, trUtf8("エラー"), trUtf8("アニメーションウィンドウがありません")) ;
			return ;
		}
		m_pExportPNGForm = new ExportPNGForm(&m_EditData, &setting, this) ;
		m_pExpngSubWindow = m_pMdiArea->addSubWindow( m_pExportPNGForm ) ;
		m_pExportPNGForm->show() ;
		connect(m_pAnimationForm->getGLWidget(), SIGNAL(sig_exportPNGRectChange()), m_pExportPNGForm, SLOT(slot_changeRect())) ;
		connect(m_pExportPNGForm, SIGNAL(sig_changeRect()), m_pAnimationForm->getGLWidget(), SLOT(update())) ;
		connect(m_pExportPNGForm, SIGNAL(sig_startExport()), m_pAnimationForm, SLOT(slot_playAnimation())) ;
		connect(m_pExportPNGForm, SIGNAL(sig_cancel()), this, SLOT(slot_closeExportPNGForm())) ;

		m_pAnimationForm->getGLWidget()->update();
	}
}

void MainWindow::slot_closeExportPNGForm( void )
{
	m_pMdiArea->removeSubWindow(m_pExpngSubWindow);
	delete m_pExportPNGForm ;
	m_pExportPNGForm = NULL ;
	m_pExpngSubWindow = NULL ;
}

void MainWindow::slot_portCheckDrawCenter(bool flag)
{
	emit sig_portCheckDrawCenter(flag) ;
}

void MainWindow::slot_portDragedImage(CObjectModel::FrameData data)
{
	emit sig_portDragedImage(data) ;
}

void MainWindow::slot_pushColorToolButton( void )
{
	if ( !m_pMdiArea->findChild<ColorPickerForm *>(trUtf8("ColorPickerForm")) ) {
		ColorPickerForm *p = new ColorPickerForm(&m_EditData, m_pMdiArea) ;
		m_pMdiArea->addSubWindow( p ) ;
		p->show();
		connect(p, SIGNAL(sig_setColorToFrameData(QRgb)), m_pAnimationForm, SLOT(slot_setColorFromPicker(QRgb))) ;
	}
}

void MainWindow::slot_destroyAnmWindow( void )
{
	if ( m_pSubWindow_Anm ) {
		qDebug("destroyAnmWindow save setting");
		setting.setAnmWindowPos(m_pSubWindow_Anm->pos());
		setting.setAnmWindowSize(m_pSubWindow_Anm->size());
	}
	m_pSubWindow_Anm = NULL ;
}

void MainWindow::slot_destroyImgWindow( void )
{
	if ( m_pSubWindow_Img ) {
		qDebug("destroyImgWindow save setting");
		setting.setImgWindowPos(m_pSubWindow_Img->pos());
		setting.setImgWindowSize(m_pSubWindow_Img->size());
	}
	m_pSubWindow_Img = NULL ;
}

void MainWindow::slot_destroyLoupeWindow( void )
{
	if ( m_pSubWindow_Loupe ) {
		qDebug("destroyLoupeWindow save setting");
		setting.setLoupeWindowPos(m_pSubWindow_Loupe->pos());
		setting.setLoupeWindowSize(m_pSubWindow_Loupe->size());
	}
	m_pSubWindow_Loupe = NULL ;
}

void MainWindow::slot_reqFinished(QNetworkReply *reply)
{
	if ( reply->error() ) {
		qDebug() << "request error:" << reply->errorString() ;
		return ;
	}

	QString str = reply->readAll() ;
	qDebug() << "version:" << str ;
	if ( str == kVersion ) {
		return ;
	}
}

// JSON 吐き出し
void MainWindow::slot_exportJSON()
{
	if ( m_StrSaveFileName.isEmpty() ) {
		QMessageBox::warning(this, trUtf8("エラー"), trUtf8("1度保存してください")) ;
		return ;
	}
	int extPos = m_StrSaveFileName.lastIndexOf(".") ;
	if ( extPos < 0 ) {
		QMessageBox::warning(this, trUtf8("エラー"), trUtf8("1度保存してください")) ;
		return ;
	}
	QString fileName = m_StrSaveFileName.left(extPos) + ".json" ;

	CAnm2DJson data ;
	if ( !data.makeFromEditData(m_EditData) ) {
		if ( data.getErrorNo() != CAnm2DBase::kErrorNo_Cancel ) {
			QMessageBox::warning(this, trUtf8("エラー"), trUtf8("コンバート失敗 %1:\n%2").arg(fileName).arg(data.getErrorNo())) ;
		}
		return ;
	}

	QFile file(fileName) ;
	if ( !file.open(QFile::WriteOnly) ) {
		QMessageBox::warning(this, trUtf8("エラー"), trUtf8("保存失敗 %1:\n%2").arg(fileName).arg(file.errorString())) ;
		return ;
	}
	file.write(data.getData().toAscii()) ;

	QMessageBox msgBox ;
	msgBox.setText(trUtf8("JSON吐き出し終了:")+fileName) ;
	msgBox.exec() ;
}

#ifndef QT_NO_DEBUG
void MainWindow::slot_dbgObjectDump( void )
{
	if ( !m_pAnimationForm ) { return ; }

	m_pAnimationForm->dbgDumpObject() ;
}
#endif
// 設定を復元
void MainWindow::readRootSetting( void )
{
#if 1
	QSettings settings(qApp->applicationDirPath() + "/settnig.ini", QSettings::IniFormat) ;
	qDebug() << "readRootSetting\n" << settings.allKeys() ;
	qDebug() << "file:" << qApp->applicationDirPath() + "/settnig.ini" ;
#else
	QSettings settings("Editor", "rootSettings") ;
#endif
	settings.beginGroup("Global");
#if defined(Q_OS_WIN32)
	QString dir = settings.value("cur_dir", QString(".\\")).toString() ;
	QString save_dir = settings.value("save_dir", dir).toString() ;
	QString png_dir = settings.value("png_dir", dir).toString() ;
#elif defined(Q_OS_MAC)
	QString dir = settings.value("cur_dir", QString("/Users/")).toString() ;
	QString save_dir = settings.value("save_dir", QString("/Users/")).toString() ;
	QString png_dir = settings.value("png_dir", QString("/Users/")).toString() ;
#elif defined(Q_OS_LINUX)
	QString dir = settings.value("cur_dir", QString("/home/")).toString() ;
	QString save_dir = settings.value("save_dir", QString("/home/")).toString() ;
	QString png_dir = settings.value("png_dir", QString("/home/")).toString() ;
#else
	#error OSが定義されてないよ
#endif
	QRgb col ;
	col = settings.value("anime_color", 0).toUInt() ;
	QColor animeCol = QColor(qRed(col), qGreen(col), qBlue(col), qAlpha(col)) ;
	col = settings.value("image_color", 0).toUInt() ;
	QColor imageCol = QColor(qRed(col), qGreen(col), qBlue(col), qAlpha(col)) ;
	bool bSaveImage = settings.value("save_image", false).toBool() ;
	settings.endGroup();

	settings.beginGroup("MainWindow");
	QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint() ;
	QSize size = settings.value("size", QSize(400, 400)).toSize() ;
	settings.endGroup();

	settings.beginGroup("AnimationWindow");
	QPoint aw_pos = settings.value("pos", QPoint(-1, -1)).toPoint() ;
	QSize aw_size = settings.value("size", QSize(-1, -1)).toSize() ;
	bool bBackImage = settings.value("use_back_image", false).toBool() ;
	QString strBackImage = settings.value("back_image", "").toString() ;
	bool bFrame = settings.value("disp_frame", true).toBool() ;
	bool bCenter = settings.value("disp_center", false).toBool() ;
	int treeWidth = settings.value("tree_width", -1).toInt() ;
	int treeWidthIdx = settings.value("tree_width_idx", -1).toInt() ;
	settings.endGroup();

	settings.beginGroup("ImageWindow");
	QPoint img_pos = settings.value("pos", QPoint(-1, -1)).toPoint() ;
	QSize img_size = settings.value("size", QSize(-1, -1)).toSize() ;
	settings.endGroup();

	settings.beginGroup("LoupeWindow");
	QPoint loupe_pos = settings.value("pos", QPoint(-1, -1)).toPoint() ;
	QSize loupe_size = settings.value("size", QSize(-1, -1)).toSize() ;
	settings.endGroup();

	move(pos) ;
	resize(size) ;
	setting.setOpenDir(dir) ;
	setting.setSaveDir(save_dir);
	setting.setSavePngDir(png_dir);
	setting.setAnimeBGColor(animeCol);
	setting.setImageBGColor(imageCol);
	setting.setSaveImage(bSaveImage);
	setting.setBackImagePath(strBackImage) ;
	setting.setUseBackImage(bBackImage) ;
	setting.setDrawFrame(bFrame) ;
	setting.setDrawCenter(bCenter) ;

	setting.setAnmWindowPos(aw_pos) ;
	setting.setAnmWindowSize(aw_size) ;

	setting.setImgWindowPos(img_pos) ;
	setting.setImgWindowSize(img_size) ;

	setting.setLoupeWindowPos(loupe_pos) ;
	setting.setLoupeWindowSize(loupe_size) ;

	setting.setAnmWindowTreeWidth(treeWidth, treeWidthIdx) ;
}

// 設定を保存
void MainWindow::writeRootSetting( void )
{
#if 1
	QSettings settings(qApp->applicationDirPath() + "/settnig.ini", QSettings::IniFormat) ;
	qDebug() << "writeRootSetting writable:" << settings.isWritable() ;
	qDebug() << "file:" << qApp->applicationDirPath() + "/settnig.ini" ;
#else
	QSettings settings("Editor", "rootSettings") ;
#endif
	settings.beginGroup("Global");
	settings.setValue("cur_dir",		setting.getOpenDir()) ;
	settings.setValue("save_dir",		setting.getSaveDir()) ;
	settings.setValue("png_dir",		setting.getSavePngDir()) ;
	settings.setValue("anime_color",	setting.getAnimeBGColor().rgba());
	settings.setValue("image_color",	setting.getImageBGColor().rgba());
	settings.setValue("save_image",		setting.getSaveImage());
	settings.endGroup();

	settings.beginGroup("MainWindow");
	settings.setValue("pos",	pos()) ;
	settings.setValue("size",	size()) ;
	settings.endGroup();

	QPoint pos ;
	QSize size ;

	if ( m_pSubWindow_Anm ) {
		pos = m_pSubWindow_Anm->pos() ;
		size = m_pSubWindow_Anm->size() ;
	}
	else {
		pos = setting.getAnmWindowPos() ;
		size = setting.getAnmWindowSize() ;
	}

	settings.beginGroup("AnimationWindow");
	settings.setValue("pos",			pos) ;
	settings.setValue("size",			size) ;
	settings.setValue("use_back_image",	setting.getUseBackImage());
	settings.setValue("back_image",		setting.getBackImagePath());
	settings.setValue("disp_frame",		setting.getDrawFrame());
	settings.setValue("disp_center",	setting.getDrawCenter()) ;
	settings.setValue("tree_width",		setting.getAnmWindowTreeWidth());
	settings.setValue("tree_width_idx",	setting.getAnmWindowTreeWidthIndex());
	settings.endGroup();

	if ( m_pSubWindow_Img ) {
		pos = m_pSubWindow_Img->pos() ;
		size = m_pSubWindow_Img->size() ;
	}
	else {
		pos = setting.getImgWindowPos() ;
		size = setting.getImgWindowSize() ;
	}
	settings.beginGroup("ImageWindow");
	settings.setValue("pos",			pos) ;
	settings.setValue("size",			size) ;
	settings.endGroup();

	if ( m_pSubWindow_Loupe ) {
		pos = m_pSubWindow_Loupe->pos() ;
		size = m_pSubWindow_Loupe->size() ;
	}
	else {
		pos = setting.getLoupeWindowPos() ;
		size = setting.getLoupeWindowSize() ;
	}
	settings.beginGroup("LoupeWindow");
	settings.setValue("pos",			pos) ;
	settings.setValue("size",			size) ;
	settings.endGroup();
}

// アクションを作成
void MainWindow::createActions( void )
{
	// 開く
	m_pActOpen = new QAction(trUtf8("&Open..."), this) ;
	m_pActOpen->setShortcuts(QKeySequence::Open) ;
	m_pActOpen->setStatusTip(trUtf8("ファイルを開きます")) ;
	connect(m_pActOpen, SIGNAL(triggered()), this, SLOT(slot_open())) ;

	// 保存
	m_pActSave = new QAction(trUtf8("&Save"), this) ;
	m_pActSave->setShortcuts(QKeySequence::Save) ;
	m_pActSave->setStatusTip(trUtf8("ファイルを保存します")) ;
	connect(m_pActSave, SIGNAL(triggered()), this, SLOT(slot_save())) ;

	// 名前を付けて保存
	m_pActSaveAs = new QAction(trUtf8("Save &As..."), this) ;
	m_pActSaveAs->setShortcuts(QKeySequence::SaveAs) ;
	m_pActSaveAs->setStatusTip(trUtf8("ファイルを保存します")) ;
	connect(m_pActSaveAs, SIGNAL(triggered()), this, SLOT(slot_saveAs())) ;

	// 連番PNG保存
	m_pActExportPNG = new QAction(trUtf8("連番PNG"), this) ;
	connect(m_pActExportPNG, SIGNAL(triggered()), this, SLOT(slot_exportPNG())) ;

	// JSON吐き出し
	m_pActExportJson = new QAction(trUtf8("JSON"), this) ;
	connect(m_pActExportJson, SIGNAL(triggered()), this, SLOT(slot_exportJSON())) ;

	// 終了
	m_pActExit = new QAction(trUtf8("E&xit"), this) ;
	m_pActExit->setShortcuts(QKeySequence::Quit);
	m_pActExit->setStatusTip(trUtf8("アプリケーションを終了します"));
	connect(m_pActExit, SIGNAL(triggered()), this, SLOT(close())) ;

	// 戻す
	QUndoStack *pStack = m_EditData.getUndoStack() ;
	m_pActUndo = pStack->createUndoAction(this, trUtf8("&Undo")) ;
	m_pActUndo->setShortcuts(QKeySequence::Undo);

	// やり直す
	m_pActRedo = pStack->createRedoAction(this, trUtf8("&Redo")) ;
	m_pActRedo->setShortcuts(QKeySequence::Redo);

	// イメージウィンドウon/off
	m_pActImageWindow = new QAction(trUtf8("Image Window"), this) ;
	m_pActImageWindow->setEnabled(false);
	m_pActImageWindow->setCheckable(true);
	connect(m_pActImageWindow, SIGNAL(triggered(bool)), this, SLOT(slot_triggeredImageWindow(bool))) ;

	// ルーペウィンドウon/off
	m_pActLoupeWindow = new QAction(trUtf8("Loupe Window"), this) ;
	m_pActLoupeWindow->setEnabled(false);
	m_pActLoupeWindow->setCheckable(true);
	connect(m_pActLoupeWindow, SIGNAL(triggered(bool)), this, SLOT(slot_triggeredLoupeWindow(bool))) ;

	// アニメーションウィンドウon/off
	m_pActAnimeWindow = new QAction(trUtf8("Animeation Window"), this) ;
	m_pActAnimeWindow->setEnabled(false);
	m_pActAnimeWindow->setCheckable(true);
	connect(m_pActAnimeWindow, SIGNAL(triggered(bool)), this, SLOT(slot_triggeredAnimeWindow(bool))) ;

	// オプション
	m_pActOption = new QAction(trUtf8("&Option"), this) ;
	connect(m_pActOption, SIGNAL(triggered()), this, SLOT(slot_option())) ;

	// ヘルプ
	m_pActHelp = new QAction(trUtf8("&Help"), this) ;
	m_pActHelp->setShortcuts(QKeySequence::HelpContents) ;
	connect(m_pActHelp, SIGNAL(triggered()), this, SLOT(slot_help())) ;

	// Qtについて
	m_pActAboutQt = new QAction(trUtf8("About &Qt"), this) ;
	connect(m_pActAboutQt, SIGNAL(triggered()), qApp, SLOT(aboutQt())) ;

#ifndef QT_NO_DEBUG
	m_pActDbgDump = new QAction(tr("Dump"), this) ;
	connect(m_pActDbgDump, SIGNAL(triggered()), this, SLOT(slot_dbgObjectDump())) ;
#endif
}

// メニューを作成
void MainWindow::createMenus( void )
{
	QMenu *pMenu ;

	pMenu = menuBar()->addMenu(trUtf8("&File")) ;
	pMenu->addAction(m_pActOpen) ;
	pMenu->addAction(m_pActSave) ;
	pMenu->addAction(m_pActSaveAs) ;
	pMenu->addSeparator() ;
	{
		QMenu *p = pMenu->addMenu(trUtf8("Export")) ;
		p->addAction(m_pActExportPNG) ;
		p->addAction(m_pActExportJson) ;
	}
	pMenu->addSeparator() ;
	pMenu->addAction(m_pActExit) ;

	pMenu = menuBar()->addMenu(trUtf8("&Edit")) ;
	pMenu->addAction(m_pActUndo) ;
	pMenu->addAction(m_pActRedo) ;
#if 0
	pMenu = menuBar()->addMenu(trUtf8("&Window")) ;
	pMenu->addAction(m_pActImageWindow) ;
	pMenu->addAction(m_pActLoupeWindow) ;
	pMenu->addAction(m_pActAnimeWindow) ;
#endif
	pMenu = menuBar()->addMenu(trUtf8("Too&ls")) ;
	pMenu->addAction(m_pActOption) ;

	pMenu = menuBar()->addMenu(trUtf8("&Help")) ;
	pMenu->addAction(m_pActHelp) ;
	pMenu->addAction(m_pActAboutQt) ;

#ifndef QT_NO_DEBUG
	pMenu = menuBar()->addMenu(tr("Debug")) ;
	pMenu->addAction(m_pActDbgDump) ;
#endif
}

// 「開く」での現在のディレクトリを設定
void MainWindow::setCurrentDir( QString &fileName )
{
	QString tmp(fileName) ;
	int index = tmp.lastIndexOf( '/' ) ;
	if ( index < 0 ) { return ; }

	tmp.remove( index + 1, tmp.size() ) ;
	setting.setOpenDir(tmp) ;
}

// ウィンドウ達を作成
void MainWindow::createWindows( void )
{
	makeAnimeWindow() ;
	makeImageWindow() ;
	makeLoupeWindow() ;
}

// imageDataのサイズを2の累乗に修正
void MainWindow::resizeImage( QImage &imageData )
{
	util::resizeImage(imageData) ;
}

// ファイル開く
bool MainWindow::fileOpen( QString fileName )
{
	setCurrentDir( fileName ) ;		// カレントディレクトリを設定

	if ( fileName.toLower().indexOf(".png")		<= 0
	  && fileName.toLower().indexOf(".bmp")		<= 0
	  && fileName.toLower().indexOf(".jpg")		<= 0
	  && fileName.indexOf(FILE_EXT_ANM2D_XML)	<= 0
	  && fileName.indexOf(FILE_EXT_ANM2D_BIN)	<= 0 ) {
		QMessageBox::warning(this, tr("warning"), trUtf8("対応していないファイルです") ) ;
		return false ;
	}

	if ( m_pMdiArea->currentSubWindow() ) {
		slot_destroyAnmWindow();
		slot_destroyImgWindow();
		slot_destroyLoupeWindow();
		m_pMdiArea->closeAllSubWindows() ;	// 全部閉じる
	}
	m_EditData.resetData();
	m_UndoIndex = 0 ;

	// XML アニメファイル
	if ( fileName.indexOf(FILE_EXT_ANM2D_XML) > 0 ) {
		CAnm2DXml data(setting.getSaveImage()) ;

		QFile file(fileName) ;
		if ( !file.open(QFile::ReadOnly) ) {
			QMessageBox::warning(this, trUtf8("エラー 0"), trUtf8("読み込みに失敗しました:%1").arg(fileName)) ;
			return false ;
		}
		QDomDocument xml ;
		xml.setContent(&file) ;
		data.setFilePath(fileName);
		if ( !data.makeFromFile(xml, m_EditData) ) {
			QMessageBox::warning(this, trUtf8("エラー 1"), trUtf8("読み込みに失敗しました:%1").arg(data.getErrorString()) )  ;
			return false ;
		}
		m_StrSaveFileName = fileName ;
	}
	// バイナリ アニメファイル
	else if ( fileName.indexOf(FILE_EXT_ANM2D_BIN) > 0 ) {
		CAnm2DBin data ;
		QByteArray dataArray ;

		QFile file(fileName) ;
		if ( !file.open(QFile::ReadOnly) ) {
			QMessageBox::warning(this, trUtf8("エラー 0"), trUtf8("読み込みに失敗しました:%1").arg(fileName)) ;
			return false ;
		}
		QDataStream in(&file) ;
		dataArray.resize(file.size());
		in.readRawData(dataArray.data(), file.size()) ;
		data.setFilePath(fileName);
		if ( !data.makeFromFile(dataArray, m_EditData) ) {
			QMessageBox::warning(this, trUtf8("エラー 1"), trUtf8("読み込みに失敗しました:%1").arg(data.getErrorString()) )  ;
			return false ;
		}
		m_StrSaveFileName = fileName ;
	}
	// 画像ファイル
	else {
		CEditData::ImageData data ;
		QImage imageData ;
		if ( !imageData.load(fileName) ) {
			QMessageBox::warning(this, trUtf8("エラー"), trUtf8("読み込みに失敗しました:%1").arg(fileName)) ;
			return false ;
		}

		data.Image			= imageData ;
		data.fileName		= fileName ;
		data.nTexObj		= 0 ;
		data.lastModified	= QDateTime::currentDateTimeUtc() ;
		data.nNo			= m_EditData.getImageDataListSize() ;
		m_EditData.addImageData(data);

		m_StrSaveFileName = QString() ;
	}

	for ( int i = 0 ; i < m_EditData.getImageDataListSize() ; i ++ ) {
		CEditData::ImageData *p = m_EditData.getImageData(i) ;
		if ( !p ) { continue ; }
		p->origImageW = p->Image.width() ;
		p->origImageH = p->Image.height() ;
		resizeImage(p->Image);
	}

	setWindowTitle(tr("Animation Creator[%1]").arg(m_StrSaveFileName));

	createWindows() ;

	return true ;
}

// ファイル保存
bool MainWindow::saveFile( QString fileName )
{
	qDebug() << "SaveFile:" << fileName ;

	m_EditData.sortFrameDatas() ;

	// XML
	if ( fileName.indexOf(FILE_EXT_ANM2D_XML) > 0 ) {
		CAnm2DXml data(setting.getSaveImage()) ;
		QProgressDialog prog(trUtf8("保存しています"), trUtf8("&Cancel"), 0, 100, this) ;
		prog.setWindowModality(Qt::WindowModal);
		prog.setAutoClose(true);
		data.setProgress(&prog);
		data.setFilePath(fileName);

		QApplication::setOverrideCursor(Qt::WaitCursor);
		if ( !data.makeFromEditData(m_EditData) ) {
			if ( data.getErrorNo() != CAnm2DBase::kErrorNo_Cancel ) {
				QMessageBox::warning(this, trUtf8("エラー"),
									 trUtf8("コンバート失敗 %1:\n%2").arg(fileName).arg(data.getErrorNo())) ;
			}
			QApplication::restoreOverrideCursor();
			return false ;
		}
		prog.setValue(prog.maximum());

		QFile file(fileName) ;
		if ( !file.open(QFile::WriteOnly) ) {
			QMessageBox::warning(this, trUtf8("エラー"),
								 trUtf8("保存失敗 %1:\n%2").arg(fileName).arg(file.errorString())) ;
			QApplication::restoreOverrideCursor();
			return false ;
		}
		file.write(data.getData().toString(4).toAscii()) ;
		QApplication::restoreOverrideCursor();

		m_UndoIndex = m_EditData.getUndoStack()->index() ;
		setWindowTitle(tr("Animation Creator[%1]").arg(fileName));
		return true ;
	}
	// バイナリ
	else if ( fileName.indexOf(FILE_EXT_ANM2D_BIN) > 0 ) {
		qDebug() << "save binary" ;

		CAnm2DBin data ;
		data.setFilePath(fileName);
		QApplication::setOverrideCursor(Qt::WaitCursor) ;
		if ( !data.makeFromEditData(m_EditData) ) {
			if ( data.getErrorNo() != CAnm2DBase::kErrorNo_Cancel ) {
				QMessageBox::warning(this, trUtf8("エラー"),
									 trUtf8("コンバート失敗 %1:\n%2").arg(fileName).arg(data.getErrorNo())) ;
			}
			QApplication::restoreOverrideCursor();
			return false ;
		}
		QFile file(fileName) ;
		if ( !file.open(QFile::WriteOnly) ) {
			QMessageBox::warning(this, trUtf8("エラー"),
								 trUtf8("保存失敗 %1:\n%2").arg(fileName).arg(file.errorString())) ;
			QApplication::restoreOverrideCursor();
			return false ;
		}
		QDataStream out(&file) ;
		out.writeRawData(data.getData().data(), data.getData().size()) ;

		QApplication::restoreOverrideCursor();

		m_UndoIndex = m_EditData.getUndoStack()->index() ;
		setWindowTitle(tr("Animation Creator[%1]").arg(fileName));
		qDebug() << "save successed" ;
		return true ;
	}

	return false ;
}

// 変更したファイルを保存するか
// キャンセルならfalse
bool MainWindow::checkChangedFileSave( void )
{
	QUndoStack *p = m_EditData.getUndoStack() ;
	if ( p->index() != m_UndoIndex ) {
		QMessageBox::StandardButton reply = QMessageBox::question(this, trUtf8("確認"), trUtf8("現在のファイルを保存しますか？"),
																  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel) ;
		if ( reply == QMessageBox::Yes ) {		// 保存する
			slot_save();
			m_UndoIndex = p->index() ;
		}
		else if ( reply == QMessageBox::Cancel ) {	// キャンセル
			return false ;
		}
	}
	return true ;
}

// イメージウィンドウ作成
void MainWindow::makeImageWindow( void )
{
	m_pImageWindow = new ImageWindow( &setting, &m_EditData, m_pAnimationForm, this, m_pMdiArea ) ;
	m_pSubWindow_Img = m_pMdiArea->addSubWindow(m_pImageWindow) ;
	m_pImageWindow->show();

	QPoint pos = setting.getImgWindowPos();
	if ( pos.x() >= 0 && pos.y() >= 0 ) {
		m_pSubWindow_Img->move(pos);
	}
	QSize size = setting.getImgWindowSize();
	if ( size.width() >= 0 && size.height() >= 0 ) {
		m_pSubWindow_Img->resize(size);
	}

	m_pActImageWindow->setEnabled(true);
	m_pActImageWindow->setChecked(true);

	connect(this, SIGNAL(sig_modifiedImageFile(int)), m_pImageWindow, SLOT(slot_modifiedImage(int))) ;
	connect(this, SIGNAL(sig_endedOption()), m_pImageWindow, SLOT(slot_endedOption())) ;
	connect(this, SIGNAL(sig_portCheckDrawCenter(bool)), m_pImageWindow, SLOT(slot_changeDrawCenter(bool))) ;
	connect(this, SIGNAL(sig_portDragedImage(CObjectModel::FrameData)), m_pImageWindow, SLOT(slot_dragedImage(CObjectModel::FrameData))) ;

	connect(m_pSubWindow_Img, SIGNAL(destroyed()), this, SLOT(slot_destroyImgWindow())) ;
}

// ルーペウィンドウ作成
void MainWindow::makeLoupeWindow( void )
{
	m_pLoupeWindow = new CLoupeWindow(&m_EditData, this, m_pMdiArea) ;
	m_pSubWindow_Loupe = m_pMdiArea->addSubWindow( m_pLoupeWindow ) ;
	m_pLoupeWindow->show();

	QPoint pos = setting.getLoupeWindowPos();
	if ( pos.x() >= 0 && pos.y() >= 0 ) {
		m_pSubWindow_Loupe->move(pos);
	}
	QSize size = setting.getLoupeWindowSize();
	if ( size.width() >= 0 && size.height() >= 0 ) {
		m_pSubWindow_Loupe->resize(size);
	}

	m_pActLoupeWindow->setEnabled(true);
	m_pActLoupeWindow->setChecked(true);

	connect(m_pSubWindow_Loupe, SIGNAL(destroyed()), this, SLOT(slot_destroyLoupeWindow())) ;
}

// アニメーションフォーム作成
void MainWindow::makeAnimeWindow( void )
{
	m_pAnimationForm = new AnimationForm( &m_EditData, &setting, m_pMdiArea ) ;
	m_pSubWindow_Anm = m_pMdiArea->addSubWindow( m_pAnimationForm ) ;
	m_pAnimationForm->show();
	m_pAnimationForm->setBarCenter();
	m_pAnimationForm->slot_endedOption();
	QPoint pos = setting.getAnmWindowPos() ;
	if ( pos.x() >= 0 && pos.y() >= 0 ) {
		m_pSubWindow_Anm->move(pos) ;
	}
	QSize size = setting.getAnmWindowSize() ;
	if ( size.width() >= 0 && size.height() >= 0 ) {
		m_pSubWindow_Anm->resize(size) ;
	}

	m_pActAnimeWindow->setEnabled(true);
	m_pActAnimeWindow->setChecked(true);

	connect(this, SIGNAL(sig_modifiedImageFile(int)), m_pAnimationForm, SLOT(slot_modifiedImage(int))) ;
	connect(this, SIGNAL(sig_endedOption()), m_pAnimationForm, SLOT(slot_endedOption())) ;
	connect(m_pAnimationForm, SIGNAL(sig_portCheckDrawCenter(bool)), this, SLOT(slot_portCheckDrawCenter(bool))) ;
	connect(m_pAnimationForm, SIGNAL(sig_portDragedImage(CObjectModel::FrameData)), this, SLOT(slot_portDragedImage(CObjectModel::FrameData))) ;
	connect(m_pAnimationForm, SIGNAL(sig_pushColorToolButton()), this, SLOT(slot_pushColorToolButton())) ;

	connect(m_pSubWindow_Anm, SIGNAL(destroyed()), this, SLOT(slot_destroyAnmWindow())) ;
}
