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

#include "modelbase.h"
#include "debugdialog.h"

#include <QMessageBox>

ModelBase::ModelBase( bool makeRoot )
{
	m_referenceModel = NULL;
	m_root = (makeRoot) ? new ModelPart() : NULL;
}

ModelBase::~ModelBase() {
	// seems to get rid of a bunch of compiler warnings
}


ModelPart * ModelBase::root() {
	return m_root;
}

ModelPart * ModelBase::retrieveModelPart(const QString & /* moduleID */)  {
	return NULL;
}

// loads a model from an fz file--assumes a reference model exists with all parts
bool ModelBase::load(const QString & fileName, ModelBase * refModel, QList<ModelPart *> & modelParts) {
	m_referenceModel = refModel;

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(NULL, QObject::tr("Fritzing"),
                             QObject::tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }

    QString errorStr;
    int errorLine;
    int errorColumn;
    QDomDocument* domDocument = new QDomDocument();

    if (!domDocument->setContent(&file, true, &errorStr, &errorLine, &errorColumn)) {
        QMessageBox::information(NULL, QObject::tr("Fritzing"),
                                 QObject::tr("Parse error (1) at line %1, column %2:\n%3\n%4")
                                 .arg(errorLine)
                                 .arg(errorColumn)
                                 .arg(errorStr)
								 .arg(fileName));
        return false;
    }

    QDomElement root = domDocument->documentElement();
   	if (root.isNull()) {
        QMessageBox::information(NULL, QObject::tr("Fritzing"), QObject::tr("The file %1 is not a Fritzing file (2).").arg(fileName));
   		return false;
	}

    if (root.tagName() != "module") {
        QMessageBox::information(NULL, QObject::tr("Fritzing"), QObject::tr("The file %1 is not a Fritzing file.").arg(fileName));
        return false;
    }

    QDomElement title = root.firstChildElement("title");
	if (!title.isNull()) {
		this->root()->modelPartStuff()->setTitle(title.text());
	}

	QDomElement instances = root.firstChildElement("instances");
	if (instances.isNull()) {
        QMessageBox::information(NULL, QObject::tr("Fritzing"), QObject::tr("The file %1 is not a Fritzing file (3).").arg(fileName));
        return false;
	}

	// delete any aready-existing model parts
    for (int i = m_root->children().count() - 1; i >= 0; i--) {
    	QObject* child = m_root->children()[i];
    	child->setParent(NULL);
    	delete child;
   	}

	return loadInstances(instances, modelParts);

}

bool ModelBase::loadInstances(QDomElement & instances, QList<ModelPart *> & modelParts) 
{
   	QHash<long, ModelPart *> partHash;
   	QDomElement instance = instances.firstChildElement("instance");
   	ModelPart* modelPart = NULL;
   	while (!instance.isNull()) {
   		// for now assume all parts are in the palette
   		QString moduleIDRef = instance.attribute("moduleIdRef");
   		modelPart = m_referenceModel->retrieveModelPart(moduleIDRef);
   		if (modelPart == NULL) {
   			instance = instance.nextSiblingElement("instance");
   			continue;
   		}

   		modelPart = addModelPart(m_root, modelPart);
   		modelPart->setInstanceDomElement(instance);
		modelParts.append(modelPart);

   		// TODO Mariano: i think this is not the way
   		QString instanceTitle = instance.firstChildElement("title").text();
   		if(!instanceTitle.isNull() && !instanceTitle.isEmpty()) {
   			if(!modelPart->partInstanceStuff()) modelPart->setPartInstanceStuff(new PartInstanceStuff());
   			modelPart->partInstanceStuff()->setTitle(instanceTitle);
   		}

   		bool ok;
   		long index = instance.attribute("modelIndex").toLong(&ok);
   		if (ok) {
   			partHash.insert(index, modelPart);

			// set the index so we can find the same model part later, as we continue loading
			modelPart->setModelIndex(index);
  		}

   		instance = instance.nextSiblingElement("instance");
  	}

	return true;
}



ModelPart * ModelBase::addModelPart(ModelPart * parent, ModelPart * copyChild) {
	ModelPart * modelPart = new ModelPart();
	modelPart->copyNew(copyChild);
	modelPart->setParent(parent);
	modelPart->initConnectors();
	return modelPart;
}

ModelPart * ModelBase::addPart(QString newPartPath, bool addToReference) {
	Q_UNUSED(newPartPath);
	Q_UNUSED(addToReference);
	Q_ASSERT(false);
	return NULL;
}

// TODO Mariano: this function should never get called. Make pure virtual
bool ModelBase::addPart(ModelPart * modelPart, bool update) {
        Q_UNUSED(modelPart);
        Q_UNUSED(update);
        Q_ASSERT(false);
	return false;
}


void ModelBase::save(const QString & fileName, bool asPart) {
	QDir dir(fileName);
	dir.cdUp();

	QString temp = dir.absolutePath() + "/" + "part_temp.xml";
    QFile file1(temp);
    if (!file1.open(QFile::WriteOnly | QFile::Text)) {
        QMessageBox::warning(NULL, QObject::tr("Fritzing"),
                             QObject::tr("Cannot write file temp file:\n%1.")
                              .arg(file1.errorString()));
        return;
    }

    QXmlStreamWriter streamWriter(&file1);
    streamWriter.setAutoFormatting(true);

    if(asPart) {
    	m_root->saveAsPart(streamWriter, true);
    } else {
    	m_root->saveInstances(streamWriter, true);
    }

	file1.close();
	QFile original(fileName);
	original.remove();
	file1.rename(fileName);
}

bool ModelBase::paste(ModelBase * refModel, QByteArray & data, QList<ModelPart *> & modelParts) 
{
	m_referenceModel = refModel;

	QDomDocument domDocument;
	QString errorStr;
	int errorLine;
	int errorColumn;
	bool result = domDocument.setContent(data, &errorStr, &errorLine, &errorColumn);
	if (!result) return false;

	QDomElement module = domDocument.documentElement();
	if (module.isNull()) {
		return false;
	}

	QDomElement instances = module.firstChildElement("instances");
   	if (instances.isNull()) {
   		return false;
	}

	// need to map modelIndexes from copied parts to new modelIndexes
	QHash<long, long> oldToNew;
	QDomElement instance = instances.firstChildElement("instance");
	while (!instance.isNull()) {
		long oldModelIndex = instance.attribute("modelIndex").toLong();
		oldToNew.insert(oldModelIndex, ModelPart::nextIndex());
		instance = instance.nextSiblingElement("instance");
	}
	renewModelIndexes(instances, oldToNew);

	//QFile file("test.xml");
	//file.open(QFile::WriteOnly);
	//file.write(domDocument.toByteArray());
	//file.close();

	return loadInstances(instances, modelParts);
}

void ModelBase::renewModelIndexes(QDomElement & instances, QHash<long, long> & oldToNew) 
{
	QDomElement instance = instances.firstChildElement("instance");
	while (!instance.isNull()) {
		long oldModelIndex = instance.attribute("modelIndex").toLong();
		instance.setAttribute("modelIndex", QString::number(oldToNew.value(oldModelIndex)));
		QDomElement views = instance.firstChildElement("views");
		if (!views.isNull()) {
			QDomElement view = views.firstChildElement();
			while (!view.isNull()) {
				QDomElement connectors = view.firstChildElement("connectors");
				if (!connectors.isNull()) {
					QDomElement connector = connectors.firstChildElement("connector");
					while (!connector.isNull()) {
						QDomElement connects = connector.firstChildElement("connects");
						if (!connects.isNull()) {
							QDomElement connect = connects.firstChildElement("connect");
							while (!connect.isNull()) {
								oldModelIndex = connect.attribute("modelIndex").toLong();
								connect.setAttribute("modelIndex", QString::number(oldToNew.value(oldModelIndex)));
								connect = connect.nextSiblingElement("connect");
							}
						}
						connector = connector.nextSiblingElement("connector");
					}
				}

				view = view.nextSiblingElement();
			}
		}

		instance = instance.nextSiblingElement("instance");
	}
}