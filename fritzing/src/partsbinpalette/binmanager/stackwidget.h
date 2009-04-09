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

$Revision: 2776 $:
$Author: merunga $:
$Date: 2009-04-02 13:54:08 +0200 (Thu, 02 Apr 2009) $

********************************************************************/


#ifndef STACKWIDGET_H_
#define STACKWIDGET_H_

#include <QFrame>
#include <QTabWidget>
#include <QTabBar>
#include <QVBoxLayout>

class StackWidgetSeparator;
class StackTabWidget;

#define DragFromOrTo QPair<QWidget* /*tab widget*/, int /*index*/>

class StackWidget : public QFrame {
	Q_OBJECT
	public:
		StackWidget(QWidget *parent=0);

		int addWidget(QWidget *widget);
		int count() const;
		int currentIndex() const;
		QWidget *currentWidget() const;
		int indexOf(QWidget *widget) const;
		void insertWidget(int index, QWidget *widget);
		void removeWidget(QWidget *widget);
		QWidget *widget(int index) const;
		bool contains(QWidget *widget) const;

	public slots:
		void setCurrentIndex(int index);
		void setCurrentWidget(QWidget *widget);

		void setDragSource(StackTabWidget*, int index);
		void setDropReceptor(QWidget* receptor, int index);

	signals:
		void currentChanged(int index);
		void widgetRemoved(int index);

	protected:
		int closestIndexToPos(const QPoint &pos);
		StackWidgetSeparator *newSeparator();

		QVBoxLayout *m_layout;
		QWidget *m_current;

		DragFromOrTo m_dragSource;
		DragFromOrTo m_dropReceptor;
};

#endif /* STACKWIDGET_H_ */
