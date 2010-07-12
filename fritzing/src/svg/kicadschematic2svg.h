/*******************************************************************

Part of the Fritzing project - http://fritzing.org
Copyright (c) 2007-2010 Fachhochschule Potsdam - http://fh-potsdam.de

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

#ifndef KICADSCHEMATIC2SVG_H
#define KICADSCHEMATIC2SVG_H

#include <QString>
#include <QStringList>
#include <QTextStream>

#include "kicad2svg.h"

class KicadSchematic2Svg : public Kicad2Svg
{

public:
	KicadSchematic2Svg();
	QString convert(const QString & filename, const QString &defName);

public:
	static QStringList listDefs(const QString & filename);

public:


protected:


};


#endif // KICADSCHEMATIC2SVG_H