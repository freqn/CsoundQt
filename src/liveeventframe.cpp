/*
    Copyright (C) 2009 Andres Cabrera
    mantaraya36@gmail.com

    This file is part of QuteCsound.

    QuteCsound is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    QuteCsound is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with Csound; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307 USA
*/

#include "liveeventframe.h"
#include "eventsheet.h"

#include <QTextEdit>
#include <QResizeEvent>

LiveEventFrame::LiveEventFrame(QString csdName, QWidget *parent, Qt::WindowFlags f) :
    QFrame(parent, f), m_csdName(csdName)
{
  setupUi(this);

  setWindowTitle(m_csdName);
  m_sheet = new EventSheet(this);
  m_sheet->show();
  m_sheet->setTempo(60.0);
  scrollArea->setWidget(m_sheet);

  m_editor = new QTextEdit(this);
  m_editor->hide();

  connect(tempoSpinBox,SIGNAL(valueChanged(double)), m_sheet,SLOT(setTempo(double)));
}

EventSheet * LiveEventFrame::getSheet()
{
  return m_sheet;
}

void LiveEventFrame::setTempo(double tempo)
{
  tempoSpinBox->setValue(tempo);
}

void LiveEventFrame::changeEvent(QEvent *e)
{
    QFrame::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

void LiveEventFrame::resizeEvent (QResizeEvent * event)
{
  QSize s = event->size();
  s.setHeight(s.height() - 30);
  scrollArea->resize(s);
}

void LiveEventFrame::closeEvent (QCloseEvent * event)
{
  emit closed();
}