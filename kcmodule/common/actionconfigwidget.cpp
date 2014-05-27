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
#include <QLayoutItem>

ActionConfigWidget::ActionConfigWidget(QWidget* parent) : QWidget(parent)
{
    m_gridLayout = new QGridLayout(this);
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
            currentSectionCheckbox->setStyleSheet("font-weight: bold;");
            m_gridLayout->addWidget(line.second, row, 0, 1, 3);
        } else {
            // connect enabled / disabled
            QLabel* label = new QLabel(this);
            label->setText(line.first);

            m_gridLayout->addItem(new QSpacerItem(50 ,3), row, 0);
            m_gridLayout->addWidget(label, row, 1, Qt::AlignRight);
            m_gridLayout->addWidget(line.second, row, 2);

            connect(currentSectionCheckbox, SIGNAL(toggled(bool)),
                    label, SLOT(setEnabled(bool)));
            connect(currentSectionCheckbox, SIGNAL(toggled(bool)),
                    line.second, SLOT(setEnabled(bool)));
        }
        row++;
    }
}

#include "actionconfigwidget.moc"
