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

#include "graphicsutils.h"
#include "../fsvgrenderer.h"

#include <QList>
#include <QLineF>
#include <qmath.h>


const qreal GraphicsUtils::InchesPerMeter = 39.370078;


void GraphicsUtils::distanceFromLine(double cx, double cy, double ax, double ay, double bx, double by, 
									 double & dx, double & dy, double &distanceSegment, bool & atEndpoint)
{

	// http://www.codeguru.com/forum/showthread.php?t=194400

	//
	// find the distance from the point (cx,cy) to the line
	// determined by the points (ax,ay) and (bx,by)
	//

	double r_numerator = (cx-ax)*(bx-ax) + (cy-ay)*(by-ay);
	double r_denomenator = (bx-ax)*(bx-ax) + (by-ay)*(by-ay);
	double r = r_numerator / r_denomenator;
     
	if ( (r >= 0) && (r <= 1) )
	{
		dx = ax + r*(bx-ax);
		dy = ay + r*(by-ay);
		distanceSegment = (cx-dx)*(cx-dx) + (cy-dy)*(cy-dy);
		atEndpoint = false;
	}
	else
	{
		atEndpoint = true;
		double dist1 = (cx-ax)*(cx-ax) + (cy-ay)*(cy-ay);
		double dist2 = (cx-bx)*(cx-bx) + (cy-by)*(cy-by);
		if (dist1 < dist2)
		{
			dx = ax;
			dy = ay;
			distanceSegment = dist1;
		}
		else
		{
			dx = bx;
			dy = by;
			distanceSegment = dist2;
		}
	}

	return;
}

struct PD {
	QPointF p;
	qreal d;
};

bool pdLessThan(PD* pd1, PD* pd2) {
	return pd1->d < pd2->d;
}

QPointF GraphicsUtils::calcConstraint(QPointF initial, QPointF current) {
	QList<PD *> pds;

	PD * pd = new PD;
	pd->p.setX(current.x());
	pd->p.setY(initial.y());
	pd->d = (current.y() - initial.y()) * (current.y() - initial.y());
	pds.append(pd);

	pd = new PD;
	pd->p.setX(initial.x());
	pd->p.setY(current.y());
	pd->d = (current.x() - initial.x()) * (current.x() - initial.x());
	pds.append(pd);

	qreal dx, dy, d;
	bool atEndpoint;

	QLineF plus45(initial.x() - 10000, initial.y() - 10000, initial.x() + 10000, initial.y() + 10000);
	distanceFromLine(current.x(), current.y(), plus45.p1().x(), plus45.p1().y(), plus45.p2().x(), plus45.p2().y(), dx, dy, d, atEndpoint);
	pd = new PD;
	pd->p.setX(dx);
	pd->p.setY(dy);
	pd->d = d;
	pds.append(pd);
		
	QLineF minus45(initial.x() + 10000, initial.y() - 10000, initial.x() - 10000, initial.y() + 10000);
	distanceFromLine(current.x(), current.y(), minus45.p1().x(), minus45.p1().y(), minus45.p2().x(), minus45.p2().y(), dx, dy, d, atEndpoint);
	pd = new PD;
	pd->p.setX(dx);
	pd->p.setY(dy);
	pd->d = d;
	pds.append(pd);

	qSort(pds.begin(), pds.end(), pdLessThan);
	QPointF result = pds[0]->p;
	foreach (PD* pd, pds) {
		delete pd;
	}
	return result;
}

qreal GraphicsUtils::pixels2mils(qreal p) {
	return p * 1000.0 / FSvgRenderer::printerScale();
}

qreal GraphicsUtils::pixels2ins(qreal p) {
	return p / FSvgRenderer::printerScale();
}

qreal GraphicsUtils::distance2(QPointF p1, QPointF p2) {
	return ((p1.x() - p2.x()) * (p1.x() - p2.x())) + ((p1.y() - p2.y()) * (p1.y() - p2.y()));
}


qreal GraphicsUtils::mm2mils(qreal mm) {
	return (mm / 25.4 * 1000);
}

qreal GraphicsUtils::pixels2mm(qreal p) {
	return (p / FSvgRenderer::printerScale() * 25.4);
}

qreal GraphicsUtils::mils2pixels(qreal m) {
	return (FSvgRenderer::printerScale() * m / 1000);
}

void GraphicsUtils::saveTransform(QXmlStreamWriter & streamWriter, const QTransform & transform) {
	if (transform.isIdentity()) return;

	streamWriter.writeStartElement("transform");
	streamWriter.writeAttribute("m11", QString::number(transform.m11()));
	streamWriter.writeAttribute("m12", QString::number(transform.m12()));
	streamWriter.writeAttribute("m13", QString::number(transform.m13()));
	streamWriter.writeAttribute("m21", QString::number(transform.m21()));
	streamWriter.writeAttribute("m22", QString::number(transform.m22()));
	streamWriter.writeAttribute("m23", QString::number(transform.m23()));
	streamWriter.writeAttribute("m31", QString::number(transform.m31()));
	streamWriter.writeAttribute("m32", QString::number(transform.m32()));
	streamWriter.writeAttribute("m33", QString::number(transform.m33()));
	streamWriter.writeEndElement();
}

bool GraphicsUtils::loadTransform(const QDomElement & transformElement, QTransform & transform)
{
	if (transformElement.isNull()) return false;

	qreal m11 = transform.m11();
	qreal m12 = transform.m12();
	qreal m13 = transform.m13();
	qreal m21 = transform.m21();
	qreal m22 = transform.m22();
	qreal m23 = transform.m23();
	qreal m31 = transform.m31();
	qreal m32 = transform.m32();
	qreal m33 = transform.m33();
	bool ok;
	qreal temp;

	temp = transformElement.attribute("m11").toDouble(&ok);
	if (ok) m11 = temp;
	temp = transformElement.attribute("m12").toDouble(&ok);
	if (ok) m12 = temp;
	temp = transformElement.attribute("m13").toDouble(&ok);
	if (ok) m13 = temp;
	temp = transformElement.attribute("m21").toDouble(&ok);
	if (ok) m21 = temp;
	temp = transformElement.attribute("m22").toDouble(&ok);
	if (ok) m22 = temp;
	temp = transformElement.attribute("m23").toDouble(&ok);
	if (ok) m23 = temp;
	temp = transformElement.attribute("m31").toDouble(&ok);
	if (ok) m31 = temp;
	temp = transformElement.attribute("m32").toDouble(&ok);
	if (ok) m32 = temp;
	temp = transformElement.attribute("m33").toDouble(&ok);
	if (ok) m33 = temp;

	transform.setMatrix(m11, m12, m13, m21, m22, m23, m31, m32, m33);
	return true;
}

qreal GraphicsUtils::getNearestOrdinate(qreal ordinate, qreal units) {
	qreal lo = qFloor(ordinate / units) * units;
	qreal hi = qCeil(ordinate / units) * units;
	return (qAbs(lo - ordinate) <= qAbs(hi - ordinate)) ? lo : hi;
}

bool GraphicsUtils::is90(const QMatrix & matrix) {
	if (matrix.m11() == 0) return true;

	if (matrix.m11() == M_PI / 2) return true;
	if (matrix.m11() == -M_PI / 2) return true;
	if (matrix.m11() == 3 * M_PI / 2) return true;

	return false;
}

void GraphicsUtils::shortenLine(QPointF & p1, QPointF & p2, qreal d1, qreal d2) {
	qreal angle1 = atan2(p2.y() - p1.y(), p2.x() - p1.x());
	qreal angle2 = angle1 - M_PI;  
	qreal dx1 = d1 * cos(angle1);
	qreal dy1 = d1 * sin(angle1);
	qreal dx2 = d2 * cos(angle2);
	qreal dy2 = d2 * sin(angle2);
	p1.setX(p1.x() + dx1);
	p1.setY(p1.y() + dy1);
	p2.setX(p2.x() + dx2);
	p2.setY(p2.y() + dy2);
}


// based on code from http://code-heaven.blogspot.com/2009/05/c-program-for-liang-barsky-line.html
bool GraphicsUtils::liangBarskyLineClip(double x1, double y1, double x2, double y2, double wxmin, double wxmax, double wymin, double wymax, 
							double & x11, double & y11, double & x22, double & y22)
{
	qreal p1 = -(x2 - x1 ); 
	qreal q1 = x1 - wxmin;
	qreal p2 = ( x2 - x1 ); 
	qreal q2 = wxmax - x1;
	qreal p3 = - ( y2 - y1 ); 
	qreal q3 = y1 - wymin;
	qreal p4 = ( y2 - y1 ); 
	qreal q4 = wymax - y1;

	x11 = x1;
	y11 = y1;
	x22 = x2;
	y22 = y2;

	if( ( ( p1 == 0.0 ) && ( q1 < 0.0 ) ) ||
		( ( p2 == 0.0 ) && ( q2 < 0.0 ) ) ||
		( ( p3 == 0.0 ) && ( q3 < 0.0 ) ) ||
		( ( p4 == 0.0 ) && ( q4 < 0.0 ) ) )
	{
		return false;
	}

	double u1 = 0.0, u2 = 1.0;

	if( p1 != 0.0 )
	{
		double r1 = q1 /p1 ;
		if( p1 < 0 )
			u1 = qMax(r1, u1 );
		else
			u2 = qMin(r1, u2 );
	}
	if( p2 != 0.0 )
	{
		double r2 = q2 /p2 ;
		if( p2 < 0 )
			u1 = qMax(r2, u1 );
		else
			u2 = qMin(r2, u2 );

	}
	if( p3 != 0.0 )
	{
		double r3 = q3 /p3 ;
		if( p3 < 0 )
			u1 = qMax(r3, u1 );
		else
			u2 = qMin(r3, u2 );
	}
	if( p4 != 0.0 )
	{
		double r4 = q4 /p4 ;
		if( p4 < 0 )
			u1 = qMax(r4, u1 );
		else
			u2 = qMin(r4, u2 );
	}

	if( u1 > u2 ) {
		return false;
	}

	x11 = x1 + u1 * ( x2 - x1 ) ;
	y11 = y1 + u1 * ( y2 - y1 ) ;

	x22 = x1 + u2 * ( x2 - x1 );
	y22 = y1 + u2 * ( y2 - y1 );

	return true;
}

