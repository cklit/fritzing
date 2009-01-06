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


#ifndef REFERENCEMODEL_H_
#define REFERENCEMODEL_H_

#include "../palettemodel.h"
#include "daos.h"

class ReferenceModel : public PaletteModel {
	Q_OBJECT
	public:
		virtual ModelPart *loadPart(const QString & path, bool update) = 0;

		virtual ModelPart *retrieveModelPart(const QString &moduleID) = 0;
		virtual ModelPart *retrieveModelPart(const QString &family, const QMultiHash<QString /*name*/, QString /*value*/> &properties) = 0;
		virtual ModelPart *retrieveModelPart(const Part *examplePart) = 0;
		virtual QString retrieveModuleId(const Part *examplePart) = 0;

		virtual bool addPart(ModelPart * newModel, bool update) = 0;
		virtual bool updatePart(ModelPart * newModel) = 0;
		virtual bool addPart(Part* part) = 0;

		virtual bool swapEnabled() = 0;
		virtual QString partTitle(const QString moduleID) = 0;

	public slots:
		virtual void recordProperty(const QString &name, const QString &value) = 0;
		virtual QString retrieveModuleIdWithRecordedProps(const QString &family) = 0;
		virtual QString retrieveModuleId(const QString &family, const QMultiHash<QString /*name*/, QString /*value*/> &properties) = 0;
		virtual QStringList values(const QString &family, const QString &propName, bool distinct=true) = 0;
};

#endif /* REFERENCEMODEL_H_ */
