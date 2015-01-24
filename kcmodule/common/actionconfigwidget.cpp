/***************************************************************************
 *   Copyright (C) 2010 by Sebastian Kugler <sebas@kde.org>                *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
 ***************************************************************************/

#include "actionconfigwidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QApplication>
#include <QLayoutItem>


ActionConfigWidget::ActionConfigWidget(QWidget* parent) : QWidget(parent)
{
    m_gridLayout = new QGridLayout(this);
    setLayout(m_gridLayout);

    // FIXME: pixelSize() returns an useful -1 here, pointSize() doesn't
    // that's correct though, since points != pixels, and the latter matter
    m_gridLayout->setVerticalSpacing(QApplication::font().pointSize() / 2);

}

ActionConfigWidget::~ActionConfigWidget()
{}

void ActionConfigWidget::addWidgets(QList<QPair<QString, QWidget*> > configMap)
{
    int row = m_gridLayout->rowCount();
    row++;

    QCheckBox* currentSectionCheckbox = 0;

    QList<QPair<QString, QWidget*> >::const_iterator it;
    for (it = configMap.constBegin(); it != configMap.constEnd(); ++it) {
        QPair<QString, QWidget*> line = *it;
        if (line.first.isEmpty()) {
            // A title checkbox
            currentSectionCheckbox = qobject_cast<QCheckBox*>(line.second);
            currentSectionCheckbox->setChecked(true);
            m_gridLayout->addWidget(line.second, row, 0, 1, 3);

        // allow left-aligning checkboxes without treating them as section header
        } else if (line.first == QLatin1String("NONE")) {
            m_gridLayout->addItem(new QSpacerItem(50 ,3), row, 0);
            m_gridLayout->addWidget(line.second, row, 1, Qt::AlignRight);
            //m_gridLayout->addWidget(line.second, row, 1, 2, 1, Qt::AlignRight);

        } else {
            // connect enabled / disabled
            QLabel* label = new QLabel(this);
            label->setText(line.first);

            m_gridLayout->addItem(new QSpacerItem(50 ,3), row, 0);
            m_gridLayout->addWidget(label, row, 1, Qt::AlignRight);
            m_gridLayout->addWidget(line.second, row, 2);

            connect(currentSectionCheckbox, &QAbstractButton::toggled,
                    label, &QWidget::setEnabled);
            connect(currentSectionCheckbox, &QAbstractButton::toggled,
                    line.second, &QWidget::setEnabled);
        }
        row++;
    }
}

#include "actionconfigwidget.moc"
