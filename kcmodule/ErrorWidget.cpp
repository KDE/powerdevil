/***************************************************************************
 *   Copyright (C) 2008 by Dario Freddi <drf@kdemod.ath.cx>                *
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

#include "ErrorWidget.h"

#include <KIcon>

ErrorWidget::ErrorWidget(QWidget *parent)
        : QWidget(parent)
{
    setupUi(this);

    warningLabel->setPixmap(KIcon("dialog-warning").pixmap(128, 128));
}

ErrorWidget::~ErrorWidget()
{
}

void ErrorWidget::setError(const QString &error)
{
    detailsLabel->setText(error);
}

QString ErrorWidget::error() const
{
    return detailsLabel->text();
}

#include "ErrorWidget.moc"
