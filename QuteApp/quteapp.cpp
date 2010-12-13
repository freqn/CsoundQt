/*
    Copyright (C) 2010 Andres Cabrera
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

#include <QtGui>
#include "quteapp.h"
#include "types.h"
#include "csoundoptions.h"
#include "simpledocument.h"
#include "opentryparser.h"
#include "widgetlayout.h"
#include "console.h"

QuteApp::QuteApp(QWidget *parent)
    : QMainWindow(parent)
{
  QDir::setCurrent("/data");
  m_options = new CsoundOptions;
  m_options->fileName1 = "data/quteapp.csd";
  OpEntryParser *m_opcodeTree = new OpEntryParser(":/main/opcodes.xml");
  m_doc = new SimpleDocument(this, m_opcodeTree);
  m_console = m_doc->getConsole();
  m_console->setWindowTitle(tr("Console"));
  m_console->setWindowFlags(Qt::Window);
  m_console->show();
  createMenus();
  setCentralWidget((QWidget *) m_doc->getWidgetLayout());
  loadCsd();
//  start();
}

QuteApp::~QuteApp()
{
  delete m_options;
  delete m_opcodeTree;
}

void QuteApp::createMenus()
{
  QMenu *menu = menuBar()->addMenu(tr("&File"));
  menu->addAction("Run", this, SLOT(start()), QKeySequence("Ctrl+R"));
  menu->addAction("Stop", this, SLOT(stop()), QKeySequence("Ctrl+."));
  menu->addAction("Show Console Output", this, SLOT(showConsole()));
}

void QuteApp::start()
{
  m_doc->play(m_options);
}

void QuteApp::pause()
{
  m_doc->pause();
}

void QuteApp::stop()
{
  m_doc->stop();
}

void QuteApp::save()
{
  qDebug() << "QuteApp::save() not implemented yet.";
}

void QuteApp::showConsole()
{
  m_console->setVisible(!m_console->isVisible());
}

bool QuteApp::loadCsd()
{
  QString fileName = m_options->fileName1;
  QFile file(fileName);
  if (!file.open(QFile::ReadOnly)) {
    QMessageBox::warning(this, tr("QuteCsound"),
                         tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
    return false;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);

  QString text;
  while (!file.atEnd()) {
    QByteArray line = file.readLine();
    while (line.contains("\r\n")) {
      line.replace("\r\n", "\n");  //Change Win returns to line endings
    }
    while (line.contains("\r")) {
      line.replace("\r", "\n");  //Change Mac returns to line endings
    }
    QTextDecoder decoder(QTextCodec::codecForLocale());
    text = text + decoder.toUnicode(line);
    if (!line.endsWith("\n"))
      text += "\n";
  }

//  m_doc->setInitialDir(initialDir);
//  m_doc->showLiveEventPanels(false);

//  setCurrentOptionsForPage(m_doc);
//  setCurrentFile(fileName);

//  m_doc->setFileName(fileName);  // Must set before sending text to set highlighting mode

//  connectActions();
//  connect(documentPages[curPage], SIGNAL(currentTextUpdated()), this, SLOT(markInspectorUpdate()));
//  connect(documentPages[curPage], SIGNAL(modified()), this, SLOT(documentWasModified()));
//  connect(documentPages[curPage], SIGNAL(currentLineChanged(int)), this, SLOT(showLineNumber(int)));
//  connect(documentPages[curPage], SIGNAL(setWidgetClipboardSignal(QString)),
//          this, SLOT(setWidgetClipboard(QString)));
//  connect(documentPages[curPage], SIGNAL(setCurrentAudioFile(QString)),
//          this, SLOT(setCurrentAudioFile(QString)));
//  connect(documentPages[curPage]->getView(), SIGNAL(lineNumberSignal(int)),
//          this, SLOT(showLineNumber(int)));
//  connect(documentPages[curPage], SIGNAL(evaluatePythonSignal(QString)),
//          this, SLOT(evaluatePython(QString)));

  m_doc->setTextString(text);

//  m_doc->readOnly = true;
  QApplication::restoreOverrideCursor();
//  setWidgetPanelGeometry();

//  if (runNow && m_options->autoPlay) {
//    play();
//  }
  this->resize(m_doc->getWidgetLayout()->size());
  return true;
}
