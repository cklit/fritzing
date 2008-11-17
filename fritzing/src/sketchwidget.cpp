/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-08 Fachhochschule Potsdam - http://fh-potsdam.de

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
#include <QGraphicsScene>
#include <QPoint>
#include <QMatrix>
#include <QtAlgorithms>
#include <QPen>
#include <QColor>
#include <QRubberBand>
#include <QLine>
#include <QHash>
#include <QBrush>
#include <QGraphicsItem>
#include <QMainWindow>

#include "paletteitem.h"
#include "wire.h"
#include "commands.h"
#include "modelpart.h"
#include "debugdialog.h"
#include "layerkinpaletteitem.h"
#include "sketchwidget.h"
#include "bettertriggeraction.h"
#include "connectoritem.h"
#include "bus.h"
#include "virtualwire.h"
#include "busconnectoritem.h"
#include "itemdrag.h"
#include "layerattributes.h"
#include "waitpushundostack.h"
#include "zoomcombobox.h"
#include "autorouter1.h"

SketchWidget::SketchWidget(ItemBase::ViewIdentifier viewIdentifier, QWidget *parent, int size, int minSize)
    : InfoGraphicsView(parent)
{
	m_ignoreSelectionChangeEvents = false;
	m_droppingItem = NULL;
	m_chainDrag = m_chainDragging = false;
	m_connectorDragWire = NULL;
	m_holdingSelectItemCommand = NULL;
	m_viewIdentifier = viewIdentifier;
	m_scaleValue = 100;
	m_maxScaleValue = 2000;
	m_minScaleValue = 1;
	setAlignment(Qt::AlignLeft | Qt::AlignTop);
	setDragMode(QGraphicsView::RubberBandDrag);
    setFrameStyle(QFrame::Sunken | QFrame::StyledPanel);
    setAcceptDrops(true);
	setRenderHint(QPainter::Antialiasing, true);
	//setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	//setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	setTransformationAnchor(QGraphicsView::AnchorViewCenter);
	//setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
	//setTransformationAnchor(QGraphicsView::NoAnchor);
    QGraphicsScene* scene = new QGraphicsScene(this);
    this->setScene(scene);

    //this->scene()->setSceneRect(0,0, 1000, 1000);

    // Setting the scene rect here seems to mean it never resizes when the user drags an object
    // outside the sceneRect bounds.  So catch some signal and do the resize manually?
    // this->scene()->setSceneRect(0, 0, 500, 500);

    // if the sceneRect isn't set, the view seems to grow and scroll gracefully as new items are added
    // however, it doesn't shrink if items are removed.

    // a bit of a hack so that, when there is no scenerect set,
    // the first item dropped into the scene doesn't leap to the top left corner
    // as the scene resizes to fit the new item
   	QGraphicsLineItem * item = new QGraphicsLineItem();
    item->setLine(1,1,1,1);
    this->scene()->addItem(item);
    item->setVisible(false);

    connect(this->scene(), SIGNAL(selectionChanged()), this, SLOT(scene_selectionChanged()));

    connect(QApplication::clipboard(),SIGNAL(changed(QClipboard::Mode)),this,SLOT(restartPasteCount()));
    restartPasteCount(); // the first time

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    resize(size, size);
    setMinimumSize(minSize, minSize);


	//QGraphicsItemGroup * group = new QGraphicsItemGroup();
	//group->setZValue(10);
	//this->scene()->addItem(group);
	//
	//QPen pen(QBrush(QColor(255, 0, 0)), 5);
	//
    //item = new QGraphicsLineItem();
    //item->setLine(300,300,500,500);
    //this->scene()->addItem(item);
    //item->setZValue(0.5);
	//item->setFlags(QGraphicsItem::ItemIsSelectable  | QGraphicsItem::ItemIsMovable );
	//item->setPen(pen);
   	//group->addToGroup(item);
    //
    //item = new QGraphicsLineItem();
    //item->setLine(500,300,300,500);
    //this->scene()->addItem(item);
    //item->setZValue(10.5);
	//item->setFlags(QGraphicsItem::ItemIsSelectable  | QGraphicsItem::ItemIsMovable );
	//item->setPen(pen);
    //group->addToGroup(item);

    m_lastPaletteItemSelected = NULL;
}

void SketchWidget::restartPasteCount() {
	m_pasteCount = 1;
}

QUndoStack* SketchWidget::undoStack() {
	return m_undoStack;
}

void SketchWidget::setUndoStack(WaitPushUndoStack * undoStack) {
	m_undoStack = undoStack;
}

ItemBase* SketchWidget::loadFromModel(ModelPart *modelPart, const ViewGeometry& viewGeometry){
	// assumes modelPart has already been added to the sketch
	// or you're in big trouble when you delete the item
	return addItemAux(modelPart, viewGeometry, ItemBase::getNextID(), NULL, true);
}

void SketchWidget::loadFromModel() {
	ModelPart* root = m_sketchModel->root();
	QHash<long, ItemBase *> newItems;
	QHash<ItemBase *, QDomElement *> itemDoms;

	QString viewName = ItemBase::viewIdentifierXmlName(m_viewIdentifier);

	// first make the parts
	foreach (QObject * object, root->children()) {
		ModelPart* mp = qobject_cast<ModelPart *>(object);
		if (mp == NULL) continue;

		QDomElement instance = mp->instanceDomElement();
		if (instance.isNull()) continue;

		QDomElement views = instance.firstChildElement("views");
		if (views.isNull()) continue;

		QDomElement view = views.firstChildElement(viewName);
		if (view.isNull()) continue;

		QDomElement geometry = view.firstChildElement("geometry");
		if (geometry.isNull()) continue;

		ViewGeometry viewGeometry(geometry);
		// use a function of the model index to ensure the same parts have the same ID across views
		ItemBase * item = addItemAux(mp, viewGeometry, ItemBase::getNextID(mp->modelIndex()), NULL, true);
		if (item != NULL) {
			PaletteItem * paletteItem = dynamic_cast<PaletteItem *>(item);
			if (paletteItem) {
				// wires don't have transforms
				paletteItem->setTransforms();
			}
			else {
				Wire * wire = dynamic_cast<Wire *>(item);
				if (wire != NULL) {
					QDomElement extras = view.firstChildElement("wireExtras");
					wire->setExtras(extras);
				}
			}

			// use the modelIndex from mp, not from the newly created item, because we're mapping from the modelIndex in the xml file
			newItems.insert(mp->modelIndex(), item);
			itemDoms.insert(item, new QDomElement(view));
		}
	}

	foreach (ItemBase * itemBase, itemDoms.keys()) {
		QDomElement * dom = itemDoms.value(itemBase);
		itemBase->restoreConnections(*dom,  newItems);
		delete dom;
	}


	// redraw the ratsnest
	if ((m_viewIdentifier == ItemBase::PCBView) || (m_viewIdentifier == ItemBase::SchematicView)) {
		foreach (ItemBase * newItem, newItems.values()) {
			foreach (QGraphicsItem * childItem, newItem->childItems()) {
				ConnectorItem * fromConnectorItem = dynamic_cast<ConnectorItem *>(childItem);
				if (fromConnectorItem == NULL) continue;

				foreach (ConnectorItem * toConnectorItem, fromConnectorItem->connectedToItems()) {
					/*
					DebugDialog::debug(QString("restoring ratsnest: %1 %2, %3 %4")
						.arg(fromConnectorItem->attachedToTitle())
						.arg(fromConnectorItem->connectorStuffID())
						.arg(toConnectorItem->attachedToTitle())
						.arg(toConnectorItem->connectorStuffID())
						);
					*/
					dealWithRatsnest(fromConnectorItem, toConnectorItem, true);
				}
			}
		}
	}

	if (m_viewIdentifier == ItemBase::PCBView) {
		bool autorouted = true;
		QList<ConnectorItem *> allConnectorItems;
		foreach (QGraphicsItem * item, scene()->items()) {
			ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(item);
			if (connectorItem == NULL) continue;
			if (connectorItem->attachedToItemType() != ModelPart::Part) continue;
			allConnectorItems.append(connectorItem);
		}

		while (allConnectorItems.count() > 0) {
			QList<ConnectorItem *> connectorItems;
			QList<ConnectorItem *> ratPartsConnectorItems;
			QList<ConnectorItem *> tracePartsConnectorItems;
			QList<BusConnectorItem *> busConnectorItems;
			connectorItems.append(allConnectorItems[0]);
			BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, true, ViewGeometry::RatsnestFlag);
			BusConnectorItem::collectParts(connectorItems, ratPartsConnectorItems);

			connectorItems.clear();
			busConnectorItems.clear();
			connectorItems.append(allConnectorItems[0]);
			BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, true, ViewGeometry::JumperFlag | ViewGeometry::TraceFlag);
			BusConnectorItem::collectParts(connectorItems, tracePartsConnectorItems);
			if (tracePartsConnectorItems.count() != ratPartsConnectorItems.count()) {
				autorouted = false;
				allConnectorItems.clear();
				break;
			}

			foreach (ConnectorItem * ci, ratPartsConnectorItems) {
				// don't check these parts again
				allConnectorItems.removeOne(ci);
				DebugDialog::debug(QString("allparts count %1").arg(allConnectorItems.count()) );
			}
		}


		if (autorouted) {
			// TODO need to figure out which net each wire belongs to
			// or save the ratsnest wires so they can simply be reloaded
			DebugDialog::debug("autorouted");
			foreach (QGraphicsItem * item, scene()->items()) {
				Wire * wire = dynamic_cast<Wire *>(item);
				if (wire == NULL) continue;

				if (wire->getRatsnest()) {
					wire->setRouted(true);
					wire->setColorString("unrouted", 0.35);
				}
			}
		}
	}

	this->scene()->clearSelection();
	cleanUpWires(false);
}

ItemBase * SketchWidget::addItem(const QString & moduleID, BaseCommand::CrossViewType crossViewType, const ViewGeometry & viewGeometry, long id) {
	if (m_paletteModel == NULL) return NULL;

	ItemBase * itemBase = NULL;
	ModelPart * modelPart = m_paletteModel->retrieveModelPart(moduleID);
	if (modelPart != NULL) {
		QApplication::setOverrideCursor(Qt::WaitCursor);
		statusMessage(tr("loading part"));
		itemBase = addItem(modelPart, crossViewType, viewGeometry, id);
		statusMessage(tr("done loading"), 2000);
		QApplication::restoreOverrideCursor();
	}

	return itemBase;
}

ItemBase * SketchWidget::addItem(ModelPart * modelPart, BaseCommand::CrossViewType crossViewType, const ViewGeometry & viewGeometry, long id, PaletteItem* partsEditorPaletteItem) {
	modelPart = m_sketchModel->addModelPart(m_sketchModel->root(), modelPart);
	ItemBase * newItem = addItemAux(modelPart, viewGeometry, id, partsEditorPaletteItem, true);
	if (crossViewType == BaseCommand::CrossView) {
		emit itemAddedSignal(modelPart, viewGeometry, id);
	}

	return newItem;
}

ItemBase * SketchWidget::addItemAux(ModelPart * modelPart, const ViewGeometry & viewGeometry, long id, PaletteItem* partsEditorPaletteItem, bool doConnectors )
{
	Q_UNUSED(partsEditorPaletteItem);

	if (doConnectors) {
		modelPart->initConnectors();    // is a no-op if connectors already in place
	}

	if (modelPart->itemType() == ModelPart::Wire ) {
		bool virtualWire = viewGeometry.getVirtual();
		Wire * wire = NULL;
		if (virtualWire) {
			wire = new VirtualWire(modelPart, m_viewIdentifier, viewGeometry, id, m_itemMenu);
             			wire->setUp(getWireViewLayerID(viewGeometry), m_viewLayers);

//			if (m_viewIdentifier == ItemBase::BreadboardView) {
//				// hide all virtual wires in breadboard view
//				wire->setVisible(false);
//			}


		}
		else {
			wire = new Wire(modelPart, m_viewIdentifier, viewGeometry, id, m_itemMenu);
			wire->setUp(getWireViewLayerID(viewGeometry), m_viewLayers);
		}

		bool succeeded = connect(wire, SIGNAL(wireChangedSignal(Wire*, QLineF, QLineF, QPointF, QPointF, ConnectorItem *, ConnectorItem *)	),
				this, SLOT(wire_wireChanged(Wire*, QLineF, QLineF, QPointF, QPointF, ConnectorItem *, ConnectorItem *)),
				Qt::DirectConnection);		// DirectConnection means call the slot directly like a subroutine, without waiting for a thread or queue
		succeeded = succeeded && connect(wire, SIGNAL(wireSplitSignal(Wire*, QPointF, QPointF, QLineF)),
				this, SLOT(wire_wireSplit(Wire*, QPointF, QPointF, QLineF)));
		succeeded = succeeded && connect(wire, SIGNAL(wireJoinSignal(Wire*, ConnectorItem *)),
				this, SLOT(wire_wireJoin(Wire*, ConnectorItem*)));
		if (!succeeded) {
			DebugDialog::debug("wire signal connect failed");
		}

		addToScene(wire, wire->viewLayerID());
		DebugDialog::debug(QString("adding wire %1 %2").arg(wire->id()).arg(m_viewIdentifier) );

		checkNewSticky(wire);
		return wire;
	}
	else {
    	PaletteItem* paletteItem = new PaletteItem(modelPart, m_viewIdentifier, viewGeometry, id, m_itemMenu);
		DebugDialog::debug(QString("adding part %1 %2 %3").arg(id).arg(paletteItem->title()).arg(m_viewIdentifier) );
    	ItemBase * itemBase = addPartItem(modelPart, paletteItem, doConnectors);
		if (itemBase->itemType() == ModelPart::Breadboard) {
			if (m_viewIdentifier != ItemBase::BreadboardView) {
				// don't need to see the breadboard in the other views
				// but it's there so connections can be more easily synched between views
				itemBase->setVisible(false);
			}
		}
		return itemBase;
	}

}

void SketchWidget::checkSticky(ItemBase * item, QUndoCommand * parentCommand) {
	if (item->sticky()) {
		foreach (ItemBase * s, item->sticking().keys()) {
			ViewGeometry viewGeometry = s->getViewGeometry();
			s->saveGeometry();
			new MoveItemCommand(this, s->id(), viewGeometry, s->getViewGeometry(), parentCommand);
		}

		stickyScoop(item, parentCommand);
	}
	else {
		ItemBase * stickyOne = overSticky(item);
		ItemBase * wasStickyOne = item->stuckTo();
		if (stickyOne != wasStickyOne) {
			if (wasStickyOne != NULL) {
				new StickyCommand(this, wasStickyOne->id(), item->id(), false, parentCommand);
			}
			if (stickyOne != NULL) {
				new StickyCommand(this, stickyOne->id(), item->id(), true, parentCommand);
			}
		}
	}
}

void SketchWidget::checkNewSticky(ItemBase * itemBase) {
	if (itemBase->sticky()) {
		stickyScoop(itemBase, NULL);
	}
	else {
		ItemBase * stickyOne = overSticky(itemBase);
		if (stickyOne != NULL) {
			// would prefer to handle this via command object, but it's tricky because an item dropped in a given view
			// would only need to stick in a different view
			stickyOne->addSticky(itemBase, true);
			itemBase->addSticky(stickyOne, true);
		}
	}
}

PaletteItem* SketchWidget::addPartItem(ModelPart * modelPart, PaletteItem * paletteItem, bool doConnectors) {

	ViewLayer::ViewLayerID viewLayerID = getViewLayerID(modelPart);

	if (paletteItem->renderImage(modelPart, m_viewIdentifier, m_viewLayers, viewLayerID, true, doConnectors)) {
		addToScene(paletteItem, paletteItem->viewLayerID());
		paletteItem->loadLayerKin(m_viewLayers);
		for (int i = 0; i < paletteItem->layerKin().count(); i++) {
			LayerKinPaletteItem * lkpi = paletteItem->layerKin()[i];
			this->scene()->addItem(lkpi);
			lkpi->setHidden(!layerIsVisible(lkpi->viewLayerID()));
		}

		connect(paletteItem, SIGNAL(connectionChangedSignal(ConnectorItem *, ConnectorItem *, bool)	),
				this, SLOT(item_connectionChanged(ConnectorItem *, ConnectorItem *, bool)),
				Qt::DirectConnection);   // DirectConnection means call the slot directly like a subroutine, without waiting for a thread or queue

		checkNewSticky(paletteItem);
		return paletteItem;
	}
	else {
		// nobody falls through to here now?

		QMessageBox::information(dynamic_cast<QMainWindow *>(this->window()), QObject::tr("Fritzing"),
								 QObject::tr("The file %1 is not a Fritzing file (1).").arg(modelPart->modelPartStuff()->path()) );


		DebugDialog::debug(QString("addPartItem renderImage failed %1").arg(modelPart->moduleID()) );

		//paletteItem->modelPart()->removeViewItem(paletteItem);
		//delete paletteItem;
		//return NULL;
		scene()->addItem(paletteItem);
		//paletteItem->setVisible(false);
		return paletteItem;
	}
}

void SketchWidget::addToScene(ItemBase * item, ViewLayer::ViewLayerID viewLayerID) {
	scene()->addItem(item);
 	item->setSelected(true);
 	item->setHidden(!layerIsVisible(viewLayerID));
	if (!item->getVirtual()) {
		item->setFlag(QGraphicsItem::ItemIsMovable, true);
	}
}

ItemBase * SketchWidget::findItem(long id) {
	// TODO:  replace scene()->items()

	QList<QGraphicsItem *> items = this->scene()->items();
	for (int i = 0; i < items.size(); i++) {
		ItemBase* base = ItemBase::extractItemBase(items[i]);
		if (base == NULL) continue;

		if (base->id() == id) {
			return base;
		}
	}

	return NULL;
}

void SketchWidget::deleteItem(long id, bool deleteModelPart, bool doEmit) {
	DebugDialog::debug(tr("delete item %1 %2").arg(id).arg(doEmit) );
	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		deleteItem(pitem, deleteModelPart, doEmit);
	}

}

void SketchWidget::deleteItem(ItemBase * itemBase, bool deleteModelPart, bool doEmit)
{
	if (m_infoView != NULL) {
		m_infoView->unregisterCurrentItemIf(itemBase->id());
	}
	m_lastSelected.removeOne(itemBase);

	long id = itemBase->id();
	itemBase->removeLayerKin();
	this->scene()->removeItem(itemBase);
	DebugDialog::debug(tr("delete item %1 %2").arg(id).arg(itemBase->title()) );

	ModelPart * modelPart = itemBase->modelPart();
	m_sketchModel->removeModelPart(modelPart);
	delete itemBase;

	if (doEmit) {
		emit itemDeletedSignal(id);
	}

	if (deleteModelPart) {
		delete modelPart;
	}
}

void SketchWidget::deleteItem() {
	cutDeleteAux("Delete");
}

void SketchWidget::cutDeleteAux(QString undoStackMessage) {
    // get sitems first, before calling stackSelectionState
    // because selectedItems will return an empty list
	const QList<QGraphicsItem *> sitems = scene()->selectedItems();

	int delCount = 0;
	ItemBase * saveBase = NULL;
	for (int i = 0; i < sitems.size(); i++) {

		ItemBase *itemBase = ItemBase::extractTopLevelItemBase(sitems[i]);
		if (itemBase == NULL) continue;

		saveBase = itemBase;
		delCount++;
	}

	if (delCount <= 0) {
		return;
	}

	QString string;

	if (delCount == 1) {
		string = tr("%1 %2").arg(undoStackMessage).arg(saveBase->modelPart()->title());
	}
	else {
		string = tr("%1 %2 items").arg(undoStackMessage).arg(QString::number(delCount));
	}

	QUndoCommand * parentCommand = new QUndoCommand(string);
	new CleanUpWiresCommand(this, false, parentCommand);
	parentCommand->setText(string);

    stackSelectionState(false, parentCommand);

	QSet <VirtualWire *> virtualWires;
	QList<ItemBase *> deletedItems;
	QList<BusConnectorItem *> busConnectorItems;
	collectBusConnectorItems(busConnectorItems);

	// the theory is that we only need to disconnect virtual wires at this point
	// and not regular connectors
	// since in figuring out how to manage busConnections
	// all parts are connected to busConnections only by virtual wires

    for (int i = 0; i < sitems.count(); i++) {
    	ItemBase * itemBase = ItemBase::extractTopLevelItemBase(sitems[i]);
    	if (itemBase == NULL) continue;

		ModelPart * modelPart = itemBase->modelPart();
		if (modelPart == NULL) continue;

		QMultiHash<ConnectorItem *, ConnectorItem *>  connectorHash;

		itemBase->collectConnectors(connectorHash, this->scene());
		deletedItems.append(itemBase);

		// now prepare to disconnect all the deleted item's connectors
		foreach (ConnectorItem * fromConnectorItem,  connectorHash.uniqueKeys()) {
			if (fromConnectorItem->attachedTo()->getVirtual()) {
				virtualWires.insert(dynamic_cast<VirtualWire *>(fromConnectorItem->attachedTo()));
				continue;
			}
			foreach (ConnectorItem * toConnectorItem, connectorHash.values(fromConnectorItem)) {
				if (toConnectorItem->attachedTo()->getVirtual()) {
					virtualWires.insert(dynamic_cast<VirtualWire *>(toConnectorItem->attachedTo()));
					continue;
				}
				extendChangeConnectionCommand(fromConnectorItem, toConnectorItem,
											  false, false, parentCommand);
				fromConnectorItem->tempRemove(toConnectorItem);
				toConnectorItem->tempRemove(fromConnectorItem);
			}
		}
   	}

	cleanUpVirtualWires(virtualWires, busConnectorItems, parentCommand);

	for (int i = 0; i < deletedItems.count(); i++) {
		ItemBase * itemBase = deletedItems[i];
		makeDeleteItemCommand(itemBase, parentCommand);
		emit deleteItemSignal(itemBase->id(), parentCommand);			// let the other views add the command
	}


	new CleanUpWiresCommand(this, true, parentCommand);
   	m_undoStack->push(parentCommand);


}

void SketchWidget::reorgBuses(QList<BusConnectorItem *> & busConnectorItems, QUndoCommand * parentCommand) {
	foreach (BusConnectorItem * busConnectorItem, busConnectorItems) {
		// check whether bus needs to be unmerged and/or remerged with a different busConnector
		QList<ConnectorItem *> connectorItems;
		QList<BusConnectorItem *> equalBusConnectorItems;
		connectorItems.append(busConnectorItem);
		BusConnectorItem::collectEqualPotential(connectorItems, equalBusConnectorItems, false, ViewGeometry::NoFlag);
		equalBusConnectorItems.removeOne(busConnectorItem);
		foreach (BusConnectorItem * mergedBusConnectorItem,  busConnectorItem->merged()) {
			if (!equalBusConnectorItems.contains(mergedBusConnectorItem)) {
				new MergeBusCommand(this, mergedBusConnectorItem->attachedToID(), mergedBusConnectorItem->busID(), mergedBusConnectorItem->mapToScene(mergedBusConnectorItem->pos()),
									busConnectorItem->attachedToID(), busConnectorItem->busID(), busConnectorItem->mapToScene(busConnectorItem->pos()),
									false, parentCommand);
				busConnectorItem->unmerge(mergedBusConnectorItem);
			}
		}
		if (!busConnectorItem->isMerged() && equalBusConnectorItems.count() > 0) {
			// remerge
			BusConnectorItem * other = equalBusConnectorItems[0];
			new MergeBusCommand(this, other->attachedToID(), other->busID(), other->mapToScene(other->pos()),
								busConnectorItem->attachedToID(), busConnectorItem->busID(), busConnectorItem->mapToScene(busConnectorItem->pos()),
								true, parentCommand);
			busConnectorItem->merge(other);
		}
	}
}

void SketchWidget::deleteVirtualWires(QSet<VirtualWire *> & virtualWires, QUndoCommand * parentCommand) {
	QSetIterator<VirtualWire *> it(virtualWires);
    while (it.hasNext()) {
		VirtualWire * wire = it.next();
		wire->saveGeometry();
		new DeleteItemCommand(this, BaseCommand::CrossView, wire->modelPart()->moduleID(), wire->getViewGeometry(), wire->id(), parentCommand);
	}
}

void SketchWidget::disconnectVirtualWires(QSet<VirtualWire *> & virtualWires, QUndoCommand * parentCommand) {
	// set up disconnect and delete for virtual wires
	QSetIterator<VirtualWire *> it(virtualWires);
    while (it.hasNext()) {
		VirtualWire * wire = it.next();
		disconnectVirtualWire(wire->connector0(), parentCommand);
		disconnectVirtualWire(wire->connector1(), parentCommand);
	}
	it.toFront();
    while (it.hasNext()) {
		VirtualWire * wire = it.next();
		// now really disconnect the virtual wires to get ready to clean up busConnectorItems
		wire->tempRemoveAllConnections();
	}
}

void SketchWidget::disconnectVirtualWire(ConnectorItem * fromConnectorItem, QUndoCommand * parentCommand) {
	QList<ConnectorItem *> connectedToItems = fromConnectorItem->connectedToItems();
	for (int i = 0; i < connectedToItems.count(); i++) {
		ConnectorItem * toConnectorItem = connectedToItems[i];
		BusConnectorItem * busConnectorItem = dynamic_cast<BusConnectorItem *>(toConnectorItem);
		new ChangeConnectionCommand(this, BaseCommand::CrossView, toConnectorItem->attachedToID(),
									(busConnectorItem == NULL) ? toConnectorItem->connectorStuffID() : busConnectorItem->busID(),
									fromConnectorItem->attachedToID(), fromConnectorItem->connectorStuffID(),
									false, true,
									busConnectorItem != NULL, false, parentCommand);

	}
}

void SketchWidget::extendChangeConnectionCommand(long fromID, const QString & fromConnectorID,
												 long toID, const QString & toConnectorID,
												 bool connect, bool seekLayerKin, QUndoCommand * parentCommand)
{
	ItemBase * fromItem = findItem(fromID);
	if (fromItem == NULL) {
		return;  // for now
	}

	ItemBase * toItem = findItem(toID);
	if (toItem == NULL) {
		return;		// for now
	}

	ConnectorItem * fromConnectorItem = findConnectorItem(fromItem, fromConnectorID, seekLayerKin);
	if (fromConnectorItem == NULL) return; // for now

	ConnectorItem * toConnectorItem = findConnectorItem(toItem, toConnectorID, seekLayerKin);
	if (toConnectorItem == NULL) return; // for now

	extendChangeConnectionCommand(fromConnectorItem, toConnectorItem, connect, seekLayerKin, parentCommand);
}

void SketchWidget::extendChangeConnectionCommand(ConnectorItem * fromConnectorItem, ConnectorItem * toConnectorItem,
												 bool connect, bool seekLayerKin, QUndoCommand * parentCommand)
{
	// cases:
	//		delete
	//		paste
	//		drop (wire)
	//		drop (part)
	//		move (part)
	//		move (wire)
	//		drag wire end
	//		drag out new wire

	ItemBase * fromItem = fromConnectorItem->attachedTo();
	if (fromItem == NULL) {
		return;  // for now
	}

	ItemBase * toItem = toConnectorItem->attachedTo();
	if (toItem == NULL) {
		return;		// for now
	}

	new ChangeConnectionCommand(this, BaseCommand::CrossView,
								fromItem->id(), fromConnectorItem->connectorStuffID(),
								toItem->id(), toConnectorItem->connectorStuffID(),
								connect, seekLayerKin, false, false, parentCommand);
	if (connect) {
		fromConnectorItem->tempConnectTo(toConnectorItem);
		toConnectorItem->tempConnectTo(fromConnectorItem);
		Bus * bus = NULL;
		ItemBase * busOwner = NULL;
		if (fromConnectorItem->bus() != NULL) {
			bus = hookToBus(toConnectorItem, fromConnectorItem, parentCommand);
			busOwner = fromConnectorItem->attachedTo();
		}
		else if (toConnectorItem->bus() != NULL) {
			bus = hookToBus(fromConnectorItem, toConnectorItem, parentCommand);
			busOwner = toConnectorItem->attachedTo();
		}
		prepMergeBuses(fromConnectorItem, parentCommand);
		fromConnectorItem->tempRemove(toConnectorItem);
		toConnectorItem->tempRemove(fromConnectorItem);
	}
	else {
	}
}

Bus * SketchWidget::hookToBus(ConnectorItem * from, ConnectorItem * busTo, QUndoCommand * parentCommand)
{
	Bus * bus = busTo->bus();
	if (bus == NULL) return NULL;

	ItemBase * busBase = busTo->attachedTo();
	DebugDialog::debug(QString("looking for bus %1 on %2 busTo %3 from %4")
		.arg(bus->id())
		.arg(busBase->id())
		.arg(busTo->attachedToTitle())
		.arg(from->attachedToTitle()));

	BusConnectorItem * busConnectorItem = busBase->busConnectorItem(bus);
	if (busConnectorItem == NULL) {
		// this shouldn't happen

	}
	else {
		if (!busConnectorItem->initialized()) {
			new InitializeBusConnectorItemCommand(this, busConnectorItem->attachedToID(), busConnectorItem->busID(),
												  parentCommand);
			// do it now anyway
			busConnectorItem->initialize(m_viewIdentifier);
		}

	}
	createWire(from, busConnectorItem, true,  ViewGeometry::VirtualFlag, true, BaseCommand::CrossView, parentCommand);
	return bus;
}

long SketchWidget::createWire(ConnectorItem * from, ConnectorItem * to, bool toIsBus, ViewGeometry::WireFlags wireFlags,
							  bool addItNow, BaseCommand::CrossViewType crossViewType, QUndoCommand * parentCommand)
{
	long newID = ItemBase::getNextID();
	ViewGeometry viewGeometry;
	QPointF fromPos = from->sceneAdjustedTerminalPoint();
	viewGeometry.setLoc(fromPos);
	QPointF toPos = to->sceneAdjustedTerminalPoint();
	QLineF line(0, 0, toPos.x() - fromPos.x(), toPos.y() - fromPos.y());
	viewGeometry.setLine(line);
	viewGeometry.setWireFlags(wireFlags);

	DebugDialog::debug(QString("creating virtual wire %11: %1, flags: %6, from %7 %8, to %9 %10, frompos: %2 %3, topos: %4 %5")
		.arg(newID)
		.arg(fromPos.x()).arg(fromPos.y())
		.arg(toPos.x()).arg(toPos.y())
		.arg(wireFlags)
		.arg(from->attachedToTitle()).arg(from->connectorStuffID())
		.arg(to->attachedToTitle()).arg(to->connectorStuffID())
		.arg(m_viewIdentifier)
		);

	new AddItemCommand(this, crossViewType, Wire::moduleIDName, viewGeometry, newID, parentCommand, false);
	new ChangeConnectionCommand(this, crossViewType, from->attachedToID(), from->connectorStuffID(),
			newID, "connector0", true, true, false, false, parentCommand);
	new ChangeConnectionCommand(this, crossViewType, to->attachedToID(), to->connectorStuffID(),
			newID, "connector1", true, true, toIsBus, false, parentCommand);

	if (addItNow) {
		ItemBase * newItemBase = addItemAux(m_paletteModel->retrieveModelPart(Wire::moduleIDName), viewGeometry, newID, NULL, true);
		if (newItemBase) {
			tempConnectWire(newItemBase, from, to);
			m_temporaries.append(newItemBase);
		}
	}

	return newID;
}

void SketchWidget::prepMergeBuses(ConnectorItem * connectorItem, QUndoCommand * parentCommand)
{
	QList<ConnectorItem *> connectorItems;
	QList<BusConnectorItem *> busConnectorItems;
	connectorItems.append(connectorItem);
	BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, false, ViewGeometry::NoFlag);

	if (busConnectorItems.count() >= 2) {
		for (int i = 0; i < busConnectorItems.count() - 1; i++) {
			for (int j = i + 1; j < busConnectorItems.count(); j++) {
				BusConnectorItem * bci_i = busConnectorItems[i];
				BusConnectorItem * bci_j = busConnectorItems[j];
				if (!bci_i->isMergedWith(bci_j)) {
					new MergeBusCommand(this, bci_i->attachedToID(), bci_i->busID(), bci_i->mapToScene(bci_i->pos()),
						bci_j->attachedToID(), bci_j->busID(), bci_j->mapToScene(bci_j->pos()),
						true, parentCommand);
				}
			}
		}
	}
}

void SketchWidget::moveItem(long id, ViewGeometry & viewGeometry) {
	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		if (pitem != NULL) {
			pitem->moveItem(viewGeometry);
		}
	}
}

void SketchWidget::rotateItem(long id, qreal degrees) {
	DebugDialog::debug(QObject::tr("rotating %1 %2").arg(id).arg(degrees) );

	if (!isVisible()) return;

	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		PaletteItem * paletteItem = dynamic_cast<PaletteItem *>(pitem);
		if (paletteItem != NULL) {
			paletteItem->rotateItemAnd(degrees);
		}
	}

}

void SketchWidget::flipItem(long id, Qt::Orientations orientation) {
	DebugDialog::debug(QObject::tr("fliping %1 %2").arg(id).arg(orientation) );

	if (!isVisible()) return;

	 ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		PaletteItem * paletteItem = dynamic_cast<PaletteItem *>(pitem);
		if (paletteItem != NULL) {
			paletteItem->flipItemAnd(orientation);
		}
	}
}


void SketchWidget::changeWire(long fromID, QLineF line, QPointF pos, bool useLine)
{
	ItemBase * fromItem = findItem(fromID);
	if (fromItem == NULL) return;

	Wire* wire = dynamic_cast<Wire *>(fromItem);
	if (wire == NULL) return;

	wire->setLineAnd(line, pos, useLine);
}

void SketchWidget::selectItem(long id, bool state, bool updateInfoView) {
	this->clearHoldingSelectItem();
	ItemBase * item = findItem(id);
	if (item != NULL) {
		item->setSelected(state);
		if(updateInfoView) {
			// show something in the info view, even if it's not selected
			Wire *wire = dynamic_cast<Wire*>(item);
			if(!wire) {
				InfoGraphicsView::viewItemInfo(item);
			} else {
				InfoGraphicsView::viewItemInfo(wire);
			}
		}
		emit itemSelectedSignal(id, state);
	}

	PaletteItem *pitem = dynamic_cast<PaletteItem*>(item);
	if(pitem) m_lastPaletteItemSelected = pitem;
}

void SketchWidget::selectDeselectAllCommand(bool state) {
	this->clearHoldingSelectItem();

	QString stackString = state ? "Select All" : "Deselect";
	m_undoStack->beginMacro(stackString);

	SelectItemCommand* cmd = stackSelectionState(true, NULL);
	cmd->setSelectItemType( state ? SelectItemCommand::SelectAll : SelectItemCommand::DeselectAll );
	selectAllItems(state);

	m_undoStack->endMacro();

	emit allItemsSelectedSignal(state);
}

void SketchWidget::selectAllItems(bool state) {
	QList<QGraphicsItem *> items = this->scene()->items();
	for(int i=0; i < items.size(); i++) {
		QGraphicsItem* pitem = items.at(i);
		if (pitem != NULL) {
			pitem->setSelected(state);
		}
	}
}

void SketchWidget::cut() {
	copy();
	cutDeleteAux("Cut");
}

void SketchWidget::duplicate() {
	copy();
	pasteDuplicateAux(tr("Duplicate"));
}

void SketchWidget::copy() {
	QList<ItemBase *> bases;

	// sort them in z-order so the copies also appear in the same order
	sortSelectedByZ(bases);

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << bases.size();

 	for (int i = 0; i < bases.size(); ++i) {
 		ItemBase *base = bases.at(i);
 		ViewGeometry * viewGeometry = new ViewGeometry(base->getViewGeometry());
		QHash<ItemBase::ViewIdentifier, ViewGeometry * > geometryHash;
		geometryHash.insert(m_viewIdentifier, viewGeometry);
		emit copyItemSignal(base->id(), geometryHash);			// let the other views add their geometry
 		dataStream << base->modelPart()->moduleID() << (qint64) base->id();
		QList <ItemBase::ViewIdentifier> keys = geometryHash.keys();
		dataStream << (qint32) keys.count();
		for (int j = 0; j < keys.count(); j++) {
			ViewGeometry * vg = geometryHash.value(keys[j]);
			dataStream << (qint32) keys[j] << vg->loc() << vg->line() << vg->transform();
		}
		for (int j = 0; j < keys.count(); j++) {
			ViewGeometry * vg = geometryHash[keys[j]];
			delete vg;
		}
	}

	QSet<VirtualWire *> virtualWires;
	QSet<BusConnectorItem *> busConnectors;
	QMultiHash<ConnectorItem *, ConnectorItem *> allConnectorHash;

	// now save the connector info
	foreach (ItemBase * base, bases) {
		QMultiHash<ConnectorItem *, ConnectorItem *> connectorHash;
		base->collectConnectors(connectorHash, scene());
		QMultiHash<ConnectorItem *, ConnectorItem *> disconnectorHash;

		// first get the connectors from each part and remove the ones pointing outside the copied set of parts
		foreach (ConnectorItem * fromConnectorItem, connectorHash.uniqueKeys()) {
			DebugDialog::debug(QString("testing from: %1 %2 %3").arg(fromConnectorItem->attachedToID())
				.arg(fromConnectorItem->attachedToTitle())
				.arg(fromConnectorItem->connectorStuffID()) );
			foreach (ConnectorItem * toConnectorItem, connectorHash.values(fromConnectorItem)) {
				DebugDialog::debug(QString("testing to: %1 %2 %3").arg(toConnectorItem->attachedToID())
					.arg(toConnectorItem->attachedToTitle())
					.arg(toConnectorItem->connectorStuffID()) );
				ItemBase * toBase = toConnectorItem->attachedTo()->layerKinChief();
				if (!bases.contains(toBase)) {
					bool doDelete = true;

					if (toBase->getVirtual()) {
						// deal with inter-connected virtual wires
						VirtualWire * virtualWire = dynamic_cast<VirtualWire *>(toBase);
						ItemBase * itemBase = virtualWire->otherConnector(toConnectorItem)->firstConnectedToIsh()->attachedTo();
						if (bases.contains(itemBase)) {
							virtualWires.insert(virtualWire);
							BusConnectorItem * bci = dynamic_cast<BusConnectorItem *>(fromConnectorItem);
							if (bci != NULL) {
								busConnectors.insert(bci);
							}
							doDelete = false;
						}
					}

					if (doDelete) {
						// don't copy external connection--but don't delete during walkthrough
						disconnectorHash.insert(fromConnectorItem, toConnectorItem);
						DebugDialog::debug("deleting");
					}
				}
			}
		}

		// delete now so that connectorHash isn't modified as we're walking through it  
		foreach (ConnectorItem * fromConnectorItem, disconnectorHash.uniqueKeys()) {
			foreach (ConnectorItem * toConnectorItem, disconnectorHash.values(fromConnectorItem)) {
				connectorHash.remove(fromConnectorItem, toConnectorItem);
			}
		}

		foreach (ConnectorItem * fromConnectorItem, connectorHash.uniqueKeys()) {
			foreach (ConnectorItem * toConnectorItem, connectorHash.values(fromConnectorItem)) {
				allConnectorHash.insert(fromConnectorItem, toConnectorItem);
			}
		}
    }

	// copy virtual wires
	dataStream << virtualWires.count();
	QSet<VirtualWire *>::const_iterator vwit = virtualWires.constBegin();
	while (vwit != virtualWires.constEnd()) {
		VirtualWire * vw = *vwit;

		ViewGeometry * viewGeometry = new ViewGeometry(vw->getViewGeometry());
		dataStream << vw->modelPart()->moduleID() << (qint64) vw->id();
		dataStream << viewGeometry->loc() << viewGeometry->line();
		++vwit;
	}

	// now shove the connection info into the dataStream
	dataStream << (int) allConnectorHash.count();
	DebugDialog::debug(QString("copying %1").arg(allConnectorHash.count()) );
	foreach (ConnectorItem * fromConnectorItem,  allConnectorHash.uniqueKeys()) {
		BusConnectorItem * busConnectorItem = dynamic_cast<BusConnectorItem *>(fromConnectorItem);
		foreach (ConnectorItem * toConnectorItem, allConnectorHash.values(fromConnectorItem)) {
			dataStream << (qint64) fromConnectorItem->attachedTo()->layerKinChief()->id();
			QString thing;
			if (busConnectorItem != NULL) {
				dataStream << busConnectorItem->busID();
				thing = busConnectorItem->busID();
			}
			else {
				dataStream << fromConnectorItem->connectorStuffID();
				thing = fromConnectorItem->connectorStuffID();
			}
			dataStream << (dynamic_cast<BusConnectorItem *>(fromConnectorItem) != NULL);
			dataStream << (qint64) toConnectorItem->attachedTo()->layerKinChief()->id();
			dataStream << toConnectorItem->connectorStuffID();
			dataStream << (dynamic_cast<BusConnectorItem *>(toConnectorItem) != NULL);

			DebugDialog::debug(QString("copying %1 %2 %3 %4").arg(fromConnectorItem->attachedTo()->layerKinChief()->id())
			.arg(thing)
			.arg( toConnectorItem->attachedTo()->layerKinChief()->id())
			.arg(toConnectorItem->connectorStuffID()) );
		}
	}

    QMimeData *mimeData = new QMimeData;
    mimeData->setData("application/x-dnditemsdata", itemData);
	QClipboard *clipboard = QApplication::clipboard();
	if (clipboard == NULL) {
		// shouldn't happen
		delete mimeData;
		return;
	}

	clipboard->setMimeData(mimeData, QClipboard::Clipboard);
}
void SketchWidget::paste() {
	pasteDuplicateAux(tr("Paste"));
}

void SketchWidget::pasteDuplicateAux(QString undoStackMessage) {
	clearHoldingSelectItem();

	QClipboard *clipboard = QApplication::clipboard();
	if (clipboard == NULL) {
		// shouldn't happen
		return;
	}

	const QMimeData* mimeData = clipboard->mimeData(QClipboard::Clipboard);
	if (mimeData == NULL) return;

   	if (!mimeData->hasFormat("application/x-dnditemsdata")) return;

    QByteArray itemData = mimeData->data("application/x-dnditemsdata");
    QDataStream dataStream(&itemData, QIODevice::ReadOnly);
    int size;
    dataStream >> size;
	qint64 dataStreamPos = dataStream.device()->pos();					// remember where we are so we can reset back

	QString messageStr;
	if (size == 1) {
		QString moduleID;
		dataStream >> moduleID;
		bool reset = dataStream.device()->seek(dataStreamPos);			// reset the datastream so the next loop can start reading from there
		if (!reset) {
			// this shouldn't happen
			DebugDialog::debug("reset datastream didn't happen, bailing out");
			return;
		}

		ModelPart * modelPart = m_paletteModel->retrieveModelPart(moduleID);
		messageStr = tr("%1 %2").arg(undoStackMessage).arg(modelPart->title());
	}
	else {
		messageStr = tr("%1 %2 items").arg(undoStackMessage).arg(QString::number(size));
	}

	QUndoCommand * parentCommand = new QUndoCommand(messageStr);
	new CleanUpWiresCommand(this, false, parentCommand);

    stackSelectionState(false, parentCommand);

	QHash<long, long> mapIDs;
    for (int i = 0; i < size; i++) {
		QString moduleID;
		qint64 itemID;
		ModelPart* modelPart = NULL;
		qint32 geometryCount;

		dataStream >> moduleID >> itemID >> geometryCount;
		long newItemID = ItemBase::getNextID();
		mapIDs.insert(itemID, newItemID);
		modelPart = m_paletteModel->retrieveModelPart(moduleID);
		for (int j = 0; j < geometryCount; j++) {
			QTransform transform;
			QPointF loc;
			QLineF line;
    		ViewGeometry viewGeometry;
			qint32 viewIdentifier;

			dataStream >> viewIdentifier >> loc >> line >> transform;
    		viewGeometry.setLoc(loc);
    		viewGeometry.setLine(line);
			viewGeometry.setTransform(transform);
    		viewGeometry.offset(20*m_pasteCount, 20*m_pasteCount);

			SketchWidget * sketchWidget = NULL;
			emit findSketchWidgetSignal((ItemBase::ViewIdentifier) viewIdentifier, sketchWidget);
			if (sketchWidget != NULL) {
				new AddItemCommand(sketchWidget, BaseCommand::SingleView, modelPart->moduleID(), viewGeometry, newItemID, parentCommand, false);
			}
		}
   	}
    m_pasteCount++;				// used for offsetting paste items, not a count of how many items are pasted

	int virtualWireCount;
	dataStream >> virtualWireCount;
	DebugDialog::debug(QString("pasting virtual wires %1").arg(virtualWireCount) );
	for (int i = 0; i < virtualWireCount; i++) {
		QString moduleID;
		qint64 id;
		QPointF loc;
		QLineF line;

		dataStream >> moduleID >> id >> loc >> line;
		long newID = ItemBase::getNextID();
		mapIDs.insert(id, newID);
		ViewGeometry viewGeometry;
		viewGeometry.setLoc(loc);
		viewGeometry.setLine(line);
		viewGeometry.setVirtual(true);
		DebugDialog::debug(tr("pasting %1").arg(newID) );
		new AddItemCommand(this, BaseCommand::CrossView, moduleID, viewGeometry, newID, parentCommand, false);
	}

	// now deal with interconnections between the copied parts
	for (int i = 0; i < size; i++) {
		int connectionCount;
		dataStream >> connectionCount;
		DebugDialog::debug(QString("pasting connections %1").arg(connectionCount) );
		for (int j = 0; j < connectionCount; j++) {
			qint64 fromID;
			QString fromConnectorID;
			bool fromIsBus;
			QString toConnectorID;
			bool toIsBus;
			qint64 toID;
			dataStream >> fromID >> fromConnectorID >> fromIsBus >> toID >> toConnectorID >> toIsBus;
			DebugDialog::debug(tr("pasting %1 %2 %3 %4").arg(fromID).arg(fromConnectorID).arg(toID).arg(toConnectorID) );
			fromID = mapIDs.value(fromID);
			toID = mapIDs.value(toID);
			new ChangeConnectionCommand(this, BaseCommand::CrossView,
										fromID, fromConnectorID,
										toID, toConnectorID,
										true, true, fromIsBus, false, parentCommand);

			//extendChangeConnectionCommand(fromID, fromConnectorID,
			//							  toID, toConnectorID,
			//							  true, true, parentCommand);
		}
	}


	foreach (long oid, mapIDs.keys()) {
		DebugDialog::debug(QString("map from: %1 to: %2").arg(oid).arg(mapIDs.value(oid)) );
	}

	clearTemporaries();

	new CleanUpWiresCommand(this, true, parentCommand);
   	m_undoStack->push(parentCommand);
}


void SketchWidget::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("application/x-dnditemdata") && event->source() != this) {

    	m_droppingWire = false;
        QByteArray itemData = event->mimeData()->data("application/x-dnditemdata");
        QDataStream dataStream(&itemData, QIODevice::ReadOnly);

        QString moduleID;
        QPointF offset;
        dataStream >> moduleID >> offset;

    	ModelPart * modelPart = m_paletteModel->retrieveModelPart(moduleID);
    	if (modelPart !=  NULL) {
			switch (m_viewIdentifier) {
				case ItemBase::PCBView:
				case ItemBase::SchematicView:
					if (modelPart->itemType() == ModelPart::Wire || modelPart->itemType() == ModelPart::Breadboard) {
						// can't drag and drop these parts in these views
						return;
					}
					break;
				default:
					break;
			}

    		m_droppingWire = (modelPart->itemType() == ModelPart::Wire);
			m_droppingOffset = offset;

			if (ItemDrag::_cache().contains(this)) {
				m_droppingItem->setVisible(true);
			}
			else {
    			ViewGeometry viewGeometry;
    			QPointF p = QPointF(this->mapToScene(event->pos())) - offset;
    			viewGeometry.setLoc(p);

				long fromID = ItemBase::getNextID();

				// create temporary item
				// don't need connectors for breadboard
				// could live without them for arduino as well
				// TODO: how to specify which parts don't need connectors during drag and drop from palette?
				m_droppingItem = addItemAux(modelPart, viewGeometry, fromID, NULL, modelPart->itemType() != ModelPart::Breadboard);

				ItemDrag::_cache().insert(this, m_droppingItem);
				m_droppingItem->setCacheMode(QGraphicsItem::ItemCoordinateCache);
				connect(ItemDrag::_itemDrag(), SIGNAL(dragIsDoneSignal(ItemDrag *)), this, SLOT(dragIsDoneSlot(ItemDrag *)));
			}
			//ItemDrag::_setPixmapVisible(false);


		// make sure relevant layer is visible
			ViewLayer::ViewLayerID viewLayerID;
			if (m_droppingWire) {
				viewLayerID = getWireViewLayerID(m_droppingItem->getViewGeometry());
			}
			else if(modelPart->tags().contains("ruler",Qt::CaseInsensitive)) {
				viewLayerID = getRulerViewLayerID();
			}
			else {
				viewLayerID = getPartViewLayerID();
			}

			ViewLayer * viewLayer = m_viewLayers.value(viewLayerID);
			if (viewLayer != NULL && !viewLayer->visible()) {
				setLayerVisible(viewLayer, true);
			}
		}

        event->acceptProposedAction();
    } else {
		QGraphicsView::dragEnterEvent(event);
    }
}

void SketchWidget::dragLeaveEvent(QDragLeaveEvent * event) {
	if (m_droppingItem != NULL) {
		m_droppingItem->setVisible(false);
		//ItemDrag::_setPixmapVisible(true);
	}

	QGraphicsView::dragLeaveEvent(event);
}

void SketchWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-dnditemdata") && event->source() != this) {



    	dragMoveHighlightConnector(event);

        event->acceptProposedAction();
        return;
    }

	QGraphicsView::dragMoveEvent(event);
}

void SketchWidget::dragMoveHighlightConnector(QDragMoveEvent *event) {

	if (m_droppingItem == NULL) return;

	QPointF loc = this->mapToScene(event->pos()) - m_droppingOffset;

	m_droppingItem->setItemPos(loc);

	m_droppingItem->findConnectorsUnder();
}


void SketchWidget::dropEvent(QDropEvent *event)
{
	clearHoldingSelectItem();

	if (m_droppingItem == NULL) return;

    if (event->mimeData()->hasFormat("application/x-dnditemdata") && event->source() != this) {

    	ModelPart * modelPart = m_droppingItem->modelPart();
    	if (modelPart == NULL) return;
    	if (modelPart->modelPartStuff() == NULL) return;

		QUndoCommand* parentCommand = new QUndoCommand(tr("Add %1").arg(modelPart->title()));
		new CleanUpWiresCommand(this, false, parentCommand);
    	stackSelectionState(false, parentCommand);

		m_droppingItem->saveGeometry();
    	ViewGeometry viewGeometry = m_droppingItem->getViewGeometry();

		long fromID = m_droppingItem->id();

		BaseCommand::CrossViewType crossViewType = BaseCommand::CrossView;
		if (modelPart->properties().values().contains(QString("ruler"))) { // TODO Mariano: add case-insensitiveness
			// rulers are local to a particular view
			crossViewType = BaseCommand::SingleView;
		}
		new AddItemCommand(this, crossViewType, modelPart->moduleID(), viewGeometry, fromID, parentCommand);

		bool gotConnector = false;
		foreach (QGraphicsItem * childItem, m_droppingItem->childItems()) {
			ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(childItem);
			if (connectorItem == NULL) continue;

			ConnectorItem * to = connectorItem->overConnectorItem();
			if (to != NULL) {
				to->connectorHover(to->attachedTo(), false);
				connectorItem->setOverConnectorItem(NULL);   // clean up
				extendChangeConnectionCommand(connectorItem, to, true, false, parentCommand);
				gotConnector = true;
			}
		}

		clearTemporaries();

		killDroppingItem();

		new CleanUpWiresCommand(this, true, parentCommand);
        m_undoStack->waitPush(parentCommand, 10);

        event->acceptProposedAction();
		DebugDialog::debug("after drop event");
    }
	else {
		QGraphicsView::dropEvent(event);
	}

}

void SketchWidget::viewItemInfo(long id) {
	ItemBase *item = findItem(id);
	if (item == NULL) return;

	m_lastPaletteItemSelected = dynamic_cast<PaletteItem*>(item);
	InfoGraphicsView::viewItemInfo(item);
}

SelectItemCommand* SketchWidget::stackSelectionState(bool pushIt, QUndoCommand * parentCommand) {

	// if pushIt assumes m_undoStack->beginMacro has previously been called

	SelectItemCommand* selectItemCommand = new SelectItemCommand(this, SelectItemCommand::NormalSelect, parentCommand);
	const QList<QGraphicsItem *> sitems = scene()->selectedItems();
 	for (int i = 0; i < sitems.size(); ++i) {
 		ItemBase * base = ItemBase::extractTopLevelItemBase(sitems.at(i));
 		if (base == NULL) continue;

 		 selectItemCommand->addUndo(base->id());
    }

    if (pushIt) {
     	m_undoStack->push(selectItemCommand);
    }

    return selectItemCommand;
}

void SketchWidget::mousePressEvent(QMouseEvent *event) {

	//setRenderHint(QPainter::Antialiasing, false);

	if (m_chainDragging) {
		// set a timer, if it expires, then create another link in the chain
	}

	clearHoldingSelectItem();
	m_savedItems.clear();
	m_needToConnectItems.clear();
	m_moveEventCount = 0;
	m_holdingSelectItemCommand = stackSelectionState(false, NULL);

	QGraphicsItem* item = this->itemAt(event->pos());

	QGraphicsView::mousePressEvent(event);
	if (item == NULL) {
		return;
	}

	// to do: combine this with stack selection state so it doesn't have to happen twice
	QList<QGraphicsItem *> items = this->scene()->selectedItems ();
	for (int i = 0; i < items.size(); i++) {
		ItemBase *item = ItemBase::extractTopLevelItemBase(items[i]);
		if (item == NULL) continue;

		item->saveGeometry();
		m_savedItems.append(item);
	}

}

void SketchWidget::mouseMoveEvent(QMouseEvent *event) {
	m_moveEventCount++;
	QGraphicsView::mouseMoveEvent(event);
}

void SketchWidget::mouseReleaseEvent(QMouseEvent *event) {
	//setRenderHint(QPainter::Antialiasing, true);

	QGraphicsView::mouseReleaseEvent(event);
	//setDragMode(QGraphicsView::RubberBandDrag);

	if (m_connectorDragWire != NULL) {
		m_connectorDragWire->ungrabMouse();

		// remove again (may not have been removed earlier)
		if (m_connectorDragWire->scene() != NULL) {
			this->scene()->removeItem(m_connectorDragWire);
			// TODO Mariano: perhaps we could ask for the attachedTo
			m_infoView->unregisterCurrentItem();

		}

		DebugDialog::debug("deleting connector drag wire");
		delete m_connectorDragWire;
		m_connectorDragWire = NULL;
		m_savedItems.clear();
		return;
	}

	if (m_savedItems.size() <= 0) return;

	checkMoved();
	m_savedItems.clear();
}

void SketchWidget::checkMoved()
{
	if (m_moveEventCount == 0) {
		return;
	}

	ItemBase * saveBase = m_savedItems[0];
	int moveCount = m_savedItems.count();
	if (moveCount <= 0) {
		return;
	}

	clearHoldingSelectItem();

	QString moveString;
	QString viewName = ItemBase::viewIdentifierName(m_viewIdentifier);

	if (moveCount == 1) {
		moveString = tr("Move %2 (%1)").arg(viewName).arg(saveBase->modelPart()->title());
	}
	else {
		moveString = tr("Move %2 items (%1)").arg(viewName).arg(QString::number(moveCount));
	}

	QSet <VirtualWire *> virtualWires;
	QList<BusConnectorItem *> busConnectorItems;
	collectBusConnectorItems(busConnectorItems);

	QUndoCommand * parentCommand = new QUndoCommand(moveString);
	new CleanUpWiresCommand(this, false, parentCommand);
	for (int i = 0; i < m_savedItems.size(); i++) {
		// already know it's a topLevel item
		ItemBase *item = m_savedItems[i];
		if (item == NULL) continue;

		ViewGeometry viewGeometry(item->getViewGeometry());
		item->saveGeometry();

		new MoveItemCommand(this, item->id(), viewGeometry, item->getViewGeometry(), parentCommand);

		if (item->itemType() == ModelPart::Breadboard) continue;

		checkSticky(item, parentCommand);

		if (item->itemType() == ModelPart::Wire) continue;

		disconnectFromFemale(item, m_savedItems, virtualWires, parentCommand);
	}

	QList<ConnectorItem *> keys = m_needToConnectItems.keys();
	for (int i = 0; i < keys.count(); i++) {
		ConnectorItem * from = keys[i];
		if (from == NULL) continue;

		ConnectorItem * to = m_needToConnectItems[from];
		if (to == NULL) continue;

		extendChangeConnectionCommand(from, to, true, false, parentCommand);
	}

	cleanUpVirtualWires(virtualWires, busConnectorItems, parentCommand);
	clearTemporaries();

	m_needToConnectItems.clear();
	new CleanUpWiresCommand(this, true, parentCommand);
	m_undoStack->push(parentCommand);
}

void SketchWidget::relativeZoom(qreal step) {
	qreal tempSize = m_scaleValue + step;
	if (tempSize < m_minScaleValue) {
		m_scaleValue = m_minScaleValue;
		emit zoomOutOfRange(m_scaleValue);
		return;
	}
	if (tempSize > m_maxScaleValue) {
		m_scaleValue = m_maxScaleValue;
		emit zoomOutOfRange(m_scaleValue);
		return;
	}
	qreal tempScaleValue = tempSize/100;

	m_scaleValue = tempSize;

	QMatrix matrix;
	//matrix.translate((width() / 2) - mousePosition.x(), (height() / 2) -  mousePosition.y());
	//qreal scale = qPow(qreal(2), (m_scaleValue-50) / qreal(50));
	matrix.scale(tempScaleValue, tempScaleValue);
	//matrix.translate(mousePosition.x() - (width() / 2), mousePosition.y() - (height() / 2));
	this->setMatrix(matrix);

	emit zoomChanged(m_scaleValue);
}

void SketchWidget::absoluteZoom(qreal percent) {
	relativeZoom(percent-m_scaleValue);
}

qreal SketchWidget::currentZoom() {
	return m_scaleValue;
}

void SketchWidget::wheelEvent(QWheelEvent* event) {
	QPointF mousePosition = event->pos();
	qreal delta = ((qreal)event->delta() / 120) * ZoomComboBox::ZoomStep;
	if (delta == 0) return;

	// Scroll zooming throw the combobox options
	/*if(delta < 0) {
		emit zoomOut(-1*delta);
	} else {
		emit zoomIn(delta);
	}*/

	// Scroll zooming relative to the current size
	relativeZoom(delta);

	//this->verticalScrollBar()->setValue(verticalScrollBar()->value() + 3);
	//this->horizontalScrollBar()->setValue(horizontalScrollBar()->value() + 3);


	//to do: center zoom around mouse location



	//QPointF pos = event->pos();
	//QPointF spos = this->mapToScene((int) pos.x(), (int) pos.y());


	//DebugDialog::debug(QObject::tr("translate %1 %2").arg(spos.x()).arg(spos.y()) );

}


void SketchWidget::setPaletteModel(PaletteModel * paletteModel) {
	m_paletteModel = paletteModel;
}

void SketchWidget::setRefModel(ReferenceModel *refModel) {
	m_refModel = refModel;
}

void SketchWidget::setSketchModel(SketchModel * sketchModel) {
	m_sketchModel = sketchModel;
}

void SketchWidget::sketchWidget_itemAdded(ModelPart * modelPart, const ViewGeometry & viewGeometry, long id) {
	addItemAux(modelPart, viewGeometry, id, NULL, true);
}

void SketchWidget::sketchWidget_itemDeleted(long id) {
	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		DebugDialog::debug(QString("really deleting %1 %2").arg(id).arg(m_viewIdentifier) );
		pitem->removeLayerKin();
		this->scene()->removeItem(pitem);
		delete pitem;
	}
}

void SketchWidget::scene_selectionChanged() {
	if (m_ignoreSelectionChangeEvents) {
		return;
	}

	// TODO: this can be dangerous if an item is on m_lastSelected and the item is deleted without being deselected first.

	// hack to make up for missing selection state updates
	foreach (QGraphicsItem * item, m_lastSelected) {
		item->update();
	}

	m_lastSelected = scene()->selectedItems();
	emit selectionChangedSignal();

	// hack to make up for missing selection state updates
	foreach (QGraphicsItem * item, m_lastSelected) {
		item->update();
	}

	if (m_holdingSelectItemCommand != NULL) {
		int selCount = 0;
		ItemBase* saveBase = NULL;
		QString selString;
		const QList<QGraphicsItem *> sitems = scene()->selectedItems();
	 	for (int i = 0; i < sitems.size(); ++i) {
	 		ItemBase * base = ItemBase::extractItemBase(sitems.at(i));
	 		if (base == NULL) continue;

	 		saveBase = base;
	 		m_holdingSelectItemCommand->addRedo(base->id());
	 		selCount++;
	    }

	    SelectItemCommand* temp = m_holdingSelectItemCommand;
	    m_holdingSelectItemCommand = NULL;
		if (selCount == 1) {
			selString = tr("Select %1").arg(saveBase->modelPart()->title());
		}
		else {
			selString = tr("Select %1 items").arg(QString::number(selCount));
		}

		temp->setText(selString);


	    //DebugDialog::debug("scene changed push select");
	    m_undoStack->push(temp);
	}
}

void SketchWidget::clearHoldingSelectItem() {
	if (m_holdingSelectItemCommand != NULL) {
		//DebugDialog::debug("clear holding");
		delete m_holdingSelectItemCommand;
		m_holdingSelectItemCommand = NULL;
	}
}

void SketchWidget::clearSelection() {
	this->scene()->clearSelection();
	emit clearSelectionSignal();
}

void SketchWidget::sketchWidget_clearSelection() {
	this->scene()->clearSelection();
}

void SketchWidget::sketchWidget_itemSelected(long id, bool state) {
	ItemBase * item = findItem(id);
	if (item != NULL) {
		item->setSelected(state);
	}

	PaletteItem *pitem = dynamic_cast<PaletteItem*>(item);
	if(pitem) m_lastPaletteItemSelected = pitem;
}

void SketchWidget::sketchWidget_tooltipAppliedToItem(long id, const QString& text) {
	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		pitem->setInstanceTitleAndTooltip(text);
	}
}

void SketchWidget::sketchWidget_allItemsSelected(bool state) {
	selectAllItems(state);
}

void SketchWidget::group() {
	const QList<QGraphicsItem *> sitems = scene()->selectedItems();
	if (sitems.size() < 2) return;
}

void SketchWidget::wire_wireChanged(Wire* wire, QLineF oldLine, QLineF newLine, QPointF oldPos, QPointF newPos, ConnectorItem * from, ConnectorItem * to) {
	this->clearHoldingSelectItem();
	this->m_moveEventCount = 0;  // clear this so an extra MoveItemCommand isn't posted

	// TODO: make sure all these pointers to pointers to pointers aren't null...


	if (wire == this->m_connectorDragWire) {
		dragWireChanged(wire, from, to);
		return;
	}

	QUndoCommand * parentCommand = new QUndoCommand();
	new CleanUpWiresCommand(this, false, parentCommand);

	long fromID = wire->id();

	QString fromConnectorID;
	if (from != NULL) {
		fromConnectorID = from->connectorStuffID();
	}

	long toID = -1;
	QString toConnectorID;
	if (to != NULL) {
		toID = to->attachedToID();
		toConnectorID = to->connectorStuffID();
	}

	new ChangeWireCommand(this, fromID, oldLine, newLine, oldPos, newPos, true);

	checkSticky(wire, parentCommand);

	QList<ConnectorItem *> former = from->connectedToItems();

	QString prefix;
	QString suffix;
	if (to == NULL) {
		if (former.count() > 0  && !from->chained()) {
			prefix = tr("Disconnect");
			// the suffix is a little tricky to determine
			// it might be multiple disconnects, or might be disconnecting a virtual wire, in which case, the
			// title needs to come from the virtual wire's other connection's attachedTo()

			// suffix = tr("from %1").arg(former->attachedToTitle());
		}
		else {
			prefix = tr("Change");
		}
	}
	else {
		prefix = tr("Connect");
		suffix = tr("to %1").arg(to->attachedTo()->modelPart()->title());
	}

	parentCommand->setText(QObject::tr("%1 %2 %3").arg(prefix).arg(wire->modelPart()->title()).arg(suffix) );

	if (from->chained()) {
		foreach (ConnectorItem * toConnectorItem, from->connectedToItems()) {
			Wire * toWire = dynamic_cast<Wire *>(toConnectorItem->attachedTo());
			if (toWire == NULL) continue;

			ViewGeometry vg = toWire->getViewGeometry();
			toWire->saveGeometry();
			ViewGeometry vg2 = toWire->getViewGeometry();
			new ChangeWireCommand(this, toWire->id(), vg.line(), vg2.line(), vg.loc(), vg2.loc(), true, parentCommand);
		}
	}
	else {
		if (former.count() > 0) {
			QList<ConnectorItem *> connectorItems;
			QList<BusConnectorItem *> busConnectorItems;
			connectorItems.append(from);
			BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, false, ViewGeometry::NoFlag);
			QSet<VirtualWire *> virtualWires;

			foreach (ConnectorItem * formerConnectorItem, former) {
				if (formerConnectorItem->attachedTo()->getVirtual()) {
					virtualWires.insert(dynamic_cast<VirtualWire *>(formerConnectorItem->attachedTo()));
				}
				else {
					// temp remove here, virtual wires handle temp removes
					extendChangeConnectionCommand(from, formerConnectorItem, false, false, parentCommand);
					from->tempRemove(formerConnectorItem);
					formerConnectorItem->tempRemove(from);
				}

			}

			cleanUpVirtualWires(virtualWires, busConnectorItems, parentCommand);
		}
		if (to != NULL) {
			extendChangeConnectionCommand(from, to, true, false, parentCommand);
		}
	}


	clearTemporaries();

	new CleanUpWiresCommand(this, true, parentCommand);
	m_undoStack->push(parentCommand);
}

void SketchWidget::dragWireChanged(Wire* wire, ConnectorItem * from, ConnectorItem * to)
{
	QUndoCommand * parentCommand = new QUndoCommand();
	new CleanUpWiresCommand(this, false, parentCommand);

	m_connectorDragWire->saveGeometry();
	long fromID = wire->id();

	DebugDialog::debug(tr("m_connectorDragConnector:%1 %4 from:%2 to:%3")
					   .arg(m_connectorDragConnector->connectorStuffID())
					   .arg(from->connectorStuffID())
					   .arg((to == NULL) ? "null" : to->connectorStuffID())
					   .arg(m_connectorDragConnector->attachedTo()->modelPart()->title()) );

	parentCommand->setText(tr("Create and connect wire"));

	// create a new wire with the same id as the temporary wire
	new AddItemCommand(this, BaseCommand::CrossView, m_connectorDragWire->modelPart()->moduleID(), m_connectorDragWire->getViewGeometry(), fromID, parentCommand);

	ConnectorItem * anchor = wire->otherConnector(from);
	if (anchor != NULL) {
		extendChangeConnectionCommand(anchor, m_connectorDragConnector, true, false, parentCommand);
	}
	if (to != NULL) {
		// since both wire connections are being newly created, set up the anchor connection temporarily
		// the other connection is created temporarily in extendChangeConnectionCommand
		m_connectorDragConnector->tempConnectTo(anchor);
		anchor->tempConnectTo(m_connectorDragConnector);
		extendChangeConnectionCommand(from, to, true, false, parentCommand);
		m_connectorDragConnector->tempRemove(anchor);
		anchor->tempRemove(m_connectorDragConnector);

	}

	clearTemporaries();

	// remove the temporary one
	this->scene()->removeItem(m_connectorDragWire);

	new CleanUpWiresCommand(this, true, parentCommand);
	m_undoStack->push(parentCommand);

}

void SketchWidget::addViewLayer(ViewLayer * viewLayer) {
	m_viewLayers.insert(viewLayer->viewLayerID(), viewLayer);
	BetterTriggerViewLayerAction* action = new BetterTriggerViewLayerAction(QObject::tr("%1 Layer").arg(viewLayer->displayName()), viewLayer, this);
	action->setCheckable(true);
	action->setChecked(viewLayer->visible());
    connect(action, SIGNAL(betterTriggered(QAction*)), this, SLOT(toggleLayerVisibility(QAction*)));
    viewLayer->setAction(action);
}

void SketchWidget::updateLayerMenu(QMenu * layerMenu, QAction * showAllAction, QAction * hideAllAction) {
	QList<ViewLayer::ViewLayerID>keys = m_viewLayers.keys();

	// make sure they're in ascending order when inserting into the menu
	qSort(keys.begin(), keys.end());

	for (int i = 0; i < keys.count(); i++) {
		ViewLayer * viewLayer = m_viewLayers.value(keys[i]);
    	if (viewLayer != NULL) {
			layerMenu->addAction(viewLayer->action());
		}
	}
	// TODO: Fix this function, to update the show/hide all actions
	updateAllLayersActions(showAllAction, hideAllAction);
}

void SketchWidget::setAllLayersVisible(bool visible) {
	QList<ViewLayer::ViewLayerID>keys = m_viewLayers.keys();

	for (int i = 0; i < keys.count(); i++) {
		ViewLayer * viewLayer = m_viewLayers.value(keys[i]);
		if (viewLayer != NULL) {
			setLayerVisible(viewLayer, visible);
		}
	}
}

void SketchWidget::updateAllLayersActions(QAction * showAllAction, QAction * hideAllAction) {
	QList<ViewLayer::ViewLayerID>keys = m_viewLayers.keys();

	if(keys.count()>0) {
		ViewLayer *prev = m_viewLayers.value(keys[0]);
		if (prev == NULL) {
			// jrc: I think prev == NULL is actually a side effect from an earlier bug
			// but I haven't figured out the cause yet
			// at any rate, when this bug occurs, keys[0] is some big negative number that looks like an
			// uninitialized or scrambled layerID
			DebugDialog::debug(QString("updateAllLayersActions keys[0] failed %1").arg(keys[0]) );
			showAllAction->setEnabled(false);
			hideAllAction->setEnabled(false);
			return;
		}
		bool sameState = prev->action()->isChecked();
		bool checked = prev->action()->isChecked();
		//DebugDialog::debug(tr("Layer: %1 is %2").arg(prev->action()->text()).arg(prev->action()->isChecked()));
		for (int i = 1; i < keys.count(); i++) {
			ViewLayer *viewLayer = m_viewLayers.value(keys[i]);
			//DebugDialog::debug(tr("Layer: %1 is %2").arg(viewLayer->action()->text()).arg(viewLayer->action()->isChecked()));
			if (viewLayer != NULL) {
				if(prev != NULL && prev->action()->isChecked() != viewLayer->action()->isChecked() ) {
					// if the actions aren't all checked or unchecked I don't bother about the "checked" variable
					sameState = false;
					break;
				} else {
					sameState = true;
					checked = viewLayer->action()->isChecked();
				}
				prev = viewLayer;
			}
		}

		//DebugDialog::debug(tr("sameState: %1").arg(sameState));
		//DebugDialog::debug(tr("checked: %1").arg(checked));
		if(sameState) {
			if(checked) {
				showAllAction->setEnabled(false);
				hideAllAction->setEnabled(true);
			} else {
				showAllAction->setEnabled(true);
				hideAllAction->setEnabled(false);
			}
		} else {
			showAllAction->setEnabled(true);
			hideAllAction->setEnabled(true);
		}
	} else {
		showAllAction->setEnabled(false);
		hideAllAction->setEnabled(false);
	}
}

ItemCount SketchWidget::calcItemCount() {
	ItemCount itemCount;

	// TODO: replace scene()->items()
	QList<QGraphicsItem *> items = scene()->items();
	QList<QGraphicsItem *> selItems = scene()->selectedItems();

	itemCount.selCount = 0;
	itemCount.selRotatable = 0;
	itemCount.itemsCount = 0;

	for (int i = 0; i < selItems.count(); i++) {
		if (ItemBase::extractTopLevelItemBase(selItems[i]) != NULL) {
			itemCount.selCount++;
			// can't rotate a wire or a layerKin
			if (dynamic_cast<PaletteItemBase *>(selItems[i]) != NULL) {
				itemCount.selRotatable++;
			}
		}
	}
	if (itemCount.selCount > 0) {
		for (int i = 0; i < items.count(); i++) {
			if (ItemBase::extractTopLevelItemBase(items[i]) != NULL) {
				itemCount.itemsCount++;
			}
		}
	}

	return itemCount;
}

bool SketchWidget::layerIsVisible(ViewLayer::ViewLayerID viewLayerID) {
	ViewLayer * viewLayer = m_viewLayers[viewLayerID];
	if (viewLayer == NULL) return false;

	return viewLayer->visible();
}

void SketchWidget::setLayerVisible(ViewLayer::ViewLayerID viewLayerID, bool vis) {
	ViewLayer * viewLayer = m_viewLayers[viewLayerID];
	if (viewLayer) {
		setLayerVisible(viewLayer, vis);
	}
}

void SketchWidget::toggleLayerVisibility(QAction * action) {
	BetterTriggerViewLayerAction * btvla = dynamic_cast<BetterTriggerViewLayerAction *>(action);
	if (btvla == NULL) return;

	ViewLayer * viewLayer = btvla->viewLayer();
	if (viewLayer == NULL) return;

	setLayerVisible(viewLayer, !viewLayer->visible());
}

void SketchWidget::setLayerVisible(ViewLayer * viewLayer, bool visible) {
	viewLayer->setVisible(visible);

	// TODO: replace scene()->items()
	const QList<QGraphicsItem *> items = scene()->items();
	for (int i = 0; i < items.size(); i++) {
		// want all items, not just topLevel
		ItemBase * itemBase = ItemBase::extractItemBase(items[i]);
		if (itemBase == NULL) continue;

		if (itemBase->viewLayerID() == viewLayer->viewLayerID()) {
			itemBase->setHidden(!visible);
			DebugDialog::debug(QObject::tr("setting visible %1").arg(viewLayer->visible()) );
		}
	}
}

void SketchWidget::sendToBack() {
	QList<ItemBase *> bases;
	if (!startZChange(bases)) return;

	QString text = QObject::tr("Bring forward");
	continueZChangeMax(bases, bases.size() - 1, -1, greaterThan, -1, text);
}

void SketchWidget::sendBackward() {

	QList<ItemBase *> bases;
	if (!startZChange(bases)) return;

	QString text = QObject::tr("Send backward");
	continueZChange(bases, 0, bases.size(), lessThan, 1, text);
}

void SketchWidget::bringForward() {
	QList<ItemBase *> bases;
	if (!startZChange(bases)) return;

	QString text = QObject::tr("Bring forward");
	continueZChange(bases, bases.size() - 1, -1, greaterThan, -1, text);

}

void SketchWidget::bringToFront() {
	QList<ItemBase *> bases;
	if (!startZChange(bases)) return;

	QString text = QObject::tr("Bring to front");
	continueZChangeMax(bases, 0, bases.size(), lessThan, 1, text);

}

void SketchWidget::fitInWindow() {
	QRectF itemsRect = this->scene()->itemsBoundingRect();
	QRectF viewRect = rect();

	//fitInView(itemsRect.x(), itemsRect.y(), itemsRect.width(), itemsRect.height(), Qt::KeepAspectRatio);

	qreal wRelation = (viewRect.width()-this->verticalScrollBar()->width())  / itemsRect.width();
	qreal hRelation = (viewRect.height()-this->horizontalScrollBar()->height()) / itemsRect.height();

	DebugDialog::debug(tr("scen rect: w%1 h%2").arg(itemsRect.width()).arg(itemsRect.height()));
	DebugDialog::debug(tr("view rect: w%1 h%2").arg(viewRect.width()).arg(viewRect.height()));
	DebugDialog::debug(tr("relations (v/s): w%1 h%2").arg(wRelation).arg(hRelation));

	if(wRelation < hRelation) {
		m_scaleValue = (wRelation * 100);
	} else {
		m_scaleValue = (hRelation * 100);
	}

	// Actually the zoom hasn't changed...
	emit zoomChanged(--m_scaleValue);

	this->centerOn(itemsRect.center());
}

bool SketchWidget::startZChange(QList<ItemBase *> & bases) {
	int selCount = scene()->selectedItems().count();
	if (selCount <= 0) return false;

	const QList<QGraphicsItem *> items = scene()->items();
	if (items.count() <= selCount) return false;

	sortAnyByZ(items, bases);

	return true;
}

void SketchWidget::continueZChange(QList<ItemBase *> & bases, int start, int end, bool (*test)(int current, int start), int inc, const QString & text) {

	bool moved = false;
	int last = bases.size();
	for (int i = start; test(i, end); i += inc) {
		ItemBase * base = bases[i];

		if (!base->getViewGeometry().selected()) continue;

		int j = i - inc;
		if (j >= 0 && j < last && bases[j]->viewLayerID() == base->viewLayerID()) {
			bases.move(i, j);
			moved = true;
		}
	}

	if (!moved) {
		return;
	}

	continueZChangeAux(bases, text);
}

void SketchWidget::continueZChangeMax(QList<ItemBase *> & bases, int start, int end, bool (*test)(int current, int start), int inc, const QString & text) {

	QHash<ItemBase *, ItemBase *> marked;
	bool moved = false;
	int last = bases.size();
	for (int i = start; test(i, end); i += inc) {
		ItemBase * base = bases[i];
		if (!base->getViewGeometry().selected()) continue;
		if (marked[base] != NULL) continue;

		marked.insert(base, base);

		int dest = -1;
		for (int j = i + inc; j >= 0 && j < last && bases[j]->viewLayerID() == base->viewLayerID(); j += inc) {
			dest = j;
		}

		if (dest >= 0) {
			moved = true;
			bases.move(i, dest);
			DebugDialog::debug(QObject::tr("moving %1 to %2").arg(i).arg(dest));
			i -= inc;	// because we just modified the list and would miss the next item
		}
	}

	if (!moved) {
		return;
	}

	continueZChangeAux(bases, text);
}


void SketchWidget::continueZChangeAux(QList<ItemBase *> & bases, const QString & text) {

	ChangeZCommand * changeZCommand = new ChangeZCommand(this );

	ViewLayer::ViewLayerID lastViewLayerID = ViewLayer::UnknownLayer;
	qreal z = 0;
	for (int i = 0; i < bases.size(); i++) {
		qreal oldZ = bases[i]->getViewGeometry().z();
		if (bases[i]->viewLayerID() != lastViewLayerID) {
			lastViewLayerID = bases[i]->viewLayerID();
			z = floor(oldZ);
		}
		else {
			z += ViewLayer::getZIncrement();
		}


		if (oldZ == z) continue;

		// optimize this by only adding z's that must change
		// rather than changing all of them
		changeZCommand->addTriplet(bases[i]->id(), oldZ, z);
	}

	changeZCommand->setText(text);
	m_undoStack->push(changeZCommand);
}

void SketchWidget::sortSelectedByZ(QList<ItemBase *> & bases) {

	const QList<QGraphicsItem *> items = scene()->selectedItems();
	if (items.size() <= 0) return;

	QList<QGraphicsItem *> tlBases;
	for (int i = 0; i < items.count(); i++) {
		ItemBase * itemBase =  ItemBase::extractTopLevelItemBase(items[i]);
		if (itemBase != NULL) {
			tlBases.append(itemBase);
		}
	}

	sortAnyByZ(tlBases, bases);
}


void SketchWidget::sortAnyByZ(const QList<QGraphicsItem *> & items, QList<ItemBase *> & bases) {
	for (int i = 0; i < items.size(); i++) {
		ItemBase * base = ItemBase::extractItemBase(items[i]);
		if (base != NULL) {
			bases.append(base);
			base->saveGeometry();
		}
	}

    // order by z
    qSort(bases.begin(), bases.end(), ItemBase::zLessThan);
}

bool SketchWidget::lessThan(int a, int b) {
	return a < b;
}

bool SketchWidget::greaterThan(int a, int b) {
	return a > b;
}

void SketchWidget::changeZ(QHash<long, RealPair * > triplets, qreal (*pairAccessor)(RealPair *) ) {

	// TODO: replace scene->items
	const QList<QGraphicsItem *> items = scene()->items();
	for (int i = 0; i < items.size(); i++) {
		// want all items, not just topLevel
		ItemBase * itemBase = ItemBase::extractItemBase(items[i]);
		if (itemBase == NULL) continue;

		RealPair * pair = triplets[itemBase->id()];
		if (pair == NULL) continue;

		qreal newZ = pairAccessor(pair);
		DebugDialog::debug(QObject::tr("change z %1 %2").arg(itemBase->id()).arg(newZ));
		items[i]->setZValue(newZ);

	}
}

ViewLayer::ViewLayerID SketchWidget::getWireViewLayerID(const ViewGeometry & viewGeometry) {
	if (viewGeometry.getJumper()) {

		return ViewLayer::Jumperwires;
	}

	if (viewGeometry.getTrace()) {
		return ViewLayer::Copper0;
	}

	return m_wireViewLayerID;
}

ViewLayer::ViewLayerID SketchWidget::getRulerViewLayerID() {
	return m_rulerViewLayerID;
}

ViewLayer::ViewLayerID SketchWidget::getPartViewLayerID() {
	return m_partViewLayerID;
}

ViewLayer::ViewLayerID SketchWidget::getConnectorViewLayerID() {
	return m_connectorViewLayerID;
}


void SketchWidget::mousePressConnectorEvent(ConnectorItem * connectorItem, QGraphicsSceneMouseEvent * event) {
	if (m_chainDragging) {

	}

	ModelPart * wireModel = m_paletteModel->retrieveModelPart(Wire::moduleIDName);
	if (wireModel == NULL) return;

	// make sure wire layer is visible
	ViewLayer::ViewLayerID viewLayerID = getWireViewLayerID(connectorItem->attachedTo()->getViewGeometry());
	ViewLayer * viewLayer = m_viewLayers.value(viewLayerID);
	if (viewLayer != NULL && !viewLayer->visible()) {
		setLayerVisible(viewLayer, true);
	}

   	ViewGeometry viewGeometry;
   	QPointF p = QPointF(connectorItem->mapToScene(event->pos()));
   	viewGeometry.setLoc(p);
	viewGeometry.setLine(QLineF(0,0,0,0));

	// create a temporary wire for the user to drag
	m_connectorDragConnector = connectorItem;
	m_connectorDragWire = dynamic_cast<Wire *>(addItemAux(wireModel, viewGeometry, ItemBase::getNextID(), NULL, true));
	DebugDialog::debug("creating connector drag wire");
	if (m_connectorDragWire == NULL) return;

	// give connector item the mouse, so wire doesn't get mouse moved events
	m_connectorDragWire->grabMouse();
	m_connectorDragWire->initDragEnd(m_connectorDragWire->connector0());
	//m_chainDragging = true;
}


void SketchWidget::rotateX(qreal degrees) {
	rotateFlip(degrees, 0);
}


void SketchWidget::flip(Qt::Orientations orientation) {
	rotateFlip(0, orientation);
}

void SketchWidget::rotateFlip(qreal degrees, Qt::Orientations orientation)
{
	if (!this->isVisible()) return;

	clearHoldingSelectItem();

	QList <QGraphicsItem *> items = scene()->selectedItems();
	QList <PaletteItem *> targets;

	for (int i = 0; i < items.size(); i++) {
		// can't rotate wires or layerkin (layerkin rotated indirectly)
		PaletteItem *item = dynamic_cast<PaletteItem *>(items[i]);
		if (item == NULL) continue;

		targets.append(item);
	}

	if (targets.count() <= 0) {
		return;
	}


	QSet <VirtualWire *> virtualWires;
	QList<BusConnectorItem *> busConnectorItems;
	collectBusConnectorItems(busConnectorItems);

	QString string = tr("%3 %2 (%1)")
			.arg(ItemBase::viewIdentifierName(m_viewIdentifier))
			.arg((targets.count() == 1) ? targets[0]->modelPart()->title() : QString::number(targets.count()) + " items" )
			.arg((degrees != 0) ? tr("Rotate") : tr("Flip"));

	QUndoCommand * parentCommand = new QUndoCommand(string);
	new CleanUpWiresCommand(this, false, parentCommand);

	QList<ItemBase *> emptyList;			// used for a move command
	foreach (PaletteItem * item, targets) {
		disconnectFromFemale(item, emptyList, virtualWires, parentCommand);

		if (item->sticky()) {
			//TODO: apply transformation to stuck items
		}
		// TODO: if item has female connectors, then apply transform to connected items

		if (degrees != 0) {
			new RotateItemCommand(this, item->id(), degrees, parentCommand);
		}
		else {
			new FlipItemCommand(this, item->id(), orientation, parentCommand);
		}

	}

	cleanUpVirtualWires(virtualWires, busConnectorItems, parentCommand);
	clearTemporaries();

	new CleanUpWiresCommand(this, true, parentCommand);
	m_undoStack->push(parentCommand);

}

ConnectorItem * SketchWidget::findConnectorItem(ItemBase * itemBase, const QString & connectorID, bool seekLayerKin) {

	ConnectorItem * connectorItem = itemBase->findConnectorItemNamed(connectorID);
	if (connectorItem != NULL) return connectorItem;

	seekLayerKin = true;
	if (seekLayerKin) {
		PaletteItem * pitem = dynamic_cast<PaletteItem *>(itemBase);
		if (pitem == NULL) return NULL;

		QList<class LayerKinPaletteItem *> layerKin = pitem->layerKin();
		for (int j = 0; j < layerKin.count(); j++) {
			connectorItem = layerKin[j]->findConnectorItemNamed(connectorID);
			if (connectorItem != NULL) return connectorItem;
		}

		return NULL;
	}

	return NULL;
}

PaletteItem * SketchWidget::getSelectedPart(){
	QList <QGraphicsItem *> items= scene()->selectedItems();
	PaletteItem *item = NULL;

	// dynamic cast returns null in cases where non-PaletteItems (i.e. wires and layerKin palette items) are selected
	for(int i=0; i < items.count(); i++){
		PaletteItem *temp = dynamic_cast<PaletteItem *>(items[i]);
		if (temp == NULL) continue;

		if (item != NULL) return NULL;  // there are multiple items selected
		item = temp;
	}

	return item;
}

void SketchWidget::setBackground(QColor color) {
	scene()->setBackgroundBrush(QBrush(color));
}

const QColor& SketchWidget::background() {
	return scene()->backgroundBrush().color();
}

void SketchWidget::setItemMenu(QMenu* itemMenu){
	m_itemMenu = itemMenu;
}

void SketchWidget::sketchWidget_wireConnected(long fromID, QString fromConnectorID, long toID, QString toConnectorID) {
	ItemBase * fromItem = findItem(fromID);
	if (fromItem == NULL) return;

	Wire* wire = dynamic_cast<Wire *>(fromItem);
	if (wire == NULL) return;

	ConnectorItem * fromConnectorItem = findConnectorItem(fromItem, fromConnectorID, false);
	if (fromConnectorItem == NULL) {
		// shouldn't be here
		return;
	}

	ItemBase * toItem = findItem(toID);
	if (toItem == NULL) {
		// this was a disconnect
		return;
	}

	ConnectorItem * toConnectorItem = findConnectorItem(toItem, toConnectorID, false);
	if (toConnectorItem == NULL) {
		// shouldn't really be here
		return;
	}

	QPointF p1(0,0), p2, pos;

	if (fromConnectorItem == wire->connector0()) {
		pos = toConnectorItem->sceneAdjustedTerminalPoint();
		ConnectorItem * toConnector1 = wire->otherConnector(fromConnectorItem)->firstConnectedToIsh();
		if (toConnector1 == NULL) {
			p2 = wire->otherConnector(fromConnectorItem)->mapToScene(wire->otherConnector(fromConnectorItem)->pos()) - pos;
		}
		else {
			p2 = toConnector1->sceneAdjustedTerminalPoint();
		}
	}
	else {
		pos = wire->pos();
		ConnectorItem * toConnector0 = wire->otherConnector(fromConnectorItem)->firstConnectedToIsh();
		if (toConnector0 == NULL) {
			pos = wire->pos();
		}
		else {
			pos = toConnector0->sceneAdjustedTerminalPoint();
		}
		p2 = toConnectorItem->sceneAdjustedTerminalPoint() - pos;
	}
	wire->setLineAnd(QLineF(p1, p2), pos, true);

	// here's the connect (model has been previously updated)
	fromConnectorItem->connectTo(toConnectorItem);
	toConnectorItem->connectTo(fromConnectorItem);

	this->update();

}

void SketchWidget::sketchWidget_wireDisconnected(long fromID, QString fromConnectorID) {
	DebugDialog::debug(tr("got wire disconnected"));
	ItemBase * fromItem = findItem(fromID);
	if (fromItem == NULL) return;

	Wire* wire = dynamic_cast<Wire *>(fromItem);
	if (wire == NULL) return;

	ConnectorItem * fromConnectorItem = findConnectorItem(fromItem, fromConnectorID, false);
	if (fromConnectorItem == NULL) {
		// shouldn't be here
		return;
	}

	ConnectorItem * toConnectorItem = fromConnectorItem->firstConnectedToIsh();
	if (toConnectorItem != NULL) {
		fromConnectorItem->removeConnection(toConnectorItem, true);
		toConnectorItem->removeConnection(fromConnectorItem, true);
	}
}

void SketchWidget::changeConnection(long fromID, const QString & fromConnectorID,
									long toID, const QString & toConnectorID,
									bool connect, bool doEmit, bool seekLayerKin,
									bool fromBusConnector, bool chain)
{
	changeConnectionAux(fromID, fromConnectorID, toID, toConnectorID, connect, seekLayerKin, fromBusConnector, chain);
	if (doEmit) {
		emit changeConnectionSignal(fromID, fromConnectorID, toID, toConnectorID, connect, fromBusConnector, chain);
	}
}

void SketchWidget::changeConnectionAux(long fromID, const QString & fromConnectorID,
									long toID, const QString & toConnectorID,
									bool connect, bool seekLayerKin, bool fromBusConnector, bool chain)
{
	DebugDialog::debug(QObject::tr("changeConnection: from %1 %2; to %3 %4 con:%5 v:%6")
				.arg(fromID).arg(fromConnectorID)
				.arg(toID).arg(toConnectorID)
				.arg(connect).arg(m_viewIdentifier) );

	ItemBase * fromItem = findItem(fromID);
	if (fromItem == NULL) {
		DebugDialog::debug("change connection exit 1");
		return;			// for now
	}

	ConnectorItem * fromConnectorItem = NULL;
	if (fromBusConnector) {
		fromConnectorItem = fromItem->busConnectorItem(fromConnectorID);
	}
	else {
		fromConnectorItem = findConnectorItem(fromItem, fromConnectorID, seekLayerKin);
	}
	if (fromConnectorItem == NULL) {
		// shouldn't be here
		DebugDialog::debug("change connection exit 2");
		return;
	}

	ItemBase * toItem = findItem(toID);
	if (toItem == NULL) {
		DebugDialog::debug("change connection exit 3");
		return;
	}

	ConnectorItem * toConnectorItem = findConnectorItem(toItem, toConnectorID, seekLayerKin);
	if (toConnectorItem == NULL) {
		// shouldn't be here
		DebugDialog::debug("change connection exit 4");
		return;
	}


	if (connect) {
		fromConnectorItem->connector()->connectTo(toConnectorItem->connector());
		fromConnectorItem->connectTo(toConnectorItem);
		toConnectorItem->connectTo(fromConnectorItem);
		fromItem->setChained(fromConnectorItem, chain);
		toItem->setChained(toConnectorItem, chain);
	}
	else {
		fromConnectorItem->connector()->disconnectFrom(toConnectorItem->connector());
		fromConnectorItem->removeConnection(toConnectorItem, true);
		toConnectorItem->removeConnection(fromConnectorItem, true);
	}


	fromConnectorItem->attachedTo()->updateConnections(fromConnectorItem);
	toConnectorItem->attachedTo()->updateConnections(toConnectorItem);
	if (fromBusConnector) {
		fromConnectorItem->adjustConnectedItems();
		BusConnectorItem * busConnectorItem = dynamic_cast<BusConnectorItem *>(fromConnectorItem);
		if (busConnectorItem != NULL) {
			busConnectorItem->updateVisibility(m_viewIdentifier);
		}
	}


	// for now treat them the same
	if ((m_viewIdentifier == ItemBase::PCBView) || (m_viewIdentifier == ItemBase::SchematicView)) {
		dealWithRatsnest(fromConnectorItem, toConnectorItem, connect);
	}
}

void SketchWidget::dealWithRatsnest(ConnectorItem * fromConnectorItem, ConnectorItem * toConnectorItem, bool connect) {

	if (fromConnectorItem->attachedToItemType() == ModelPart::Wire) {
		Wire * wire = dynamic_cast<Wire *>(fromConnectorItem->attachedTo());
		if (wire->getRatsnest()) {
			// don't make further ratsnest's from ratsnest
			return;
		}
	}

	if (toConnectorItem->attachedToItemType() == ModelPart::Wire) {
		Wire * wire = dynamic_cast<Wire *>(toConnectorItem->attachedTo());
		if (wire->getRatsnest()) {
			// don't make further ratsnest's from ratsnest
			return;
		}
	}
	
	if (connect) {
		QList<ConnectorItem *> connectorItems;
		QList<ConnectorItem *> partsConnectorItems;
		QList<BusConnectorItem *> busConnectorItems;
		connectorItems.append(fromConnectorItem);
		BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, true, ViewGeometry::NoFlag);
		BusConnectorItem::collectParts(connectorItems, partsConnectorItems);

		QList <Wire *> ratsnestWires;
		Wire * modelWire = NULL;

		if (m_viewIdentifier == ItemBase::PCBView) {
			int count = partsConnectorItems.count();
			for (int i = 0; i < count - 1; i++) {
				ConnectorItem * source = partsConnectorItems[i];
				for (int j = i + 1; j < count; j++) {
					ConnectorItem * dest = partsConnectorItems[j];
					// if you can't get from i to j via wires, then add a virtual ratsnest wire
					Wire* tempWire = source->wiredTo(dest, ViewGeometry::RatsnestFlag);
					if (tempWire == NULL) {
						Wire * newWire = makeOneRatsnestWire(source, dest);
						ratsnestWires.append(newWire);
						if (source->wiredTo(dest, ViewGeometry::TraceFlag | ViewGeometry::JumperFlag)) {
							newWire->setRouted(true);
						}

					}
					else {
						modelWire = tempWire;
					}
				}
			}
		}
		else /* if (m_viewIdentifier == ItemBase::SchematicView) */
		{
			// TODO:  add the bus connector items, if they have multiple connections
			// TODO: set all the bus connectors at the average location of the connected items
			// TODO: if drag one bus connector, move all merged

			// delete all the ratsnest wires before running dykstra
			int count = partsConnectorItems.count();
			for (int i = 0; i < count - 1; i++) {
				ConnectorItem * source = partsConnectorItems[i];
				for (int j = i + 1; j < count; j++) {
					ConnectorItem * dest = partsConnectorItems[j];
					// if you can't get from i to j via wires, then add a virtual ratsnest wire
					Wire* tempWire = source->wiredTo(dest, ViewGeometry::RatsnestFlag);
					if (tempWire != NULL) {
						deleteItem(tempWire, false, false);
					}
				}
			}

			QHash<ConnectorItem *, int> indexer;
			int ix = 0;
			foreach (ConnectorItem * connectorItem, partsConnectorItems) {
				indexer.insert(connectorItem, ix++);
			}

			QVector< QVector<double> *> adjacency(count);
			for (int i = 0; i < count; i++) {
				QVector<double> * row = new QVector<double>(count);
				adjacency[i] = row;
			}

			Autorouter1::dykstra(partsConnectorItems, indexer, adjacency);

			foreach (QVector<double> * row, adjacency) {
				delete row;
			}

			count = partsConnectorItems.count();
			for (int i = 0; i < count - 1; i++) {
				ConnectorItem * source = partsConnectorItems[i];
				ConnectorItem * dest = partsConnectorItems[i + 1];
				Wire * newWire = makeOneRatsnestWire(source, dest);
				ratsnestWires.append(newWire);
			}
		}

		const QColor * color = NULL;
		if (modelWire) {
			color = modelWire->color();
		}
		else {
			color = Wire::netColor(m_viewIdentifier);
		}
		foreach (Wire * wire, ratsnestWires) {
			wire->setColor((QColor) *color, wire->getRouted() ? 0.35 : 1.0);
		}


		return;
	}

	QList<ConnectorItem *> fromConnectorItems;
	QList<ConnectorItem *> fromPartsConnectorItems;
	QList<BusConnectorItem *> fromBusConnectorItems;
	fromConnectorItems.append(fromConnectorItem);
	BusConnectorItem::collectEqualPotential(fromConnectorItems, fromBusConnectorItems, true, ViewGeometry::NoFlag);
	BusConnectorItem::collectParts(fromConnectorItems, fromPartsConnectorItems);

	QList<ConnectorItem *> toConnectorItems;
	QList<ConnectorItem *> toPartsConnectorItems;
	QList<BusConnectorItem *> toBusConnectorItems;
	toConnectorItems.append(toConnectorItem);
	BusConnectorItem::collectEqualPotential(toConnectorItems, toBusConnectorItems, true, ViewGeometry::NoFlag);
	BusConnectorItem::collectParts(toConnectorItems, toPartsConnectorItems);

	foreach (ConnectorItem * from, fromPartsConnectorItems) {
		foreach (ConnectorItem * to, toPartsConnectorItems) {
			Wire * wire = from->wiredTo(to, ViewGeometry::RatsnestFlag);
			if (wire != NULL) {
				deleteItem(wire, false, false);
			}
		}
	}
}

Wire * SketchWidget::makeOneRatsnestWire(ConnectorItem * source, ConnectorItem * dest) {
	long newID = ItemBase::getNextID();
	ViewGeometry viewGeometry;
	QPointF fromPos = source->sceneAdjustedTerminalPoint();
	viewGeometry.setLoc(fromPos);
	QPointF toPos = dest->sceneAdjustedTerminalPoint();
	QLineF line(0, 0, toPos.x() - fromPos.x(), toPos.y() - fromPos.y());
	viewGeometry.setLine(line);
	viewGeometry.setWireFlags(ViewGeometry::RatsnestFlag | ViewGeometry::VirtualFlag);

	/*
	 DebugDialog::debug(QString("creating ratsnest %10: %1, from %6 %7, to %8 %9, frompos: %2 %3, topos: %4 %5")
	 .arg(newID)
	 .arg(fromPos.x()).arg(fromPos.y())
	 .arg(toPos.x()).arg(toPos.y())
	 .arg(source->attachedToTitle()).arg(source->connectorStuffID())
	 .arg(dest->attachedToTitle()).arg(dest->connectorStuffID())
	 .arg(m_viewIdentifier)
	 );
	 */

	ItemBase * newItemBase = addItemAux(m_paletteModel->retrieveModelPart(Wire::moduleIDName), viewGeometry, newID, NULL, true);
	tempConnectWire(newItemBase, source, dest);
	return  dynamic_cast<Wire *>(newItemBase);
}


void SketchWidget::tempConnectWire(ItemBase * itemBase, ConnectorItem * from, ConnectorItem * to) {
	Wire * wire = dynamic_cast<Wire *>(itemBase);
	if (wire == NULL) return;

	ConnectorItem * connector0 = wire->connector0();
	from->tempConnectTo(connector0);
	connector0->tempConnectTo(from);
	ConnectorItem * connector1 = wire->connector1();
	to->tempConnectTo(connector1);
	connector1->tempConnectTo(to);
}

void SketchWidget::sketchWidget_changeConnection(long fromID, QString fromConnectorID,
													   long toID, QString toConnectorID, bool connect,
													   bool fromBusConnector, bool chain) {
	changeConnection(fromID, fromConnectorID,
					 toID, toConnectorID,
					 connect, false, true, fromBusConnector, chain);
}

void SketchWidget::navigatorScrollChange(double x, double y) {
	QScrollBar * h = this->horizontalScrollBar();
	QScrollBar * v = this->verticalScrollBar();
	int xmin = h->minimum();
   	int xmax = h->maximum();
   	int ymin = v->minimum();
   	int ymax = v->maximum();

   	h->setValue((int) ((xmax - xmin) * x) + xmin);
   	v->setValue((int) ((ymax - ymin) * y) + ymin);
}

void SketchWidget::item_connectionChanged(ConnectorItem * from, ConnectorItem * to, bool connect)
{
	if (connect) {
		this->m_needToConnectItems.insert(from, to);
		DebugDialog::debug(QString("need to connect %1 %2 %3, %4 %5 %6")
				.arg(from->attachedToID())
				.arg(from->attachedToTitle())
				.arg(from->connectorStuffID())
				.arg(to->attachedToID())
				.arg(to->attachedToTitle())
				.arg(to->connectorStuffID()) );
	}
	else {
		//this->m_needToDisconnectItems.insert(from, to);
	}
}

BusConnectorItem * SketchWidget::initializeBusConnectorItem(long busOwnerID, const QString & busID, bool doEmit)
{
	DebugDialog::debug(QString("initializing bus %1 %2, %3 ").arg(busOwnerID).arg(busID).arg(m_viewIdentifier) );

	ItemBase * busItemBase = findItem(busOwnerID);
	if (busItemBase == NULL) return NULL;

	Bus * bus = busItemBase->buses().value(busID);
	if (bus == NULL) return NULL;

	BusConnectorItem * busConnectorItem = busItemBase->busConnectorItem(busID);
	if (busConnectorItem != NULL) {
		busConnectorItem->initialize(m_viewIdentifier);
	}

	if (doEmit) {
		emit initializeBusConnectorItemSignal(busOwnerID, busID);
	}

	return busConnectorItem;
}


void SketchWidget::sketchWidget_initializeBusConnectorItem(long busOwnerID, const QString & busID) {
	initializeBusConnectorItem(busOwnerID, busID, false);
}

void SketchWidget::keyPressEvent ( QKeyEvent * event ) {
	if(event->key() == Qt::Key_Space) {
		if (dragMode() == QGraphicsView::RubberBandDrag) {
			setDragMode(QGraphicsView::ScrollHandDrag);
			setCursor(Qt::OpenHandCursor);
		}
		else {
			setDragMode(QGraphicsView::RubberBandDrag);
			setCursor(Qt::ArrowCursor);
		}
	}

	QGraphicsView::keyPressEvent(event);
}

void SketchWidget::mergeBuses(long bus1OwnerID, const QString & bus1ID, QPointF bus1Pos,
							  long bus2OwnerID, const QString & bus2ID, QPointF bus2Pos,
							  bool merge, bool doEmit)
{

	DebugDialog::debug(QString("merge buses %1 %2 %3 %4 %5 %6")
					   .arg(bus1OwnerID).arg(bus1ID).arg(bus2OwnerID).arg(bus2ID).arg(merge).arg(m_viewIdentifier) );
	ItemBase * owner1 = findItem(bus1OwnerID);
	if (owner1 == NULL) return;

	BusConnectorItem * item1 = owner1->busConnectorItem(bus1ID);
	if (item1 == NULL) return;

	ItemBase * owner2 = findItem(bus2OwnerID);
	if (owner2 == NULL) return;

	BusConnectorItem * item2 = owner2->busConnectorItem(bus2ID);
	if (item2 == NULL) return;

	if (merge) {
		item1->merge(item2);
		item2->merge(item1);

		if (m_viewIdentifier == ItemBase::SchematicView) {
			//item1->mergeGraphicsDelay(item2, false, m_viewIdentifier);
			// make them the same for now
			//item1->mergeGraphicsDelay(item2, true, m_viewIdentifier);
		}
		else if (m_viewIdentifier == ItemBase::PCBView) {
			//item1->mergeGraphicsDelay(item2, true, m_viewIdentifier);
		}
	}
	else {
		item1->unmerge(item2);
		item2->unmerge(item1);

		// TODO: should item1 or item2 keep the group?

		if (m_viewIdentifier == ItemBase::SchematicView) {
			//item1->unmergeGraphics(item2, false, m_viewIdentifier, bus2Pos);
			// make them the same for now
			//item1->unmergeGraphics(item2, true, m_viewIdentifier, bus2Pos);
		}
		else if (m_viewIdentifier == ItemBase::PCBView) {
			//item1->unmergeGraphics(item2, true, m_viewIdentifier, bus2Pos);
		}
	}

	if (doEmit) {
		emit mergeBusesSignal(bus1OwnerID, bus1ID, bus1Pos, bus2OwnerID, bus2ID, bus2Pos, merge);
	}
}

void SketchWidget::sketchWidget_mergeBuses(long bus1OwnerID, const QString & bus1ID, QPointF bus1Pos,
								long bus2OwnerID, const QString & bus2ID, QPointF bus2Pos, bool merge)
{
	mergeBuses(bus1OwnerID, bus1ID, bus1Pos, bus2OwnerID, bus2ID, bus2Pos, merge, false);
}

void SketchWidget::sketchWidget_copyItem(long itemID, QHash<ItemBase::ViewIdentifier, ViewGeometry *> & geometryHash) {
	ItemBase * itemBase = findItem(itemID);
	if (itemBase == NULL) {
		// this shouldn't happen
		return;
	}

	ViewGeometry * vg = new ViewGeometry(itemBase->getViewGeometry());
	geometryHash.insert(m_viewIdentifier, vg);
}

void SketchWidget::sketchWidget_deleteItem(long itemID, QUndoCommand * parentCommand) {
	ItemBase * itemBase = findItem(itemID);
	if (itemBase == NULL) return;

	makeDeleteItemCommand(itemBase, parentCommand);
}

void SketchWidget::makeDeleteItemCommand(ItemBase * itemBase, QUndoCommand * parentCommand) {
	if (itemBase->itemType() == ModelPart::Wire) {
		Wire * wire = dynamic_cast<Wire *>(itemBase);
		new WireWidthChangeCommand(this, wire->id(), wire->width(), wire->width(), parentCommand);
		new WireColorChangeCommand(this, wire->id(), wire->colorString(), wire->colorString(), wire->opacity(), wire->opacity(), parentCommand);
	}
	new DeleteItemCommand(this, BaseCommand::SingleView, itemBase->modelPart()->moduleID(), itemBase->getViewGeometry(), itemBase->id(), parentCommand);
}

ItemBase::ViewIdentifier SketchWidget::viewIdentifier() {
	return m_viewIdentifier;
}


void SketchWidget::setViewLayerIDs(ViewLayer::ViewLayerID part, ViewLayer::ViewLayerID wire, ViewLayer::ViewLayerID connector, ViewLayer::ViewLayerID ruler) {
	m_partViewLayerID = part;
	m_wireViewLayerID = wire;
	m_connectorViewLayerID = connector;
	m_rulerViewLayerID = ruler;
}

void SketchWidget::dragIsDoneSlot(ItemDrag * itemDrag) {
	disconnect(itemDrag, SIGNAL(dragIsDoneSignal(ItemDrag *)), this, SLOT(dragIsDoneSlot(ItemDrag *)));
	killDroppingItem();			// 		// drag is done, but nothing dropped here: remove the temporary item
}


void SketchWidget::clearTemporaries() {
	for (int i = 0; i < m_temporaries.count(); i++) {
		QGraphicsItem * item = m_temporaries[i];
		BusConnectorItem * busConnectorItem = dynamic_cast<BusConnectorItem *>(item);
		if (busConnectorItem != NULL) {
			for (int j = busConnectorItem->connectedToItems().count() - 1; j >= 0; j--) {
				busConnectorItem->connectedToItems()[j]->tempRemove(busConnectorItem);
				busConnectorItem->tempRemove(busConnectorItem->connectedToItems()[j]);
			}
			busConnectorItem->attachedTo()->removeBusConnectorItem(busConnectorItem->bus());
		}
		else {
			VirtualWire * virtualWire = dynamic_cast<VirtualWire *>(item);
			if (virtualWire != NULL) {
				virtualWire->tempRemoveAllConnections();
			}
			else {
				// this shouldn't happen
				continue;
			}
		}

		scene()->removeItem(item);
		delete item;
	}

	m_temporaries.clear();
}

void SketchWidget::killDroppingItem() {
	// was only a temporary placeholder, get rid of it now
	if (m_droppingItem != NULL) {
		m_droppingItem->removeLayerKin();
		this->scene()->removeItem(m_droppingItem);
		delete m_droppingItem;
		m_droppingItem = NULL;
	}
}

ViewLayer::ViewLayerID SketchWidget::getViewLayerID(ModelPart * modelPart) {

	QDomElement layers = LayerAttributes::getSvgElementLayers(modelPart->modelPartStuff()->domDocument(), m_viewIdentifier);
	if (layers.isNull()) return ViewLayer::UnknownLayer;

	QDomElement layer = layers.firstChildElement("layer");
	if (layer.isNull()) return ViewLayer::UnknownLayer;

	QString layerName = layer.attribute("layerId");
	int layerCount = 0;
	while (!layer.isNull()) {
		if (++layerCount > 1) break;

		layer = layer.nextSiblingElement("layer");
	}

	if (layerCount == 1) {
		return ViewLayer::viewLayerIDFromXmlString(layerName);
	}

	if (m_viewIdentifier == ItemBase::PCBView) {
		return ViewLayer::Copper0;
	}

	return ViewLayer::viewLayerIDFromXmlString(layerName);
}

ItemBase * SketchWidget::overSticky(ItemBase * itemBase) {
	foreach (QGraphicsItem * childItem, itemBase->childItems()) {
		ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(childItem);
		if (connectorItem == NULL) continue;

		QPointF p = connectorItem->sceneAdjustedTerminalPoint();
		foreach (QGraphicsItem * item,  this->scene()->items(p)) {
			ItemBase * s = dynamic_cast<ItemBase *>(item);
			if (s == NULL) continue;

			if (s == connectorItem->attachedTo()) continue;
			if (!s->sticky()) continue;

			return s;
		}
	}

	return NULL;
}


void SketchWidget::stickem(long stickTargetID, long stickSourceID, bool stick) {
	ItemBase * stickTarget = findItem(stickTargetID);
	if (stickTarget == NULL) return;

	ItemBase * stickSource = findItem(stickSourceID);
	if (stickSource == NULL) return;

	stickTarget->addSticky(stickSource, stick);
	stickSource->addSticky(stickTarget, stick);
}

void SketchWidget::setChainDrag(bool chainDrag) {
	m_chainDrag = chainDrag;
}

void SketchWidget::stickyScoop(ItemBase * stickyOne, QUndoCommand * parentCommand) {
	foreach (QGraphicsItem * item, scene()->collidingItems(stickyOne)) {
		ItemBase * itemBase = dynamic_cast<ItemBase *>(item);
		if (itemBase == NULL) continue;
		if (itemBase->sticky()) continue;
		if (stickyOne->alreadySticking(itemBase)) continue;
		if (!stickyOne->stickyEnabled(itemBase)) continue;

		bool gotOne = false;
		foreach (QGraphicsItem * childItem, itemBase->childItems()) {
			ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(childItem);
			if (connectorItem == NULL) continue;

			QPointF p = connectorItem->sceneAdjustedTerminalPoint();
			if (stickyOne->contains(stickyOne->mapFromScene(p))) {
				gotOne = true;
				break;
			}
		}
		if (!gotOne) continue;

		if (parentCommand == NULL) {
			stickyOne->addSticky(itemBase, true);
			itemBase->addSticky(stickyOne, true);
		}
		else {
			new StickyCommand(this, stickyOne->id(), itemBase->id(), true, parentCommand);
		}

	}
}

void SketchWidget::wire_wireSplit(Wire* wire, QPointF newPos, QPointF oldPos, QLineF oldLine) {
	if (!m_chainDrag) return;  // can't split a wire in some views (for now)

	this->clearHoldingSelectItem();
	this->m_moveEventCount = 0;  // clear this so an extra MoveItemCommand isn't posted

	QUndoCommand * parentCommand = new QUndoCommand();
	new CleanUpWiresCommand(this, false, parentCommand);
	parentCommand->setText(QObject::tr("Split Wire") );

	long fromID = wire->id();

	QLineF newLine(oldLine.p1(), newPos - oldPos);
	new ChangeWireCommand(this, fromID, oldLine, newLine, oldPos, oldPos, true, parentCommand);

	long newID = ItemBase::getNextID();
	ViewGeometry vg(wire->getViewGeometry());
	vg.setLoc(newPos);
	QLineF newLine2(QPointF(0,0), oldLine.p2() + oldPos - newPos);
	vg.setLine(newLine2);

	new AddItemCommand(this, BaseCommand::SingleView, Wire::moduleIDName, vg, newID, parentCommand);
	new WireColorChangeCommand(this, newID, wire->colorString(), wire->colorString(), wire->opacity(), wire->opacity(), parentCommand);
	new WireWidthChangeCommand(this, newID, wire->width(), wire->width(), parentCommand);

	// disconnect from original wire and reconnect to new wire
	ConnectorItem * connector1 = wire->connector1();
	foreach (ConnectorItem * toConnectorItem, connector1->connectedToItems()) {
		bool isBusConnector = (dynamic_cast<BusConnectorItem *>(toConnectorItem) != NULL);
		new ChangeConnectionCommand(this, BaseCommand::SingleView, toConnectorItem->attachedToID(), toConnectorItem->connectorStuffID(),
			wire->id(), connector1->connectorStuffID(),
			false, true, isBusConnector, toConnectorItem->chained(), parentCommand);
		new ChangeConnectionCommand(this, BaseCommand::SingleView, toConnectorItem->attachedToID(), toConnectorItem->connectorStuffID(),
			newID, connector1->connectorStuffID(),
			true, true, isBusConnector, toConnectorItem->chained(), parentCommand);
	}

	// connect the two wires
	new ChangeConnectionCommand(this, BaseCommand::SingleView, wire->id(), connector1->connectorStuffID(),
			newID, "connector0", true, true, false, true, parentCommand);


	//checkSticky(wire, parentCommand);

	new CleanUpWiresCommand(this, true, parentCommand);
	m_undoStack->push(parentCommand);
}

void SketchWidget::wire_wireJoin(Wire* wire, ConnectorItem * clickedConnectorItem) {
	if (!m_chainDrag) return;  // can't join a wire in some views (for now)

	this->clearHoldingSelectItem();
	this->m_moveEventCount = 0;  // clear this so an extra MoveItemCommand isn't posted

	QUndoCommand * parentCommand = new QUndoCommand();
	parentCommand->setText(QObject::tr("Join Wire") );

	// assumes there is one and only one item connected
	ConnectorItem * toConnectorItem = clickedConnectorItem->connectedToItems()[0];
	if (toConnectorItem == NULL) return;

	Wire * toWire = dynamic_cast<Wire *>(toConnectorItem->attachedTo());
	if (toWire == NULL) return;

	if (wire->id() > toWire->id()) {
		// delete the wire with the higher id
		// so we can keep the three views in sync
		// i.e. the original wire has the lowest id in the chain
		Wire * wtemp = toWire;
		toWire = wire;
		wire = wtemp;
		ConnectorItem * ctemp = toConnectorItem;
		toConnectorItem = clickedConnectorItem;
		clickedConnectorItem = ctemp;
	}

	ConnectorItem * otherConnector = toWire->otherConnector(toConnectorItem);

	// disconnect the wires
	new ChangeConnectionCommand(this, BaseCommand::SingleView, wire->id(), clickedConnectorItem->connectorStuffID(),
			toWire->id(), toConnectorItem->connectorStuffID(), false, true, false, true, parentCommand);

	// disconnect everyone from the other end of the wire being deleted, and reconnect to the remaining wire
	foreach (ConnectorItem * otherToConnectorItem, otherConnector->connectedToItems()) {
		bool isBusConnector = (dynamic_cast<BusConnectorItem *>(otherToConnectorItem) != NULL);
		new ChangeConnectionCommand(this, BaseCommand::SingleView, otherToConnectorItem->attachedToID(), otherToConnectorItem->connectorStuffID(),
			toWire->id(), otherConnector->connectorStuffID(),
			false, true, isBusConnector, otherToConnectorItem->chained(), parentCommand);
		new ChangeConnectionCommand(this, BaseCommand::SingleView, otherToConnectorItem->attachedToID(), otherToConnectorItem->connectorStuffID(),
			wire->id(), clickedConnectorItem->connectorStuffID(),
			true, true, isBusConnector, otherToConnectorItem->chained(), parentCommand);
	}

	toWire->saveGeometry();
	makeDeleteItemCommand(toWire, parentCommand);

	QLineF newLine;
	QPointF newPos;
	if (otherConnector == toWire->connector1()) {
		newPos = wire->pos();
		newLine = QLineF(QPointF(0,0), toWire->pos() - wire->pos() + toWire->line().p2());
	}
	else {
		newPos = toWire->pos();
		newLine = QLineF(QPointF(0,0), wire->pos() - toWire->pos() + wire->line().p2());
	}
	new ChangeWireCommand(this, wire->id(), wire->line(), newLine, wire->pos(), newPos, true, parentCommand);


	//checkSticky(wire, parentCommand);

	m_undoStack->push(parentCommand);
}

void SketchWidget::hoverEnterItem(QGraphicsSceneHoverEvent * event, ItemBase * item) {
	if(m_infoViewOnHover || currentlyInfoviewed(item)) {
		InfoGraphicsView::hoverEnterItem(event, item);
	}

	if (!this->m_chainDrag) return;

	Wire * wire = dynamic_cast<Wire *>(item);
	if (wire == NULL) return;

	statusMessage(tr("Shift-click to add a bend point to the wire"));
}

void SketchWidget::statusMessage(QString message, int timeout) {
	QMainWindow * mainWindow = dynamic_cast<QMainWindow *>(window());
	if (mainWindow == NULL) return;

	mainWindow->statusBar()->showMessage(message, timeout);
}

void SketchWidget::hoverLeaveItem(QGraphicsSceneHoverEvent * event, ItemBase * item){
	if(m_infoViewOnHover) {
		InfoGraphicsView::hoverLeaveItem(event, item);
	}

	if (!this->m_chainDrag) return;

	Wire * wire = dynamic_cast<Wire *>(item);
	if (wire == NULL) return;;

	statusMessage(tr(""));
}

void SketchWidget::hoverEnterConnectorItem(QGraphicsSceneHoverEvent * event, ConnectorItem * item) {
	Q_UNUSED(event)
	if(m_infoViewOnHover || currentlyInfoviewed(item->attachedTo())) {
		viewConnectorItemInfo(item);
	}

	if (!this->m_chainDrag) return;
	if (!item->chained()) return;

	statusMessage(tr("Shift-click to delete this bend point"));
}

void SketchWidget::hoverLeaveConnectorItem(QGraphicsSceneHoverEvent * event, ConnectorItem * item){
	ItemBase* attachedTo = item->attachedTo();
	if(attachedTo) {
		if(m_infoViewOnHover || currentlyInfoviewed(attachedTo)) {
			InfoGraphicsView::hoverLeaveConnectorItem(event, item);
			if(attachedTo->collidesWithItem(item)) {
				hoverEnterItem(event,attachedTo);
			}
		}
		attachedTo->hoverLeaveConnectorItem(event, item);
	}

	if (!this->m_chainDrag) return;
	if (!item->chained()) return;

	statusMessage(tr(""));
}

bool SketchWidget::currentlyInfoviewed(ItemBase *item) {
	if(m_infoView) {
		ItemBase * currInfoView = m_infoView->currentItem();
		return !currInfoView || item == currInfoView;//->cu selItems.size()==1 && selItems[0] == item;
	}
	return false;
}

void SketchWidget::cleanUpWires(bool doEmit) {
	cleanUpWiresAux();

	if (doEmit) {
		emit cleanUpWiresSignal();
	}
}

void SketchWidget::sketchWidget_cleanUpWires() {
	cleanUpWiresAux();
}

void SketchWidget::cleanUpWiresAux() {
	QList<Wire *> wires;
	QList<QGraphicsItem *>items = scene()->items();
	// TODO: get rid of scene()->items()
	foreach (QGraphicsItem * item, items) {
		Wire * wire = dynamic_cast<Wire *>(item);
		if (wire == NULL) continue;
		if (wires.contains(wire)) continue;			// already processed

		cleanUpWire(wire, wires);
	}
}

void SketchWidget::cleanUpWire(Wire * wire, QList<Wire *> & wires)
{
	DebugDialog::debug(QString("clean up wire %1 %2").arg(wire->id()).arg(m_viewIdentifier) );
	switch (m_viewIdentifier) {
		case ItemBase::BreadboardView:
			wire->setVisible(!wire->getVirtual());
			return;
		case ItemBase::SchematicView:
			wire->setVisible(wire->getRatsnest() || wire->getTrace() || wire->getJumper());
			//break;
			return;  // for now make them the same
		case ItemBase::PCBView:
			wire->setVisible(wire->getRatsnest() || wire->getTrace() || wire->getJumper());
			return;
		default:
			return;
	}


	QList<ConnectorItem *> connectorItems;
	QList<BusConnectorItem *> busConnectorItems;
	connectorItems.append(wire->connector0());
	BusConnectorItem::collectEqualPotential(connectorItems, busConnectorItems, false, ViewGeometry::NoFlag);

	QList<ConnectorItem *> partsConnectors;
	BusConnectorItem::collectParts(connectorItems, partsConnectors);

	QList<Wire *> localWires;
	wire->collectWires(localWires, false);			// find all the wires connected to this wire
	foreach (Wire * localWire, localWires) {
		// don't process these again
		wires.append(localWire);
	}

	bool vis = false;
	if (partsConnectors.count() > 0) {
		// temporarily disconnect wires, holding onto current connection state
		QMultiHash<ConnectorItem *, ConnectorItem *> connectionState;
		foreach (Wire * localWire, localWires) {
			tempDisconnectWire(localWire->connector0(), connectionState);
			tempDisconnectWire(localWire->connector1(), connectionState);
		}

		QList<ConnectorItem *> connectorItems2;
		QList<BusConnectorItem *> busConnectorItems2;
		connectorItems2.append(partsConnectors[0]);
		BusConnectorItem::collectEqualPotential(connectorItems2, busConnectorItems2, true, ViewGeometry::NoFlag);

		// restore connections
		foreach (ConnectorItem * fromConnectorItem, connectionState.uniqueKeys()) {
			foreach (ConnectorItem * toConnectorItem, connectionState.values(fromConnectorItem)) {
				fromConnectorItem->tempConnectTo(toConnectorItem);
				toConnectorItem->tempConnectTo(fromConnectorItem);

				//DebugDialog::debug(QString("reconnect dis %1 %2 %3 %4")
				//.arg(fromConnectorItem->attachedToID())
				//.arg(fromConnectorItem->connectorStuffID())
				//.arg(toConnectorItem->attachedToID())
				//.arg(toConnectorItem->connectorStuffID()) );
			}
		}

		QList<ConnectorItem *> partsConnectors2;
		foreach (ConnectorItem * connectorItem, connectorItems2) {
			ItemBase * candidate = connectorItem->attachedTo();
			if (candidate->itemType() == ModelPart::Part) {
				if (!partsConnectors2.contains(connectorItem)) {
					partsConnectors2.append(connectorItem);
					//DebugDialog::debug(QString("recollecting part %1 %2").arg(candidate->id()).arg(connectorItem->connectorStuffID()) );
				}
			}
		}

		foreach (ConnectorItem * connectorItem, partsConnectors) {
			if (!partsConnectors2.contains(connectorItem)) {
				// disconnecting this wire and its connected wires disconnected this part, so show wire
				vis = true;
				break;
			}
		}
	}

	foreach (Wire * localWire, localWires) {
		localWire->setVisible(vis);
	}
}



void SketchWidget::tempDisconnectWire(ConnectorItem * fromConnectorItem, QMultiHash<ConnectorItem *, ConnectorItem *> & connectionState) {
	foreach (ConnectorItem * toConnectorItem, fromConnectorItem->connectedToItems()) {
		connectionState.insert(fromConnectorItem, toConnectorItem);
		fromConnectorItem->tempRemove(toConnectorItem);
		toConnectorItem->tempRemove(fromConnectorItem);
		//DebugDialog::debug(QString("temp dis %1 %2 %3 %4")
			//.arg(fromConnectorItem->attachedToID())
			//.arg(fromConnectorItem->connectorStuffID())
			//.arg(toConnectorItem->attachedToID())
			//.arg(toConnectorItem->connectorStuffID()) );
	}
}

void SketchWidget::setItemTooltip(long id, const QString &newTooltip) {
	ItemBase * pitem = findItem(id);
	if (pitem != NULL) {
		pitem->setInstanceTitleAndTooltip(newTooltip);
		emit tooltipAppliedToItem(id, newTooltip);
	}
}

void SketchWidget::setInfoViewOnHover(bool infoViewOnHover) {
	m_infoViewOnHover = infoViewOnHover;
}

void SketchWidget::updateInfoView() {
	QTimer *timer = new QTimer(this);
	connect(timer, SIGNAL(timeout()), this, SLOT(updateInfoViewSlot()));
	timer->setSingleShot(true);
	timer->start(50);
}

void SketchWidget::updateInfoViewSlot() {
	InfoGraphicsView::viewItemInfo(m_lastPaletteItemSelected);
}

void SketchWidget::swapSelected(ModelPart* other) {
	swapSelected(other->moduleID());
}

void SketchWidget::swapSelected(const QString &moduleID) {
	if(m_lastPaletteItemSelected) {
		QUndoCommand* parentCommand = new QUndoCommand(tr("Swapped %1 with module %2").arg(m_lastPaletteItemSelected->instanceTitle()).arg(moduleID));
		new SwapCommand(
				this,
				m_lastPaletteItemSelected->id(),
				m_lastPaletteItemSelected->modelPart()->moduleID(),
				moduleID,
				parentCommand);
		m_undoStack->push(parentCommand);
	}
}

void SketchWidget::swapSelected(PaletteItem* other) {
	swapSelected(other->modelPart());
}

void SketchWidget::swap(PaletteItem* from, ModelPart *to) {
	if(from && to) {
		from->swap(to, m_viewLayers);
		updateInfoView();
	}
}

void SketchWidget::swap(long itemId, const QString &moduleID, bool doEmit) {
	swap(itemId, m_refModel->retrieveModelPart(moduleID), doEmit);
}

void SketchWidget::swap(long itemId, ModelPart *to, bool doEmit) {
	PaletteItem *from = dynamic_cast<PaletteItem*>(findItem(itemId));
	if(from && to) {
		swap(from,to);

		// let's make sure that the icon pixmap will be available for the infoview
		LayerAttributes layerAttributes;
		from->setUpImage(from->modelPart(), ItemBase::IconView, ViewLayer::Icon, layerAttributes);

		if(doEmit) {
			emit swapped(itemId, to);
		}
	}
}

void SketchWidget::changeWireColor(const QString &wireTitle, long wireId,
								   const QString& oldColor, const QString newColor,
								   qreal oldOpacity, qreal newOpacity) {
	QUndoCommand* parentCommand = new QUndoCommand(
		tr("Wire %1 color changed from %2 to %3")
			.arg(wireTitle)
			.arg(oldColor)
			.arg(newColor)
	);
	new WireColorChangeCommand(
			this,
			wireId,
			oldColor,
			newColor,
			oldOpacity,
			newOpacity,
			parentCommand);
	m_undoStack->push(parentCommand);
}

void SketchWidget::changeWireColor(long wireId, const QString& color, qreal opacity) {
	ItemBase *item = findItem(wireId);
	if(Wire* wire = dynamic_cast<Wire*>(item)) {
		wire->setColorString(color, opacity);
	}
}

void SketchWidget::changeWireWidth(long wireId, int width) {
	ItemBase *item = findItem(wireId);
	if(Wire* wire = dynamic_cast<Wire*>(item)) {
		wire->setWidth(width);
	}
}

PaletteModel * SketchWidget::paletteModel() {
	return m_paletteModel;
}

bool SketchWidget::swappingEnabled() {
	return m_refModel->swapEnabled();
}

void SketchWidget::resizeEvent(QResizeEvent * event) {
	InfoGraphicsView::resizeEvent(event);
	emit resizeSignal();
}

void SketchWidget::addBreadboardViewLayers() {
	setViewLayerIDs(ViewLayer::Breadboard, ViewLayer::BreadboardWire, ViewLayer::Breadboard, ViewLayer::BreadboardRuler);

	QList<ViewLayer::ViewLayerID> layers;
	layers << ViewLayer::BreadboardBreadboard << ViewLayer::Breadboard
		<< ViewLayer::BreadboardWire << ViewLayer::BreadboardRuler;

	addViewLayersAux(layers);
}

void SketchWidget::addSchematicViewLayers() {
	setViewLayerIDs(ViewLayer::Schematic, ViewLayer::SchematicWire, ViewLayer::Schematic, ViewLayer::SchematicRuler);

	QList<ViewLayer::ViewLayerID> layers;
	layers << ViewLayer::Schematic << ViewLayer::SchematicWire << ViewLayer::SchematicRuler;

	addViewLayersAux(layers);
}

void SketchWidget::addPcbViewLayers() {
	setViewLayerIDs(ViewLayer::Silkscreen, ViewLayer::Ratsnest, ViewLayer::Copper0, ViewLayer::PcbRuler);

	QList<ViewLayer::ViewLayerID> layers;
	layers << ViewLayer::Board << ViewLayer::Copper1 << ViewLayer::Copper0 << ViewLayer::Keepout
		<< ViewLayer::Vias << ViewLayer::Soldermask << ViewLayer::Silkscreen << ViewLayer::Outline
		<< ViewLayer::Jumperwires << ViewLayer::Ratsnest << ViewLayer::PcbRuler;

	addViewLayersAux(layers);
}

void SketchWidget::addViewLayersAux(const QList<ViewLayer::ViewLayerID> &layers, float startZ) {
	float z = startZ;
	foreach(ViewLayer::ViewLayerID vlId, layers) {
		addViewLayer(new ViewLayer(vlId, true, z));
		z+=1;
	}
}

void SketchWidget::setIgnoreSelectionChangeEvents(bool ignore) {
	m_ignoreSelectionChangeEvents = ignore;
}

void SketchWidget::createJumper() {
	QString commandString = "Create jumper wire";
	QString colorString = "jumper";
	createJumperOrTrace(commandString, ViewGeometry::JumperFlag, colorString);
}


void SketchWidget::createTrace() {
	QString commandString = tr("Create trace wire");
	QString colorString = "trace";
	createJumperOrTrace(commandString, ViewGeometry::TraceFlag, colorString);
}

void SketchWidget::createJumperOrTrace(const QString & commandString, ViewGeometry::WireFlag flag, const QString & colorString)
{
	QList<QGraphicsItem *> items = scene()->selectedItems();
	if (items.count() != 1) return;

	Wire * wire = dynamic_cast<Wire *>(items[0]);
	if (wire == NULL) return;

	if (!wire->getRatsnest()) return;

	QList<ConnectorItem *> ends;
	Wire * jumperOrTrace = wire->findJumperOrTraced(flag, ends);
	QUndoCommand * parentCommand = new QUndoCommand(commandString);
	if (jumperOrTrace != NULL) {
		new WireFlagChangeCommand(this, wire->id(), wire->wireFlags(), wire->wireFlags() | ViewGeometry::RoutedFlag, parentCommand);
		new WireColorChangeCommand(this, wire->id(), wire->colorString(), "unrouted", wire->opacity(), .35, parentCommand);
	}
	else {
		long newID = createWire(ends[0], ends[1], false, flag, false, BaseCommand::SingleView, parentCommand);
		new WireColorChangeCommand(this, newID, colorString, colorString, 1.0, 1.0, parentCommand);
		new WireWidthChangeCommand(this, newID, 3, 3, parentCommand);
		new WireColorChangeCommand(this, wire->id(), wire->colorString(), "unrouted", 0.35, .35, parentCommand);
		new WireFlagChangeCommand(this, wire->id(), wire->wireFlags(), wire->wireFlags() | ViewGeometry::RoutedFlag, parentCommand);
	}
	m_undoStack->push(parentCommand);
}

void SketchWidget::hideConnectors(bool hide) {
	foreach (QGraphicsItem * item, scene()->items()) {
		ItemBase * itemBase = dynamic_cast<ItemBase *>(item);
		if (itemBase == NULL) continue;
		if (!itemBase->isVisible()) continue;

		foreach (QGraphicsItem * childItem, itemBase->childItems()) {
			ConnectorItem * connectorItem = dynamic_cast<ConnectorItem *>(childItem);
			if (connectorItem == NULL) continue;

			connectorItem->setVisible(!hide);
		}
	}
}

void SketchWidget::saveLayerVisibility()
{
	m_viewLayerVisibility.clear();
	foreach (ViewLayer::ViewLayerID viewLayerID, m_viewLayers.keys()) {
		ViewLayer * viewLayer = m_viewLayers.value(viewLayerID);
		if (viewLayer == NULL) continue;

		m_viewLayerVisibility.insert(viewLayerID, viewLayer->visible());
	}
}

void SketchWidget::restoreLayerVisibility()
{
	foreach (ViewLayer::ViewLayerID viewLayerID, m_viewLayerVisibility.keys()) {
		setLayerVisible(m_viewLayers.value(viewLayerID),  m_viewLayerVisibility.value(viewLayerID));
	}
}

bool SketchWidget::ratsAllRouted() {
	foreach (QGraphicsItem * item, scene()->items()) {
		Wire * wire = dynamic_cast<Wire *>(item);
		if (wire == NULL) continue;
		if (!wire->getRatsnest()) continue;

		if (!wire->getRouted()) return false;
	}

	return true;
}

void SketchWidget::changeWireFlags(long wireId, ViewGeometry::WireFlags wireFlags)
{
	ItemBase *item = findItem(wireId);
	if(Wire* wire = dynamic_cast<Wire*>(item)) {
		wire->setWireFlags(wireFlags);
	}
}

void SketchWidget::collectBusConnectorItems(QList<BusConnectorItem *> & busConnectorItems) {
	// collect all the bus connectors in the scene, so we can figure out which ones
	// will need to be split up
	// this seems like the quickest approach since we may be moving/rotating/flipping/deleting many items

	// TODO: replace scene()->items()

	foreach (QGraphicsItem * item,  scene()->items()) {
		BusConnectorItem * busConnectorItem = dynamic_cast<BusConnectorItem *>(item);
		if (busConnectorItem != NULL && busConnectorItem->connectionsCount() > 0) {
			busConnectorItems.append(busConnectorItem);
		}
	}
}

void SketchWidget::disconnectFromFemale(ItemBase * item, QList<ItemBase *> & savedItems, QSet <VirtualWire *> & virtualWires, QUndoCommand * parentCommand)
{
	// TODO: if pcbview, clear autorouting

	if (m_viewIdentifier != ItemBase::BreadboardView) {
		return;
	}

	// if item is attached to a virtual wire or a female connector in breadboard view
	// then disconnect it
	// at the moment, I think this doesn't apply to other views

	foreach (QGraphicsItem * childItem, item->childItems()) {
		ConnectorItem * fromConnectorItem = dynamic_cast<ConnectorItem *>(childItem);
		if (fromConnectorItem == NULL) continue;

		foreach (ConnectorItem * toConnectorItem, fromConnectorItem->connectedToItems())  {
			if (toConnectorItem->connectorType() == Connector::Female) {
				if (savedItems.contains(toConnectorItem->attachedTo())) {
					// the thing we're connected to is also moving, so don't disconnect
					continue;
				}
				extendChangeConnectionCommand(fromConnectorItem, toConnectorItem, false, true, parentCommand);
				fromConnectorItem->tempRemove(toConnectorItem);
				toConnectorItem->tempRemove(fromConnectorItem);
			}
			else if (toConnectorItem->attachedTo()->getVirtual()) {
				VirtualWire * virtualWire = dynamic_cast<VirtualWire *>(toConnectorItem->attachedTo());
				ConnectorItem * realci = virtualWire->otherConnector(toConnectorItem)->firstConnectedToIsh();
				if (realci != NULL) {
					ItemBase * otherEnd = realci->attachedTo();
					if (savedItems.contains(otherEnd)) {
						// the thing we're connected to is also moving, so don't disconnect
						continue;
					}
				}
				else {
					DebugDialog::debug("why is this only connected to a virtual item?");
				}
				virtualWires.insert(virtualWire);
			}
		}
	}
}

void SketchWidget::cleanUpVirtualWires(QSet<VirtualWire *> & virtualWires, QList<BusConnectorItem *> & busConnectorItems, QUndoCommand * parentCommand) {
	disconnectVirtualWires(virtualWires, parentCommand);
	reorgBuses(busConnectorItems, parentCommand);
	deleteVirtualWires(virtualWires, parentCommand);
}
