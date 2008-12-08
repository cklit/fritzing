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

$Revision: 1617 $:
$Author: cohen@irascible.com $:
$Date: 2008-11-22 20:32:44 +0100 (Sat, 22 Nov 2008) $

********************************************************************/

#include "schematicsketchwidget.h"
#include "autorouter1.h"
#include "debugdialog.h"
#include "virtualwire.h"

static const QString ___viewName___ = QObject::tr("Schematic View");

SchematicSketchWidget::SchematicSketchWidget(ItemBase::ViewIdentifier viewIdentifier, QWidget *parent, int size, int minSize)
    : PCBSchematicSketchWidget(viewIdentifier, parent, size, minSize)
{
}

void SchematicSketchWidget::cleanUpWire(Wire * wire, QList<Wire *> & wires)
{
	Q_UNUSED(wires);
	wire->setVisible(wire->getRatsnest());
}

void SchematicSketchWidget::addViewLayers() {
	addSchematicViewLayers();
}

void SchematicSketchWidget::updateRatsnestStatus() {
	QHash<ConnectorItem *, int> indexer;
	QList< QList<ConnectorItem *>* > allPartConnectorItems;
	Autorouter1::collectAllNets(this, indexer, allPartConnectorItems);
	removeRatsnestWires(allPartConnectorItems);
	foreach (QList<ConnectorItem *>* list, allPartConnectorItems) {
		delete list;
	}
}


void SchematicSketchWidget::dealWithRatsnest(ConnectorItem * fromConnectorItem, ConnectorItem * toConnectorItem, bool connect) 
{
	if (alreadyRatsnest(fromConnectorItem, toConnectorItem)) return;

	if (connect) {
		bool useFrom = false;
		bool useTo = false;
		if ((fromConnectorItem->attachedToItemType() == ModelPart::Part) ||
			(fromConnectorItem->attachedToItemType() == ModelPart::Board)) 
		{
			useFrom = true;
		}
		if ((toConnectorItem->attachedToItemType() == ModelPart::Part) ||
			(toConnectorItem->attachedToItemType() == ModelPart::Board)) 
		{
			useTo = true;
		}
		
		if (useFrom && useTo) {
			makeOneRatsnestWire(fromConnectorItem, toConnectorItem);
			return;
		}

		if (useFrom && toConnectorItem->attachedToItemType() == ModelPart::Wire) {
			ConnectorItem * newTo = tryWire(toConnectorItem, fromConnectorItem);
			if (newTo != NULL) {
				makeOneRatsnestWire(fromConnectorItem, newTo);
				return;
			}
		}
		else if (useTo && fromConnectorItem->attachedToItemType() == ModelPart::Wire) {
			ConnectorItem * newFrom = tryWire(fromConnectorItem, toConnectorItem);
			if (newFrom != NULL) {
				makeOneRatsnestWire(toConnectorItem, newFrom);
				return;
			}
		}

		QList<ConnectorItem *> connectorItems;
		connectorItems << fromConnectorItem, toConnectorItem;
		ConnectorItem::collectEqualPotential(connectorItems);
		QList<ConnectorItem *> partsConnectorItems;
		ConnectorItem::collectParts(connectorItems, partsConnectorItems);
		if (useFrom) {
			ConnectorItem * newTo = tryParts(fromConnectorItem, partsConnectorItems);
			if (newTo != NULL) {
				makeOneRatsnestWire(fromConnectorItem, newTo);
				return;
			}
		}
		else if (useTo) {
			ConnectorItem * newFrom = tryParts(toConnectorItem, partsConnectorItems);
			if (newFrom != NULL) {
				makeOneRatsnestWire(toConnectorItem, newFrom);
				return;
			}
		}
		else {
			for (int i = 0; i < partsConnectorItems.count() - 1; i++) {
				ConnectorItem * ci = partsConnectorItems[i];
				for (int j = i + 1; j < partsConnectorItems.count(); j++) {
					ConnectorItem * cj = partsConnectorItems[j];
					if (ci->bus() != NULL && ci->bus() == cj->bus()) continue;
					if (ci->wiredTo(cj, ViewGeometry::RatsnestFlag)) continue;
					
					if (alreadyOnBus(ci, cj)) continue;
					if (alreadyOnBus(cj, ci)) continue;

					makeOneRatsnestWire(ci, cj);
					return;
				}
			}

			return;
		}
	}
}

bool SchematicSketchWidget::alreadyOnBus(ConnectorItem * busCandidate, ConnectorItem * otherCandidate) {
	if (busCandidate->bus() == NULL) return false;

	foreach (ConnectorItem * toConnectorItem, otherCandidate->connectedToItems()) {
		if (toConnectorItem->bus() == busCandidate->bus()) return true;
	}

	return false;
}

ConnectorItem * SchematicSketchWidget::tryParts(ConnectorItem * otherConnectorItem, QList<ConnectorItem *> partsConnectorItems) 
{
	foreach (ConnectorItem * connectorItem, partsConnectorItems) {
		if (connectorItem == otherConnectorItem) continue;
		if (connectorItem->attachedTo() == otherConnectorItem->attachedTo() &&
			connectorItem->bus() != NULL &&
			connectorItem->bus() == otherConnectorItem->bus()) continue;
		if (alreadyOnBus(otherConnectorItem, connectorItem)) continue;
		if (alreadyOnBus(connectorItem, otherConnectorItem)) continue;


		return connectorItem;
	}

	return NULL;

}

ConnectorItem * SchematicSketchWidget::tryWire(ConnectorItem * wireConnectorItem, ConnectorItem * otherConnectorItem)
{
	QList<Wire *> chained;
	QList<ConnectorItem *> ends;
	QList<ConnectorItem *> uniqueEnds;
	dynamic_cast<Wire *>(wireConnectorItem->attachedTo())->collectChained(chained, ends, uniqueEnds);
	foreach (ConnectorItem * end, ends) {
		if (end == wireConnectorItem) continue;
		if (end == otherConnectorItem) continue;
		if (end->attachedToItemType() == ModelPart::Breadboard) continue;
		if (end->attachedTo() == otherConnectorItem->attachedTo() &&
			end->bus() != NULL &&
			end->bus() == otherConnectorItem->bus()) continue;
		if (alreadyOnBus(otherConnectorItem, end)) continue;
		if (alreadyOnBus(end, otherConnectorItem)) continue;


		return end;
	}

	return NULL;
}

void SchematicSketchWidget::makeWires(QList<ConnectorItem *> & partsConnectorItems, QList <Wire *> & ratsnestWires, Wire * & modelWire)
{
	Q_UNUSED(partsConnectorItems);
	Q_UNUSED(modelWire);
	Q_UNUSED(ratsnestWires);

	return;

	/*
	modelWire = NULL;

	// delete all the ratsnest wires before running dijkstra
	int count = partsConnectorItems.count();
	for (int i = 0; i < count - 1; i++) {
		ConnectorItem * source = partsConnectorItems[i];
		for (int j = i + 1; j < count; j++) {
			ConnectorItem * dest = partsConnectorItems[j];
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

	Autorouter1::dijkstra(partsConnectorItems, indexer, adjacency, ViewGeometry::NormalFlag);

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
*/
}

bool SchematicSketchWidget::canDeleteItem(QGraphicsItem * item) 
{
	VirtualWire * wire = dynamic_cast<VirtualWire *>(item);
	if (wire != NULL) return true;

	return SketchWidget::canDeleteItem(item);
}

bool SchematicSketchWidget::canCopyItem(QGraphicsItem * item) 
{
	VirtualWire * wire = dynamic_cast<VirtualWire *>(item);
	if (wire != NULL) return false;

	return SketchWidget::canDeleteItem(item);
}

void SchematicSketchWidget::reviewDeletedConnections(QList<ItemBase *> & deletedItems, QHash<ItemBase *, ConnectorPairHash *> & deletedConnections, QUndoCommand * parentCommand)
{
	// don't forget to take the virtualwires out of the list:
	PCBSchematicSketchWidget::reviewDeletedConnections(deletedItems, deletedConnections, parentCommand);

	QSet<Wire *> deleteWires;
	QSet<Wire *> undeleteWires;
	ConnectorPairHash moveItems;
	foreach (ItemBase * itemBase, deletedItems) {
		Wire * wire = dynamic_cast<Wire *>(itemBase);
		if (wire == NULL) continue;
		if (!wire->getRatsnest()) continue;

		undeleteWires.insert(wire);
		QList<Wire *> chained;
		QList<ConnectorItem *> ends;
		QList<ConnectorItem *> uniqueEnds;
		wire->collectChained(chained, ends, uniqueEnds);
		if (ends.count() == 2) {
			Wire* extraWire = ends[0]->wiredTo(ends[1], ViewGeometry::NormalFlag);
			if (extraWire != NULL) {
				// need to delete the real wire from breadboard view
				deleteWires.insert(extraWire);
			}
			else { 	
				// ensure we're not cross-disconnecting
				// and index by male connectors
				if (moveItems.uniqueKeys().contains(ends[0])) {
					moveItems.insert(ends[0], ends[1]);
				}
				else if (moveItems.uniqueKeys().contains(ends[1])) {
					moveItems.insert(ends[1], ends[0]);
				}
				else if (ends[0]->connectorType() == Connector::Female) {
					moveItems.insert(ends[1], ends[0]);
				}
				else if (ends[1]->connectorType() == Connector::Female) {
					moveItems.insert(ends[0], ends[1]);
				}	
				else {
					moveItems.insert(ends[0], ends[1]);
				}
			}		
		}
		else {
			// "isolate" the wire to determine a minimal number of parts
			// then figure out which parts to move/disconnect
		}
	}

	if (moveItems.count() > 0) {
		emit schematicDisconnectWireSignal(moveItems, deletedItems, parentCommand);
	}

	foreach (Wire * wire, undeleteWires) {
		deletedItems.removeOne(wire);
	}

	foreach (Wire * wire, deleteWires) {
		if (!deletedItems.contains(wire)) {
			deletedItems.append(wire);
		}
	}
}

bool SchematicSketchWidget::canChainMultiple() {
	return true;
}


const QString & SchematicSketchWidget::viewName() {
	return ___viewName___;
}


void SchematicSketchWidget::modifyNewWireConnections(ConnectorItem * & from, ConnectorItem * & to) 
{
	if (from->attachedToItemType() == ModelPart::Wire) {
		from = lookForBreadboardConnection(from);
	}
	else if (to->attachedToItemType() == ModelPart::Wire) {
		to = lookForBreadboardConnection(to);
	}
}


ConnectorItem * SchematicSketchWidget::lookForBreadboardConnection(ConnectorItem * & connectorItem) {
	Wire * wire = dynamic_cast<Wire *>(connectorItem->attachedTo());

	QList<ConnectorItem *> ends;
	QList<ConnectorItem *> uniqueEnds;
	QList<Wire *> wires;
	wire->collectChained(wires, ends, uniqueEnds);
	foreach (ConnectorItem * end, ends) {
		foreach (ConnectorItem * toConnectorItem, end->connectedToItems()) {
			if (toConnectorItem->attachedToItemType() == ModelPart::Breadboard) {
				return toConnectorItem;
			}
		}
	}

	ends.clear();
	ends.append(connectorItem);
	ConnectorItem::collectEqualPotential(ends);
	foreach (ConnectorItem * end, ends) {
		if (end->attachedToItemType() == ModelPart::Breadboard) {
			return end;
		}
	}

	return connectorItem;
}
