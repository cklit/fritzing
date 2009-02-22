/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2009 Fachhochschule Potsdam - http://fh-potsdam.de

Fritzing is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Fritzing is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Fritzing.  If not, see <http://www.gnu.org/licenses/>.

********************************************************************

$Revision$:
$Author$:
$Date$

********************************************************************/

#include <QtGui>
#include <QtXml>
#include <QList>
#include <QFileInfo>
#include <QStringList>
#include <QFileInfoList>
#include <QDir>

#include <QTime>
#include <QCleanlooksStyle>
#include <QDir>
#include <QSettings>
#include <QRegExpValidator>
#include <QRegExp>
#include <QPaintDevice>
#include <QPixmap>

#include "paletteitem.h"
#include "mainwindow.h"
#include "debugdialog.h"
#include "connector.h"
#include "partseditor/mainpartseditorwindow.h"
#include "fdockwidget.h"
#include "htmlinfoview.h"
#include "waitpushundostack.h"
#include "fapplication.h"
#include "layerattributes.h"
#include "triplenavigator.h"

#include "help/helper.h"
#include "dockmanager.h"
#include "group/saveasmoduledialog.h"

#include "fsvgrenderer.h"
#include "fsizegrip.h"


const QString MainWindow::UntitledSketchName = "Untitled Sketch";
int MainWindow::UntitledSketchIndex = 1;
int MainWindow::CascadeFactorX = 21;
int MainWindow::CascadeFactorY = 19;

MainWindow::MainWindow(PaletteModel * paletteModel, ReferenceModel *refModel) :
	FritzingWindow(untitledFileName(), untitledFileCount(), fileExtension())
{
	QFile styleSheet(":/resources/styles/fritzing.qss");

	m_helper = NULL;

	resize(740,600);

	// Create dot icons
	m_dotIcon = QIcon(":/resources/images/dot.png");
	m_emptyIcon = QIcon();

	m_currentWidget = NULL;
	m_currentGraphicsView = NULL;
	m_firstOpen = true;

	m_statusBar = new QStatusBar(this);
	setStatusBar(m_statusBar);
	m_statusBar->setSizeGripEnabled(false);

	setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_WS_MAC
	//setAttribute(Qt::WA_QuitOnClose, false);					// restoring this temporarily (2008.12.19)
#endif
    m_dontClose = m_closing = false;
	m_savedState = NeverSaved;

	m_paletteModel = paletteModel;
	m_refModel = refModel;
	m_sketchModel = new SketchModel(true);

	m_tabWidget = new QStackedWidget(this); //   FTabWidget(this);
	m_tabWidget->setObjectName("sketch_tabs");

	setCentralWidget(m_tabWidget);


	// all this belongs in viewLayer.xml
	m_breadboardGraphicsView = new BreadboardSketchWidget(ItemBase::BreadboardView, this);
	initSketchWidget(m_breadboardGraphicsView);
	m_breadboardWidget = new SketchAreaWidget(m_breadboardGraphicsView,this);
	//m_tabWidget->addTab(m_breadboardWidget, tr("breadboard"));
	//m_breadViewSwitcher = new ViewSwitcher(this);
	//connectSwitcherToView(m_breadViewSwitcher,m_breadboardGraphicsView);
	m_tabWidget->addWidget(m_breadboardWidget);

	m_schematicGraphicsView = new SchematicSketchWidget(ItemBase::SchematicView, this);
	initSketchWidget(m_schematicGraphicsView);
	m_schematicWidget = new SketchAreaWidget(m_schematicGraphicsView, this);
	//m_tabWidget->addTab(m_schematicWidget, tr("schematic"));
	//m_schemViewSwitcher = new ViewSwitcher(this);
	//connectSwitcherToView(m_schemViewSwitcher,m_schematicGraphicsView);
	m_tabWidget->addWidget(m_schematicWidget);

	m_pcbGraphicsView = new PCBSketchWidget(ItemBase::PCBView, this);
	initSketchWidget(m_pcbGraphicsView);
	m_pcbWidget = new SketchAreaWidget(m_pcbGraphicsView, this);
	//m_tabWidget->addTab(m_pcbWidget, tr("pcb"));
	//m_pcbViewSwitcher = new ViewSwitcher(this);
	//connectSwitcherToView(m_pcbViewSwitcher,m_pcbGraphicsView);
	m_tabWidget->addWidget(m_pcbWidget);

	m_schematicGraphicsView->addRatnestTarget(m_pcbGraphicsView);

    m_undoView = new QUndoView();
    m_undoGroup = new QUndoGroup(this);
    m_undoView->setGroup(m_undoGroup);
    m_undoGroup->setActiveStack(m_breadboardGraphicsView->undoStack());

    m_dockManager = new DockManager(this);
    m_dockManager->createBinAndInfoViewDocks();
    createActions();
    createSketchButtons();
    createMenus();
    createToolBars();
    createStatusBar();

	m_viewSwitcher = new ViewSwitcher();
	connect(m_viewSwitcher, SIGNAL(viewSwitched(int)), this, SLOT(viewSwitchedTo(int)));
	connect(this, SIGNAL(viewSwitched(int)), m_viewSwitcher, SLOT(viewSwitchedTo(int)));
	m_viewSwitcher->viewSwitchedTo(0);

    m_dockManager->createDockWindows();

    m_breadboardWidget->setContent(
    	getButtonsForView(m_breadboardWidget->viewIdentifier()),
    	createZoomOptions(m_breadboardWidget)
    );
    m_schematicWidget->setContent(
    	getButtonsForView(m_schematicWidget->viewIdentifier()),
    	createZoomOptions(m_schematicWidget)
    );
	m_pcbWidget->setContent(
		getButtonsForView(m_pcbWidget->viewIdentifier()),
		createZoomOptions(m_pcbWidget)
	);


    if (!styleSheet.open(QIODevice::ReadOnly)) {
		qWarning("Unable to open :/resources/styles/fritzing.qss");
	} else {
		setStyleSheet(styleSheet.readAll()+___MacStyle___);
	}

	QMenu *breadItemMenu = breadboardItemMenu();
    m_breadboardGraphicsView->setItemMenu(breadItemMenu);
    m_breadboardGraphicsView->setWireMenu(breadItemMenu);

    m_pcbGraphicsView->setWireMenu(pcbWireMenu());
    m_pcbGraphicsView->setItemMenu(pcbItemMenu());

    QMenu *schemItemMenu = schematicItemMenu();
    m_schematicGraphicsView->setItemMenu(schemItemMenu);
    m_schematicGraphicsView->setWireMenu(schemItemMenu);

    m_breadboardGraphicsView->setInfoView(m_infoView);
    m_pcbGraphicsView->setInfoView(m_infoView);
    m_schematicGraphicsView->setInfoView(m_infoView);

    m_breadboardGraphicsView->setBackground(QColor(204,204,204));
    m_schematicGraphicsView->setBackground(QColor(255,255,255));
    m_pcbGraphicsView->setBackground(QColor(160,168,179));							// QColor(137,144,153)

	// make sure to set the connections after the views have been created
	connect(m_tabWidget, SIGNAL(currentChanged ( int )),
			this, SLOT(tabWidget_currentChanged( int )));

	connectPairs();

	// do this the first time, since the current_changed signal wasn't sent
	m_currentGraphicsView = NULL;
	int tab = 0;
	currentNavigatorChanged(m_navigators[tab]);
	tabWidget_currentChanged(tab+1);
	tabWidget_currentChanged(tab);
	updateTransformationActions();

	this->installEventFilter(this);

	m_comboboxChanged = false;

	QSettings settings;
	if(!settings.value("main/state").isNull()) {
		restoreState(settings.value("main/state").toByteArray());
		restoreGeometry(settings.value("main/geometry").toByteArray());
	}

	setMinimumSize(0,0);
	m_tabWidget->setMinimumWidth(500);
	m_tabWidget->setMinimumWidth(0);

	m_miniViewContainerBreadboard->setView(m_breadboardGraphicsView);
	m_miniViewContainerSchematic->setView(m_schematicGraphicsView);
	m_miniViewContainerPCB->setView(m_pcbGraphicsView);

	connect(this, SIGNAL(readOnlyChanged(bool)), this, SLOT(applyReadOnlyChange(bool)));

	m_helper = new Helper(this, true);   

	m_setUpDockManagerTimer.setSingleShot(true);
	connect(&m_setUpDockManagerTimer, SIGNAL(timeout()), m_dockManager, SLOT(keepMargins()));
	m_setUpDockManagerTimer.start(1000);
}

MainWindow::~MainWindow()
{
	delete m_sketchModel;
	m_dockManager->dontKeepMargins();
	m_setUpDockManagerTimer.stop();
}


void MainWindow::initSketchWidget(SketchWidget * sketchWidget) {
	sketchWidget->setPaletteModel(m_paletteModel);
	sketchWidget->setSketchModel(m_sketchModel);
	sketchWidget->setRefModel(m_refModel);
	sketchWidget->setUndoStack(m_undoStack);
	sketchWidget->setChainDrag(true);			// enable bend points
	sketchWidget->addViewLayers();
}

/*
qreal MainWindow::getSvgWidthInInches(const QString & filename)
{
	qreal result = 0;

	QFile file(filename);
	if (!file.open(QFile::ReadOnly | QFile::Text)) {
		return result;
	}

	result = getSvgWidthInInches(file);
	file.close();

	return result;
}

qreal MainWindow::getSvgWidthInInches(QFile & file)
{
	QString errorStr;
	int errorLine;
	int errorColumn;
	QDomDocument* domDocument = new QDomDocument();

	if (!domDocument->setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
		return 0;
	}

	QDomElement root = domDocument->documentElement();
	if (root.isNull()) {
		return 0;
	}

	if (root.tagName() != "svg") {
		return 0;
	}

	QString stringWidth = root.attribute("width");
	if (stringWidth.isNull()) {
		return 0;
	}

	if (stringWidth.isEmpty()) {
		return 0;
	}

	bool ok;
	qreal result = convertToInches(stringWidth, &ok);
	if (!ok) return 0;

	return result;
}
*/

void MainWindow::connectPairs() {
	connectPair(m_breadboardGraphicsView, m_schematicGraphicsView);
	connectPair(m_breadboardGraphicsView, m_pcbGraphicsView);
	connectPair(m_schematicGraphicsView, m_breadboardGraphicsView);
	connectPair(m_schematicGraphicsView, m_pcbGraphicsView);
	connectPair(m_pcbGraphicsView, m_breadboardGraphicsView);
	connectPair(m_pcbGraphicsView, m_schematicGraphicsView);

	bool succeeded = connect(m_breadboardGraphicsView, SIGNAL(findSketchWidgetSignal(ItemBase::ViewIdentifier, SketchWidget * &)),
							 this, SLOT(findSketchWidgetSlot(ItemBase::ViewIdentifier, SketchWidget * &)),
							 Qt::DirectConnection);

	succeeded = connect(m_schematicGraphicsView, SIGNAL(findSketchWidgetSignal(ItemBase::ViewIdentifier, SketchWidget * &)),
							 this, SLOT(findSketchWidgetSlot(ItemBase::ViewIdentifier, SketchWidget * &)),
							 Qt::DirectConnection);

	succeeded = connect(m_pcbGraphicsView, SIGNAL(findSketchWidgetSignal(ItemBase::ViewIdentifier, SketchWidget * &)),
							 this, SLOT(findSketchWidgetSlot(ItemBase::ViewIdentifier, SketchWidget * &)),
							 Qt::DirectConnection);

	succeeded = connect(m_pcbGraphicsView, SIGNAL(routingStatusSignal(int, int, int, int)),
						this, SLOT(routingStatusSlot(int, int, int, int)));

	succeeded = connect(m_pcbGraphicsView, SIGNAL(ratsnestChangeSignal(SketchWidget *, QUndoCommand *)),
						this, SLOT(clearRoutingSlot(SketchWidget *, QUndoCommand *)));
	succeeded = connect(m_pcbGraphicsView, SIGNAL(movingSignal(SketchWidget *, QUndoCommand *)),
						this, SLOT(clearRoutingSlot(SketchWidget *, QUndoCommand *)));
	succeeded = connect(m_pcbGraphicsView, SIGNAL(rotatingFlippingSignal(SketchWidget *, QUndoCommand *)),
						this, SLOT(clearRoutingSlot(SketchWidget *, QUndoCommand *)));

	succeeded = connect(m_schematicGraphicsView, SIGNAL(ratsnestChangeSignal(SketchWidget *, QUndoCommand *)),
						this, SLOT(clearRoutingSlot(SketchWidget *, QUndoCommand *)));
	succeeded = connect(m_breadboardGraphicsView, SIGNAL(ratsnestChangeSignal(SketchWidget *, QUndoCommand *)),
						this, SLOT(clearRoutingSlot(SketchWidget *, QUndoCommand *)));

	succeeded = connect(m_schematicGraphicsView, SIGNAL(schematicDisconnectWireSignal(ConnectorPairHash &, QSet<ItemBase *> &, QHash<ItemBase *, ConnectorPairHash *> &, QUndoCommand *)),
						m_breadboardGraphicsView, SLOT(schematicDisconnectWireSlot(ConnectorPairHash &, QSet<ItemBase *> &, QHash<ItemBase *, ConnectorPairHash *> &, QUndoCommand *)),
						Qt::DirectConnection);

	FApplication * fapp = dynamic_cast<FApplication *>(qApp);
	if (fapp != NULL) {
		succeeded = connect(fapp, SIGNAL(spaceBarIsPressedSignal(bool)), m_breadboardGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));
		succeeded = connect(fapp, SIGNAL(spaceBarIsPressedSignal(bool)), m_schematicGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));
		succeeded = connect(fapp, SIGNAL(spaceBarIsPressedSignal(bool)), m_pcbGraphicsView, SLOT(spaceBarIsPressedSlot(bool)));
	}
}

void MainWindow::connectPair(SketchWidget * signaller, SketchWidget * slotter)
{

	bool succeeded = connect(signaller, SIGNAL(itemAddedSignal(ModelPart *, const ViewGeometry & , long )),
							 slotter, SLOT(sketchWidget_itemAdded(ModelPart *, const ViewGeometry &, long)));

	succeeded = succeeded && connect(signaller, SIGNAL(itemDeletedSignal(long)),
									 slotter, SLOT(sketchWidget_itemDeleted(long)),
									 Qt::DirectConnection);

	succeeded = succeeded && connect(signaller, SIGNAL(clearSelectionSignal()),
									 slotter, SLOT(sketchWidget_clearSelection()));

	succeeded = succeeded && connect(signaller, SIGNAL(itemSelectedSignal(long, bool)),
									 slotter, SLOT(sketchWidget_itemSelected(long, bool)));
	succeeded = succeeded && connect(signaller, SIGNAL(selectAllItemsSignal(bool, bool)),
									 slotter, SLOT(selectAllItems(bool, bool)));
	succeeded = succeeded && connect(signaller, SIGNAL(wireDisconnectedSignal(long, QString)),
									 slotter, SLOT(sketchWidget_wireDisconnected(long,  QString)));
	succeeded = succeeded && connect(signaller, SIGNAL(wireConnectedSignal(long,  QString, long,  QString)),
									 slotter, SLOT(sketchWidget_wireConnected(long, QString, long, QString)));
	succeeded = succeeded && connect(signaller, SIGNAL(changeConnectionSignal(long,  QString, long,  QString, bool, bool)),
									 slotter, SLOT(sketchWidget_changeConnection(long, QString, long, QString, bool, bool)));
	succeeded = succeeded && connect(signaller, SIGNAL(copyItemSignal(long, QHash<ItemBase::ViewIdentifier, ViewGeometry *> &)),
													   slotter, SLOT(sketchWidget_copyItem(long, QHash<ItemBase::ViewIdentifier, ViewGeometry *> &)),
									 Qt::DirectConnection);
	succeeded = succeeded && connect(signaller, SIGNAL(deleteItemSignal(long, QUndoCommand *)),
									 slotter, SLOT(sketchWidget_deleteItem(long, QUndoCommand *)),
									 Qt::DirectConnection);

	succeeded = succeeded && connect(signaller, SIGNAL(cleanUpWiresSignal(CleanUpWiresCommand *)),
									 slotter, SLOT(sketchWidget_cleanUpWires(CleanUpWiresCommand *)) );
	succeeded = succeeded && connect(signaller, SIGNAL(swapped(long, ModelPart*, bool, SwapCommand *)),
									 slotter, SLOT(swap(long, ModelPart*, bool, SwapCommand *)),
									 Qt::DirectConnection );

	succeeded = succeeded && connect(signaller, SIGNAL(dealWithRatsnestSignal(long, const QString &,
																			  long, const QString &,
																			  bool, RatsnestCommand *)),
									 slotter, SLOT(dealWithRatsnestSlot(long, const QString &,
																			  long, const QString &,
																			  bool, RatsnestCommand *)) );

	if (!succeeded) {
		DebugDialog::debug("connectPair failed");
	}

}

void MainWindow::setCurrentFile(const QString &fileName, bool addToRecent) {
	m_fileName = fileName;

	updateRaiseWindowAction();
	setTitle();

	if(addToRecent) {
		QSettings settings;
		QStringList files = settings.value("recentFileList").toStringList();
		files.removeAll(fileName);
		files.prepend(fileName);
		while (files.size() > MaxRecentFiles)
			files.removeLast();

		settings.setValue("recentFileList", files);
	}

    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        MainWindow *mainWin = qobject_cast<MainWindow *>(widget);
        if (mainWin)
            mainWin->updateRecentFileActions();
    }
}


ZoomComboBox *MainWindow::createZoomOptions(SketchAreaWidget* parent) {
	ZoomComboBox *zoomOptsComboBox = new ZoomComboBox(parent);

	setZoomComboBoxValue(parent->graphicsView()->currentZoom(), zoomOptsComboBox);
	connect(zoomOptsComboBox, SIGNAL(zoomChanged(qreal)), this, SLOT(updateViewZoom(qreal)));

    connect(parent->graphicsView(), SIGNAL(zoomChanged(qreal)), this, SLOT(updateZoomOptions(qreal)));
    connect(parent->graphicsView(), SIGNAL(zoomOutOfRange(qreal)), this, SLOT(updateZoomOptionsNoMatterWhat(qreal)));

	return zoomOptsComboBox;
}

void MainWindow::createToolBars() {
	/* TODO: Mariano this is too hacky and requires some styling
	 * around here and some else in the qss file
	 */
	/*m_toolbar = new QToolBar(this);
	m_toolbar->setObjectName("fake_tabbar");
	m_toolbar->setFloatable(false);
	m_toolbar->setMovable(false);
	int height = 0; //  m_tabWidget->tabBar()->height();
	m_toolbar->layout()->setMargin(0);
	m_toolbar->setFixedHeight(height+10);
	m_toolbar->setMinimumWidth(400); // connect to tabwidget resize event
	m_toolbar->toggleViewAction()->setVisible(false);
	// m_tabWidget->tabBar()->setParent(m_toolbar);
	addToolBar(m_toolbar);*/

	/*	QToolBar *tb2 = new QToolBar(this);
	tb2->setFloatable(false);
	tb2->setMovable(false);
	QToolButton *dummyButton = new QToolButton();
	dummyButton->setIcon(QIcon(":/resources/images/toolbar_icons/toolbarExport_pdf_icon.png"));
	tb2->addWidget(dummyButton);
	QToolButton *dummyButton2 = new QToolButton();
	dummyButton2->setIcon(QIcon(":/resources/images/toolbar_icons/toolbarOrder_icon.png"));
	tb2->addWidget(dummyButton2);
	addToolBar(tb2);*/

	/*
    m_fileToolBar = addToolBar(tr("File"));
    m_fileToolBar->setObjectName("fileToolBar");
    m_fileToolBar->addAction(m_saveAct);
    m_fileToolBar->addAction(m_printAct);

    m_editToolBar = addToolBar(tr("Edit"));
    m_editToolBar->setObjectName("editToolBar");
    m_editToolBar->addAction(m_undoAct);
    m_editToolBar->addWidget(m_zoomOptsComboBox);
    */
}

void MainWindow::createSketchButtons() {
	m_routingStatusLabel = new ExpandingLabel(m_pcbWidget);
	m_routingStatusLabel->setObjectName(SketchAreaWidget::RoutingStateLabelName);

	routingStatusSlot(0,0,0,0);			// call this after the buttons have been created, because it calls updateTraceMenu
}

SketchToolButton *MainWindow::createRotateButton(SketchAreaWidget *parent) {
	QList<QAction*> rotateMenuActions;
	rotateMenuActions << m_rotate90ccwAct << m_rotate180Act << m_rotate90cwAct;
	SketchToolButton * rotateButton = new SketchToolButton("Rotate",parent, rotateMenuActions);
	rotateButton->setText(tr("Rotate"));
	connect(rotateButton, SIGNAL(menuUpdateNeeded()), this, SLOT(updateTransformationActions()));

	m_rotateButtons << rotateButton;
	return rotateButton;
}

SketchToolButton *MainWindow::createFlipButton(SketchAreaWidget *parent) {
	QList<QAction*> flipMenuActions;
	flipMenuActions << m_flipHorizontalAct << m_flipVerticalAct;
	SketchToolButton *flipButton = new SketchToolButton("Flip",parent, flipMenuActions);
	flipButton->setText(tr("Flip"));
	connect(flipButton, SIGNAL(menuUpdateNeeded()), this, SLOT(updateTransformationActions()));

	m_flipButtons << flipButton;
	return flipButton;
}

SketchToolButton *MainWindow::createAutorouteButton(SketchAreaWidget *parent) {
	SketchToolButton *autorouteButton = new SketchToolButton("Autoroute",parent, m_autorouteAct);
	autorouteButton->setText(tr("Autoroute"));

	return autorouteButton;
}

SketchToolButton *MainWindow::createNoteButton(SketchAreaWidget *parent) {
	SketchToolButton *noteButton = new SketchToolButton("Notes",parent, m_addNoteAct);
	noteButton->setText(tr("Add a note"));
	noteButton->setEnabledIcon();					// seems to need this to display button icon first time
	return noteButton;
}

SketchToolButton *MainWindow::createExportDiyButton(SketchAreaWidget *parent) {
	SketchToolButton *exportDiyButton = new SketchToolButton("Diy",parent, m_exportDiyAct);
	exportDiyButton->setText(tr("Export Etchable PDF"));
	exportDiyButton->setEnabledIcon();				// seems to need this to display button icon first time
	return exportDiyButton;
}

QWidget *MainWindow::createToolbarSpacer(SketchAreaWidget *parent) {
	QFrame *toolbarSpacer = new QFrame(parent);
	QHBoxLayout *spacerLayout = new QHBoxLayout(toolbarSpacer);
	spacerLayout->addSpacerItem(new QSpacerItem(0,0,QSizePolicy::Expanding));

	return toolbarSpacer;
}

QList<QWidget*> MainWindow::getButtonsForView(ItemBase::ViewIdentifier viewId) {
	QList<QWidget*> retval;
	SketchAreaWidget *parent;
	switch(viewId) {
		case ItemBase::BreadboardView: parent = m_breadboardWidget; break;
		case ItemBase::SchematicView: parent = m_schematicWidget; break;
		case ItemBase::PCBView: parent = m_pcbWidget; break;
		default: return retval;
	}
	/*if(viewId != ItemBase::PCBView) {
		retval << createExportToPdfButton(parent);
	}*/
	retval << createNoteButton(parent) << createRotateButton(parent);
	if(viewId == ItemBase::BreadboardView) {
		retval << createFlipButton(parent);
	} else if(viewId == ItemBase::PCBView) {
		retval << SketchAreaWidget::separator(parent) << createAutorouteButton(parent)
			   << createExportDiyButton(parent) << m_routingStatusLabel;
	}

	if(viewId != ItemBase::PCBView) {
		retval << createToolbarSpacer(parent);
	}
	return retval;
}

void MainWindow::updateZoomOptions(qreal zoom) {
	if(!m_comboboxChanged) {
		setZoomComboBoxValue(zoom);
	} else {
		m_comboboxChanged = false;
	}
}

SketchAreaWidget *MainWindow::currentSketchArea() {
	return dynamic_cast<SketchAreaWidget*>(m_currentGraphicsView->parent());
}

void MainWindow::updateZoomOptionsNoMatterWhat(qreal zoom) {
	currentSketchArea()->zoomComboBox()->setEditText(tr("%1%").arg(zoom));
}

void MainWindow::updateViewZoom(qreal newZoom) {
	m_comboboxChanged = true;
	if(m_currentGraphicsView) m_currentGraphicsView->absoluteZoom(newZoom);
}


void MainWindow::createStatusBar()
{
    m_statusBar->showMessage(tr("Ready"));
}

void MainWindow::tabWidget_currentChanged(int index) {
	SketchAreaWidget * widgetParent = dynamic_cast<SketchAreaWidget *>(m_tabWidget->currentWidget());
	if (widgetParent == NULL) return;

	m_currentWidget = widgetParent;

	QStatusBar *sb = statusBar();
	connect(sb, SIGNAL(messageChanged(const QString &)), m_statusBar, SLOT(showMessage(const QString &)));
	widgetParent->addStatusBar(m_statusBar);
	if(sb != m_statusBar) sb->hide();

	if (m_breadboardGraphicsView) m_breadboardGraphicsView->setCurrent(false);
	if (m_schematicGraphicsView) m_schematicGraphicsView->setCurrent(false);
	if (m_pcbGraphicsView) m_pcbGraphicsView->setCurrent(false);

	SketchWidget *widget = widgetParent->graphicsView();

	if(m_currentGraphicsView) {
		disconnect(
			m_currentGraphicsView,
			SIGNAL(selectionChangedSignal()),
			this,
			SLOT(updateTransformationActions())
		);
	}
	m_currentGraphicsView = widget;
	if (widget == NULL) return;

	connect(
		m_currentGraphicsView,					// don't connect directly to the scene here, connect to the widget's signal
		SIGNAL(selectionChangedSignal()),
		this,
		SLOT(updateTransformationActions())
	);


	m_currentGraphicsView->setCurrent(true);

	//  TODO:  should be a cleaner way to do this
	switch( index ) {
		case 0 : setShowViewActionsIcons(m_showBreadboardAct, m_showSchematicAct,  m_showPCBAct); break;
		case 1 : setShowViewActionsIcons(m_showSchematicAct,  m_showBreadboardAct, m_showPCBAct); break;
		case 2 : setShowViewActionsIcons(m_showPCBAct,        m_showBreadboardAct, m_showSchematicAct); break;
		default :
			// Shouldn't get here
			DebugDialog::debug("Warning: not considered tab selected");
	}

	hideShowTraceMenu();
	updateTraceMenu();

	setTitle();

	// triggers a signal to the navigator widget
	m_navigators[index]->miniViewMousePressedSlot();
	emit viewSwitched(index);

	if (m_helper == NULL) {
		m_showInViewHelpAct->setChecked(false);
	}
	else {
		m_showInViewHelpAct->setChecked(m_helper->helpVisible(m_tabWidget->currentIndex()));
	}

	m_currentGraphicsView->updateInfoView();

	// obsolete: when there are 3 navigators and 3 zoom boxes, no need to update when current view changes
	//m_miniViewContainer0->setView(widget);
	//setZoomComboBoxValue(m_currentWidget->currentZoom());
}

void MainWindow::setShowViewActionsIcons(QAction * active, QAction * inactive1, QAction * inactive2) {
	active->setIcon(m_dotIcon);
	inactive1->setIcon(m_emptyIcon);
	inactive2->setIcon(m_emptyIcon);
}

void MainWindow::closeEvent(QCloseEvent *event) {
	if (m_dontClose) {
		event->ignore();
		return;
	}

	if(!beforeClosing() || !whatToDoWithAlienFiles() ||!m_paletteWidget->beforeClosing()) {
		event->ignore();
		return;
	}

	m_closing = true;
	emit aboutToClose();

	int count = 0;
	foreach (QWidget *widget, QApplication::topLevelWidgets()) {
		if (widget == this) continue;
		if (dynamic_cast<QMainWindow *>(widget) == NULL) continue;
		// TODO Brendan: please remove all the references and the source code
		// of the old parts editor
		//if (dynamic_cast<MainPartsEditorWindow *>(widget) != NULL) continue;

		count++;
	}

	DebugDialog::debug(QString("current main windows: %1").arg(QApplication::topLevelWidgets().size()));

	if (count == 0) {
		DebugDialog::closeDebug();
	}

	QSettings settings;
	settings.setValue("main/state",saveState());
	settings.setValue("main/geometry",saveGeometry());

	QMainWindow::closeEvent(event);
}

bool MainWindow::whatToDoWithAlienFiles() {
	if (m_alienFiles.size() > 0) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(this, tr("Save %1").arg(QFileInfo(m_fileName).baseName()),
									 tr("Do you want to keep the parts that were loaded with this shareable sketch %1?")
									 .arg(QFileInfo(m_fileName).baseName()),
									 QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (reply == QMessageBox::Yes) {
			return true;
		} else if (reply == QMessageBox::No) {
			foreach(QString pathToRemove, m_alienFiles) {
				QFile::remove(pathToRemove);
			}
			m_alienFiles.clear();
			recoverBackupedFiles();

			emit alienPartsDismissed();
			return true;
		}
		else {
			return false;
		}
	} else {
		return true;
	}
}


void MainWindow::setZoomComboBoxValue(qreal value, ZoomComboBox* zoomComboBox) {
	if(!zoomComboBox) zoomComboBox = currentSketchArea()->zoomComboBox();
	zoomComboBox->setEditText(tr("%1%").arg(value,0,'f',2));
}

void MainWindow::changeActivation(bool activate) {
	// tried using this->saveState() and this->restoreState() but couldn't get it to work

	//DebugDialog::debug(QString("change activation:%2 %1").arg(this->windowTitle()).arg(activate));

	QWidget * activeWindow = QApplication::activeWindow ();
	//DebugDialog::debug(QString("active %1").arg(activeWindow == NULL ? "NULL" : activeWindow->windowTitle()));

	if (activate) {
		if (m_savedState == Saved) {
			m_savedState = Restored;
			//DebugDialog::debug("restore state");
			//restoreState(m_savedStateData, 0);
			for (int i = 0; i < children().count(); i++) {
				FDockWidget * dock = dynamic_cast<FDockWidget *>(children()[i]);
				if (dock == NULL) continue;

				dock->restoreStateSoon();
				//DebugDialog::debug(QString("restoring dock %1").arg(dock->windowTitle()));
			}

		}
	}
	else {
		if ((activeWindow != NULL) && (activeWindow == this || activeWindow->parent() == this)) {
			//DebugDialog::debug("skipping save");
			return;
		}

		if (!(m_savedState == Saved)) {

			//m_savedStateData = saveState(0);
			m_savedState = Saved;

			//DebugDialog::debug("save state");
			for (int i = 0; i < children().count(); i++) {
				FDockWidget * dock = dynamic_cast<FDockWidget *>(children()[i]);
				if (dock == NULL) continue;

				dock->saveState();
				//DebugDialog::debug(QString("saving dock %1").arg(dock->windowTitle()));

				if (dock->isFloating() && dock->isVisible()) {
					//DebugDialog::debug(QString("hiding dock %1").arg(dock->windowTitle()));
					dock->hide();
				}
			}
		}
	}
}

void MainWindow::loadPart(QString newPartPath) {
	ModelPart * modelPart = ((PaletteModel*)m_refModel)->addPart(newPartPath, true, true);
	if(modelPart && modelPart->isValid()) {
		m_paletteWidget->addNewPart(modelPart);
		m_infoView->reloadContent(m_currentGraphicsView);
	}
}

bool MainWindow::eventFilter(QObject *object, QEvent *event) {
	if (object == this &&
		(event->type() == QEvent::KeyPress
		// || event->type() == QEvent::KeyRelease
		|| event->type() == QEvent::ShortcutOverride))
	{
		//DebugDialog::debug(QString("event filter %1").arg(event->type()) );
		updatePartMenu();
		updateTraceMenu();

		// On the mac, the first time the delete key is pressed, to be used as a shortcut for QAction m_deleteAct,
		// for some reason, the enabling of the m_deleteAct in UpdateEditMenu doesn't "take" until the next time the event loop is processed
		// Thereafter, the delete key works as it should.
		// So this call to processEvents() makes sure m_deleteAct is enabled.
		QCoreApplication::processEvents();
	}

	return QMainWindow::eventFilter(object, event);
}

void MainWindow::findSketchWidgetSlot(ItemBase::ViewIdentifier viewIdentifier, SketchWidget * & sketchWidget ) {
	if (m_breadboardGraphicsView->viewIdentifier() == viewIdentifier) {
		sketchWidget = m_breadboardGraphicsView;
		return;
	}

	if (m_schematicGraphicsView->viewIdentifier() == viewIdentifier) {
		sketchWidget = m_schematicGraphicsView;
		return;
	}

	if (m_pcbGraphicsView->viewIdentifier() == viewIdentifier) {
		sketchWidget = m_pcbGraphicsView;
		return;
	}

	sketchWidget = NULL;

}

const QString MainWindow::untitledFileName() {
	return UntitledSketchName;
}

int &MainWindow::untitledFileCount() {
	return UntitledSketchIndex;
}

const QString MainWindow::fileExtension() {
	return FritzingSketchExtension;
}

const QString MainWindow::defaultSaveFolder() {
	return FApplication::openSaveFolder();
}

const QString & MainWindow::fileName() {
	return m_fileName;
}

bool MainWindow::undoStackIsEmpty() {
	return m_undoStack->count() == 0;
}

void MainWindow::setInfoViewOnHover(bool infoViewOnHover) {
	m_breadboardGraphicsView->setInfoViewOnHover(infoViewOnHover);
	m_schematicGraphicsView->setInfoViewOnHover(infoViewOnHover);
	m_pcbGraphicsView->setInfoViewOnHover(infoViewOnHover);

	m_paletteWidget->setInfoViewOnHover(infoViewOnHover);
}

void MainWindow::swapSelected() {
	ModelPart *selInParts = m_paletteWidget->selected();
	if(selInParts) m_currentGraphicsView->swapSelected(selInParts);
}

#define ZIP_PART QString("part.")
#define ZIP_SVG  QString("svg.")

void MainWindow::saveBundledSketch() {
	QString fileExt;
	QString path = defaultSaveFolder() + "/" + QFileInfo(m_fileName).fileName()+"z";
	QString bundledFileName = FApplication::getSaveFileName(
			this,
			tr("Specify a file name"),
			path,
			tr("Fritzing (*%1)").arg(FritzingBundleExtension),
			&fileExt
		  );

	if (bundledFileName.isEmpty()) return; // Cancel pressed

	if(!alreadyHasExtension(bundledFileName)) {
		fileExt = getExtFromFileDialog(fileExt);
		bundledFileName += fileExt;
	}

	QDir destFolder = QDir::temp();

	createFolderAnCdIntoIt(destFolder, getRandText());
	QString dirToRemove = destFolder.path();

	QString aux = QFileInfo(bundledFileName).fileName();
	QString destSketchPath = // remove the last "z" from the extension
			destFolder.path()+"/"+aux.left(aux.size()-1);
	DebugDialog::debug("saving sketch temporarily to "+destSketchPath);

	bool wasModified = isWindowModified();
	QString prevFileName = m_fileName;
	saveAsAux(destSketchPath);
	m_fileName = prevFileName;
	setWindowModified(wasModified);
	setTitle();

	QList<ModelPart*> partsToSave = m_sketchModel->root()->getAllNonCoreParts();
	foreach(ModelPart* mp, partsToSave) {
		QString partPath = mp->modelPartShared()->path();
		QFile file(partPath);
		file.copy(destFolder.path()+"/"+ZIP_PART+QFileInfo(partPath).fileName());
		QList<SvgAndPartFilePath> views = mp->getAvailableViewFiles();
		foreach(SvgAndPartFilePath view, views) {
			if(view.coreContribOrUser() != "core") {
				QFile file(view.absolutePath());
				QString svgRelativePath = view.relativePath();
				file.copy(destFolder.path()+"/"+ZIP_SVG+svgRelativePath.replace("/","."));
			}
		}
	}

	if(!createZipAndSaveTo(destFolder, bundledFileName)) {
		QMessageBox::warning(
			this,
			tr("Fritzing"),
			tr("Unable to export %1 to shareable sketch").arg(bundledFileName)
		);
	}

	rmdir(dirToRemove);
}

void MainWindow::loadBundledSketch(const QString &fileName) {
	QDir destFolder = QDir::temp();

	createFolderAnCdIntoIt(destFolder, getRandText());
	QString unzipDir = destFolder.path();

	if(!unzipTo(fileName, unzipDir)) {
		QMessageBox::warning(
			this,
			tr("Fritzing"),
			tr("Unable to open shareable sketch %1").arg(fileName)
		);
	}

	moveToPartsFolderAndLoad(unzipDir);

	rmdir(unzipDir);
}

void MainWindow::moveToPartsFolderAndLoad(const QString &unzipDirPath) {
	QDir unzipDir(unzipDirPath);
	QStringList namefilters;

	MainWindow* mw = new MainWindow(m_paletteModel, m_refModel);

	namefilters << ZIP_SVG+"*";
	foreach(QFileInfo file, unzipDir.entryInfoList(namefilters)) { // svg files
		mw->copyToSvgFolder(file);
	}

	namefilters.clear();
	namefilters << ZIP_PART+"*";
	foreach(QFileInfo file, unzipDir.entryInfoList(namefilters)) { // part files
		mw->copyToPartsFolder(file);
	}

	namefilters.clear();
	namefilters << "*"+FritzingSketchExtension;

	// the sketch itself
	mw->load(unzipDir.entryInfoList(namefilters)[0].filePath(), false);
	mw->setWindowModified(true);

	closeIfEmptySketch(mw);
}

void MainWindow::copyToSvgFolder(const QFileInfo& file, const QString &destFolder) {
	QFile svgfile(file.filePath());
	// let's make sure that we remove just the suffix
	QString fileName = file.fileName().remove(QRegExp("^"+ZIP_SVG));
	QString viewFolder = fileName.left(fileName.indexOf("."));
	fileName.remove(viewFolder+".");

	QString destFilePath =
		getApplicationSubFolderPath("parts")+"/svg/"+destFolder+"/"+viewFolder+"/"+fileName;

	backupExistingFileIfExists(destFilePath);
	if(svgfile.copy(destFilePath)) {
		m_alienFiles << destFilePath;
	}
}

void MainWindow::copyToPartsFolder(const QFileInfo& file, const QString &destFolder) {
	QFile partfile(file.filePath());
	// let's make sure that we remove just the suffix
	QString destFilePath =
		getApplicationSubFolderPath("parts")+"/"+destFolder+"/"+file.fileName().remove(QRegExp("^"+ZIP_PART));

	backupExistingFileIfExists(destFilePath);
	if(partfile.copy(destFilePath)) {
		m_alienFiles << destFilePath;
	}
	ModelPart *mp = m_refModel->loadPart(destFilePath, true);
	m_paletteWidget->addPart(mp);
}

void MainWindow::binSaved(bool hasPartsFromBundled) {
	if(hasPartsFromBundled) {
		// the bin will need those parts, so just keep them
		m_alienFiles.clear();
		resetTempFolder();
	}
}

#undef ZIP_PART
#undef ZIP_SVG


void MainWindow::backupExistingFileIfExists(const QString &destFilePath) {
	if(QFileInfo(destFilePath).exists()) {
		if(m_tempDir.path() == ".") {
			m_tempDir = QDir::temp();
			createFolderAnCdIntoIt(m_tempDir, getRandText());
			DebugDialog::debug("debug folder for overwritten files: "+m_tempDir.path());
		}

		QString fileBackupName = QFileInfo(destFilePath).fileName();
		m_filesReplacedByAlienOnes << destFilePath;
		QFile file(destFilePath);
		bool alreadyExists = file.exists();
		file.copy(m_tempDir.path()+"/"+fileBackupName);

		if(alreadyExists) {
			file.remove(destFilePath);
		}
	}
}

void MainWindow::recoverBackupedFiles() {
	foreach(QString originalFilePath, m_filesReplacedByAlienOnes) {
		QFile file(m_tempDir.path()+"/"+QFileInfo(originalFilePath).fileName());
		if(file.exists(originalFilePath)) {
			file.remove();
		}
		file.copy(originalFilePath);
	}
	resetTempFolder();
}

void MainWindow::resetTempFolder() {
	if(m_tempDir.path() != ".") {
		rmdir(m_tempDir);
		m_tempDir = QDir::temp();
	}
	m_filesReplacedByAlienOnes.clear();
}

void MainWindow::routingStatusSlot(int netCount, int netRoutedCount, int connectorsLeftToRoute, int jumpers) {
	QString theText;
	if (netCount == 0) {
		theText = tr("No connections to route");
	} else if (netCount == netRoutedCount) {
		if (jumpers == 0) {
			theText = tr("Routing completed");
		}
		else {
			theText = tr("Routing completed using %n jumper(s)", "", jumpers);
		}
	} else {
		theText = tr("%1 of %2 nets routed - %n connector(s) still to be routed", "", connectorsLeftToRoute)
			.arg(netRoutedCount)
			.arg(netCount);
	}
	m_routingStatusLabel->setLabelText(theText);

	updateTraceMenu();
}


void MainWindow::clearRoutingSlot(SketchWidget * sketchWidget, QUndoCommand * parentCommand) {
	Q_UNUSED(sketchWidget);

	m_pcbGraphicsView->clearRouting(parentCommand);
}

QMenu *MainWindow::breadboardItemMenu() {
	QMenu *menu = new QMenu(QObject::tr("Part"), this);
	menu->addAction(m_rotate90cwAct);
	menu->addAction(m_rotate180Act);
	menu->addAction(m_rotate90ccwAct);
	menu->addAction(m_flipHorizontalAct);
	menu->addAction(m_flipVerticalAct);
	return viewItemMenuAux(menu);
}

QMenu *MainWindow::schematicItemMenu() {
	QMenu *menu = new QMenu(QObject::tr("Part"), this);
	menu->addAction(m_rotate90cwAct);
	menu->addAction(m_rotate180Act);
	menu->addAction(m_rotate90ccwAct);
	menu->addAction(m_flipHorizontalAct);
	menu->addAction(m_flipVerticalAct);
	return viewItemMenuAux(menu);
}

QMenu *MainWindow::pcbItemMenu() {
	QMenu *menu = new QMenu(QObject::tr("Part"), this);
	menu->addAction(m_rotate90cwAct);
	menu->addAction(m_rotate180Act);
	menu->addAction(m_rotate90ccwAct);
	menu = viewItemMenuAux(menu);
	return menu;
}

QMenu *MainWindow::pcbWireMenu() {
	QMenu *menu = new QMenu(QObject::tr("Wire"), this);
	menu->addAction(m_bringToFrontAct);
	menu->addAction(m_bringForwardAct);
	menu->addAction(m_sendBackwardAct);
	menu->addAction(m_sendToBackAct);
	menu->addSeparator();
	menu->addAction(m_createTraceAct);
	menu->addAction(m_createJumperAct);
	menu->addAction(m_excludeFromAutorouteAct);
	menu->addSeparator();
	menu->addAction(m_deleteAct);
#ifndef QT_NO_DEBUG
	menu->addSeparator();
	menu->addAction(m_infoViewOnHoverAction);
#endif

    connect(
    	menu,
    	SIGNAL(aboutToShow()),
    	this,
    	SLOT(updateWireMenu())
    );

	return menu;
}

QMenu *MainWindow::viewItemMenuAux(QMenu* menu) {
	menu->addSeparator();
	menu->addAction(m_bringToFrontAct);
	menu->addAction(m_bringForwardAct);
	menu->addAction(m_sendBackwardAct);
	menu->addAction(m_sendToBackAct);
	menu->addSeparator();
	menu->addAction(m_copyAct);
	menu->addAction(m_duplicateAct);
	menu->addAction(m_deleteAct);
	menu->addSeparator();
	menu->addAction(m_openInPartsEditorAct);
	menu->addAction(m_addToBinAct);
	menu->addSeparator();
	menu->addAction(m_showPartLabelAct);
#ifndef QT_NO_DEBUG
	menu->addSeparator();
	menu->addAction(m_infoViewOnHoverAction);
	//menu->addAction(m_swapPartAction);
#endif

    connect(
    	menu,
    	SIGNAL(aboutToShow()),
    	this,
    	SLOT(updatePartMenu())
    );

    return menu;
}

void MainWindow::applyReadOnlyChange(bool isReadOnly) {
	Q_UNUSED(isReadOnly);
	//m_saveAct->setDisabled(isReadOnly);
}

void MainWindow::currentNavigatorChanged(MiniViewContainer * miniView)
{
	int index = m_navigators.indexOf(miniView);
	if (index < 0) return;

	int oldIndex = m_tabWidget->currentIndex();
	if (oldIndex == index) return;

	this->m_tabWidget->setCurrentIndex(index);
}

void MainWindow::viewSwitchedTo(int viewIndex) {
	m_tabWidget->setCurrentIndex(viewIndex);
}

const QString MainWindow::fritzingTitle() {
	if (m_currentGraphicsView == NULL) {
		return FritzingWindow::fritzingTitle();
	}

	QString fritzing = FritzingWindow::fritzingTitle();
	return tr("%1 - [%2]").arg(fritzing).arg(m_currentGraphicsView->viewName());
}

QAction *MainWindow::raiseWindowAction() {
	return m_raiseWindowAct;
}

void MainWindow::raiseAndActivate() {
	if(isMinimized()) {
		showNormal();
	}
	raise();
	QTimer *timer = new QTimer(this);
	timer->setSingleShot(true);
	connect(timer, SIGNAL(timeout()), this, SLOT(activateWindowAux()));
	timer->start(20);
}

void MainWindow::activateWindowAux() {
	activateWindow();
}

void MainWindow::updateRaiseWindowAction() {
	QString actionText;
	QFileInfo fileInfo(m_fileName);
	if(fileInfo.exists()) {
		int lastSlashIdx = m_fileName.lastIndexOf("/");
		int beforeLastSlashIdx = m_fileName.left(lastSlashIdx).lastIndexOf("/");
		actionText = beforeLastSlashIdx > -1 && lastSlashIdx > -1 ? "..." : "";
		actionText += m_fileName.right(m_fileName.size()-beforeLastSlashIdx-1);
	} else {
		actionText = m_fileName;
	}
	m_raiseWindowAct->setText(actionText);
	m_raiseWindowAct->setToolTip(m_fileName);
	m_raiseWindowAct->setStatusTip("raise \""+m_fileName+"\" window");
}

QSizeGrip *MainWindow::sizeGrip() {
	return m_sizeGrip;
}

QStatusBar *MainWindow::realStatusBar() {
	return m_statusBar;
}

void MainWindow::moveEvent(QMoveEvent * event) {
	FritzingWindow::moveEvent(event);
	emit mainWindowMoved(this);
}

bool MainWindow::event(QEvent * e) {
	switch (e->type()) {
		case QEvent::WindowActivate:
			changeActivation(true);
			break;
		case QEvent::WindowDeactivate:
			changeActivation(false);
			break;
		default:
			break;
	}
	return FritzingWindow::event(e);
}

void MainWindow::resizeEvent(QResizeEvent * event) {
	m_sizeGrip->rearrange();
	FritzingWindow::resizeEvent(event);
}

void MainWindow::showInViewHelp() {
	//delete m_helper;
	if (m_helper == NULL) {
		m_helper = new Helper(this, true);
		return;
	}

	bool toggle = !m_helper->helpVisible(m_tabWidget->currentIndex());
	showAllFirstTimeHelp(toggle);

	/*
	m_helper->toggleHelpVisibility(m_tabWidget->currentIndex());
	*/

	m_showInViewHelpAct->setChecked(m_helper->helpVisible(m_tabWidget->currentIndex()));
}


void MainWindow::showAllFirstTimeHelp(bool show) {
	if (m_helper) {
		for (int i = 0; i < 3; i++) {
			m_helper->setHelpVisibility(i, show);
		}
	}
	m_showInViewHelpAct->setChecked(show);
}

void MainWindow::enableCheckUpdates(bool enabled)
{
	m_checkForUpdatesAct->setEnabled(enabled);
}

void MainWindow::saveAsModule() {
	SaveAsModuleDialog dialog(m_breadboardGraphicsView, this);
	int result = dialog.exec();
	if (result == QDialog::Accepted) {
		QList<ViewLayer::ViewLayerID> partViewLayerIDs;
		partViewLayerIDs << ViewLayer::BreadboardBreadboard << ViewLayer::Breadboard;
		QList<ViewLayer::ViewLayerID> wireViewLayerIDs;
		wireViewLayerIDs << ViewLayer::BreadboardWire;
		QSizeF imageSize;
		QString svg = m_breadboardGraphicsView->renderToSVG(FSvgRenderer::printerScale(), partViewLayerIDs, wireViewLayerIDs, false, imageSize);
		if (svg.isEmpty()) {
			// tell the user something reasonable
			return;
		}

		QFile file("test.svg");
		file.open(QIODevice::WriteOnly);
		QTextStream out(&file);
		out << svg;
		file.close();
	}
}
