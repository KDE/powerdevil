/*
 *   SPDX-FileCopyrightText: 2010 Sebastian Kugler <sebas@kde.org>
 *
 *   SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "actionconfigwidget.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLayoutItem>
#include <QMenu>
#include <QMenuBar>

ActionConfigWidget::ActionConfigWidget(QWidget *parent)
    : QWidget(parent)
{
    m_gridLayout = new QGridLayout(this);
    setLayout(m_gridLayout);

    // FIXME: pixelSize() returns an useful -1 here, pointSize() doesn't
    // that's correct though, since points != pixels, and the latter matter
    m_gridLayout->setVerticalSpacing(QApplication::font().pointSize() / 2);
}

ActionConfigWidget::~ActionConfigWidget()
{
}

void ActionConfigWidget::addWidgets(const QList<QPair<QString, QWidget *>> &configMap)
{
    int row = m_gridLayout->rowCount();
    row++;

    QCheckBox *currentSectionCheckbox = nullptr;

    QList<QPair<QString, QWidget *>>::const_iterator it;
    for (it = configMap.constBegin(); it != configMap.constEnd(); ++it) {
        QPair<QString, QWidget *> line = *it;
        if (line.first.isEmpty()) {
            // A title checkbox
            currentSectionCheckbox = qobject_cast<QCheckBox *>(line.second);
            currentSectionCheckbox->setChecked(true);
            m_gridLayout->addWidget(line.second, row, 0, 1, 3);

            // allow left-aligning checkboxes without treating them as section header
        } else if (line.first == QLatin1String("NONE")) {
            m_gridLayout->addItem(new QSpacerItem(50, 3), row, 0);
            m_gridLayout->addWidget(line.second, row, 1, 1, 2);
        } else {
            // connect enabled / disabled
            QLabel *label = new QLabel(this);
            label->setText(line.first);

            m_gridLayout->addItem(new QSpacerItem(50, 3), row, 0);
            m_gridLayout->addWidget(label, row, 1, Qt::AlignRight);
            m_gridLayout->addWidget(line.second, row, 2);

            connect(currentSectionCheckbox, &QAbstractButton::toggled, label, &QWidget::setEnabled);
            connect(currentSectionCheckbox, &QAbstractButton::toggled, line.second, &QWidget::setEnabled);
        }
        row++;
    }
}

#include "moc_actionconfigwidget.cpp"
