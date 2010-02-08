/*
    Copyright (C) 2008, 2009 Andres Cabrera
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

#include "documentpage.h"
#include "documentview.h"
#include "csoundengine.h"
#include "liveeventframe.h"
#include "eventsheet.h"
#include "qutecsound.h" // For playParent and renderParent functions (called from button reserved channels)
#include "opentryparser.h"
#include "types.h"
#include "dotgenerator.h"
#include "highlighter.h"
#include "widgetlayout.h"
#include "console.h"
#include "curve.h"

#include "cwindow.h"
#include <csound.hpp>  // TODO These two are necessary for the WINDAT struct. Can they be moved?


// TODO is is possible to move the editor to a separate child class, to be able to use a cleaner class?
DocumentPage::DocumentPage(QWidget *parent, OpEntryParser *opcodeTree):
    QObject(parent), m_opcodeTree(opcodeTree)
{
  fileName = "";
  companionFile = "";
  askForFile = true;
  readOnly = false;

  //FIXME these options must be set from QuteCsound configuration!
  saveLiveEvents = true;

  m_view = new DocumentView(parent);
  m_view->setOpcodeTree(m_opcodeTree);

  m_console = new ConsoleWidget(parent);
  m_console->setReadOnly(true);

  m_widgetLayout = new WidgetLayout(parent);
  m_widgetLayout->show();

  m_csEngine = new CsoundEngine();
  m_csEngine->setWidgetLayout(m_widgetLayout);  // Pass widget layout to engine

  // Connect for clearing marked lines and letting inspector know text has changed
  connect(m_view, SIGNAL(contentsChanged()), this, SLOT(textChanged()));
  connect(m_view, SIGNAL(opcodeSyntaxSignal(QString)), this, SLOT(opcodeSyntax(QString)));
//   connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(moved()));

  connect(m_console, SIGNAL(keyPressed(QString)),
          m_csEngine, SLOT(keyPressForCsound(QString)));
  connect(m_console, SIGNAL(keyReleased(QString)),
          m_csEngine, SLOT(keyReleaseForCsound(QString)));

  // Key presses on widget layout and console are passed to the engine
  connect(m_widgetLayout, SIGNAL(keyPressed(QString)),
          m_csEngine, SLOT(keyPressForCsound(QString)));
  connect(m_widgetLayout, SIGNAL(keyReleased(QString)),
          m_csEngine, SLOT(keyReleaseForCsound(QString)));
  connect(m_widgetLayout, SIGNAL(changed()), this, SLOT(setModified()));
  connect(m_widgetLayout, SIGNAL(queueEventSignal(QString)),this,SLOT(queueEvent(QString)));
  connect(m_widgetLayout, SIGNAL(setWidgetClipboardSignal(QString)),
          this, SLOT(setWidgetClipboard(QString)));

  connect(m_csEngine, SIGNAL(errorLines(QList<int>)),
          m_view, SLOT(markErrorLines(QList<int>)));
  connect(m_csEngine, SIGNAL(stopSignal()),
          this, SLOT(perfEnded()));

  // Register scopes and graphs to pass them the engine's user data
  connect(m_widgetLayout, SIGNAL(registerScope(QuteScope*)),
          m_csEngine,SLOT(registerScope(QuteScope*)));
  connect(m_widgetLayout, SIGNAL(registerGraph(QuteGraph*)),
          m_csEngine,SLOT(registerGraph(QuteGraph*)));

  // Register the console with the engine for message printing
  m_csEngine->registerConsole(m_console);

  useXml = false; // use Mac widgets
}

DocumentPage::~DocumentPage()
{
//  qDebug() << "DocumentPage::~DocumentPage()";
  disconnect(m_console, 0,0,0);
  disconnect(m_view, 0,0,0);
  disconnect(m_csEngine, 0,0,0);
  disconnect(m_widgetLayout, 0,0,0);
  delete m_view;   // Must be destroyed before the widgetLayout
  delete m_widgetLayout;
  m_csEngine->stop();
  delete m_csEngine;
  for (int i = 0; i < m_liveFrames.size(); i++) {
    deleteLiveEventFrame(m_liveFrames[i]);
  }
}

//void DocumentPage::keyPressEvent(QKeyEvent *event)
//{
//  qDebug() << "DocumentPage::keyPressEvent " << event->key();
//  // TODO is this function necessary any more?
//  if (event == QKeySequence::Cut)
//  {
//    emit doCut();
//    return;
//  }
//  if (event == QKeySequence::Copy)
//  {
//    emit doCopy();
//    return;
//  }
//  if (event == QKeySequence::Paste)
//  {
//    emit doPaste();
//    return;
//  }
////  QTextEdit::keyPressEvent(event);
//}

//void DocumentPage::closeEvent(QCloseEvent *event)
//{
//  qDebug() << "DocumentPage::closeEvent";
//  QTextEdit::closeEvent(event);
//}

int DocumentPage::setTextString(QString text, bool autoCreateMacCsoundSections)
{
  if (text.contains("<MacOptions>") and text.contains("</MacOptions>")) {
    QString options = text.right(text.size()-text.indexOf("<MacOptions>"));
    options.resize(options.indexOf("</MacOptions>") + 13);
    setMacOptionsText(options);
//     qDebug("<MacOptions> present. \n%s", options.toStdString().c_str());
    if (text.indexOf("</MacOptions>") + 13 < text.size() and text[text.indexOf("</MacOptions>") + 13] == '\n')
      text.remove(text.indexOf("</MacOptions>") + 13, 1); //remove final line break
    if (text.indexOf("<MacOptions>") > 0 and text[text.indexOf("<MacOptions>") - 1] == '\n')
      text.remove(text.indexOf("<MacOptions>") - 1, 1); //remove initial line break
    text.remove(text.indexOf("<MacOptions>"), options.size());
//     qDebug("<MacOptions> present. %s", getMacOptions("WindowBounds").toStdString().c_str());
  }
  else {
    if (autoCreateMacCsoundSections) {
      QString defaultMacOptions = "<MacOptions>\nVersion: 3\nRender: Real\nAsk: Yes\nFunctions: ioObject\nListing: Window\nWindowBounds: 72 179 400 200\nCurrentView: io\nIOViewEdit: On\nOptions:\n</MacOptions>\n";
      setMacOptionsText(defaultMacOptions);
    }
    else
      setMacOptionsText("");
  }
  if (text.contains("<MacPresets>") and text.contains("</MacPresets>")) {
    macPresets = text.right(text.size()-text.indexOf("<MacPresets>"));
    macPresets.resize(macPresets.indexOf("</MacPresets>") + 12);
    if (text.indexOf("</MacPresets>") + 12 < text.size() and text[text.indexOf("</MacPresets>") + 12] == '\n')
      text.remove(text.indexOf("</MacPresets>") + 12, 1); //remove final line break
    if (text.indexOf("<MacPresets>") > 0 and text[text.indexOf("<MacPresets>") - 1] == '\n')
      text.remove(text.indexOf("<MacPresets>") - 1, 1); //remove initial line break
    text.remove(text.indexOf("<MacPresets>"), macPresets.size());
    qDebug("<MacPresets> present.");
  }
  else {
    macPresets = "";
  }
  if (text.contains("<MacGUI>") and text.contains("</MacGUI>")) {
    QString macGUI = text.right(text.size()-text.indexOf("<MacGUI>"));
    macGUI.resize(macGUI.indexOf("</MacGUI>") + 9);
    m_widgetLayout->loadWidgets(macGUI);
    if (text.indexOf("</MacGUI>") + 9 < text.size() and text[text.indexOf("</MacGUI>") + 9] == '\n')
      text.remove(text.indexOf("</MacGUI>") + 9, 1); //remove final line break
    if (text.indexOf("<MacGUI>") > 0 and text[text.indexOf("<MacGUI>") - 1] == '\n')
      text.remove(text.indexOf("<MacGUI>") - 1, 1); //remove initial line break
    text.remove(macGUI);
    qDebug("<MacGUI> loaded.");
  }
  else {
    if (autoCreateMacCsoundSections) {
      QString macGUI = "<MacGUI>\nioView nobackground {59352, 11885, 65535}\nioSlider {5, 5} {20, 100} 0.000000 1.000000 0.000000 slider1\n</MacGUI>";
      m_widgetLayout->loadWidgets(macGUI);
    }
  }
  // This here is for compatibility with MacCsound
  QString optionsText = getMacOptions("Options:");
  if (optionsText.contains(" -o")) {
    QString outFile = optionsText.mid(optionsText.indexOf(" -o") + 1);
    int index = outFile.indexOf(" -");
    if (index > 0) {
      outFile = outFile.left(index);
    }
    optionsText.remove(outFile);
    setMacOption("Options:", optionsText);
    index = text.indexOf("<CsOptions>");
    int endindex = text.indexOf("</CsOptions>");
    if (index >= 0 and endindex > index) {
      text.remove(index, endindex - index);
      text.insert(index, "<CsOptions>\n" + outFile);
    }
    else {
      index = text.indexOf("<CsInstruments>");
      text.insert(index, "<CsOptions>\n" + outFile + "\n</CsOptions>\n");
    }
  }
  if (!text.contains("<CsoundSynthesizer>") &&
      !text.contains("</CsoundSynthesizer>") ) { // When not a csd file
    m_view->setFullText(text);  // TODO do something different if not a csd file?
    return 0;  // Don't add live event panel if not a csd file.
  }
  // Load Live Event Panels ------------------------
  while (text.contains("<EventPanel") and text.contains("</EventPanel>")) {
    QString liveEventsText = text.mid(text.indexOf("<EventPanel "),
                                      text.indexOf("</EventPanel>") - text.indexOf("<EventPanel ") + 13);
//    qDebug() << "DocumentPage::setTextString   " << liveEventsText;
    LiveEventFrame *frame = createLiveEventFrame();
    QString scoText = liveEventsText.mid(liveEventsText.indexOf(">") + 1,
                                         liveEventsText.indexOf("</EventPanel>") - liveEventsText.indexOf(">") - 1 );
    frame->getSheet()->setRowCount(1);
    frame->getSheet()->setColumnCount(6);
    frame->setFromText(scoText);
    if (liveEventsText.contains("name=\"")) {
      int index = liveEventsText.indexOf("name=\"") + 6;
      QString name = liveEventsText.mid(index,
                                        liveEventsText.indexOf("\"", index) - index );
      frame->setName(name);
    }
    if (liveEventsText.contains("tempo=\"")) {
      int index = liveEventsText.indexOf("tempo=\"") + 7;
      QString tempostr = liveEventsText.mid(index,
                                            liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      double tempo = tempostr.toDouble(&ok);
      if (ok)
        frame->setTempo(tempo);
    }
    if (liveEventsText.contains("loop=\"")) {
      int index = liveEventsText.indexOf("loop=\"") + 6;
      QString loopstr = liveEventsText.mid(index,
                                           liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      double loop = loopstr.toDouble(&ok);
      if (ok)
        frame->setLoopLength(loop);
    }
    int posx, posy, width, height;
    if (liveEventsText.contains("x=\"")) {
      int index = liveEventsText.indexOf("x=\"") + 3;
      QString xstr = liveEventsText.mid(index,
                                        liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      posx = xstr.toInt(&ok);
      if (!ok)
        posx = frame->x();
    }
    if (liveEventsText.contains("y=\"")) {
      int index = liveEventsText.indexOf("y=\"") + 3;
      QString ystr = liveEventsText.mid(index,
                                        liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      posy = ystr.toInt(&ok);
      if (!ok)
        posy = frame->y();
    }
    if (liveEventsText.contains("width=\"")) {
      int index = liveEventsText.indexOf("width=\"") + 7;
      QString wstr = liveEventsText.mid(index,
                                        liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      width = wstr.toInt(&ok);
      if (!ok)
        width = frame->width();
    }
    if (liveEventsText.contains("height=\"")) {
      int index = liveEventsText.indexOf("height=\"") + 8;
      QString hstr = liveEventsText.mid(index,
                                        liveEventsText.indexOf("\"", index) - index );
      bool ok = false;
      height = hstr.toInt(&ok);
      if (!ok)
        height = frame->height();
    }
    frame->move(posx, posy);
    frame->resize(width, height);
    text.remove(liveEventsText);
  }
  if (m_liveFrames.size() == 0) {
    LiveEventFrame *e = createLiveEventFrame();
    e->setFromText(QString()); // Must set blank for undo history point
  }
  m_view->setFullText(text);  // This must be here as some of the text has been removed along the way
  return 0;
}

QString DocumentPage::getFullText()
{
  QString fullText;
  fullText = m_view->getFullText();
//  if (!fullText.endsWith("\n"))
//    fullText += "\n";
  if (fileName.endsWith(".csd",Qt::CaseInsensitive) or fileName == "") {
    if (useXml) {
      // TODO implement xml widgets
    }
    else {
      fullText += getMacOptionsText() + "\n" + getMacWidgetsText() + "\n";
      fullText += getMacPresetsText() + "\n";
    }
    QString liveEventsText = "";
    if (saveLiveEvents) { // Only add live events sections if file is a csd file
      for (int i = 0; i < m_liveFrames.size(); i++) {
        QString panel = "<EventPanel name=\"";
        panel += m_liveFrames[i]->getName() + "\" tempo=\"";
        panel += QString::number(m_liveFrames[i]->getTempo(), 'f', 8) + "\" loop=\"";
        panel += QString::number(m_liveFrames[i]->getLoopLength(), 'f', 8) + "\" name=\"";
        panel += m_liveFrames[i]->getName() + "\" x=\"";
        panel += QString::number(m_liveFrames[i]->x()) + "\" y=\"";
        panel += QString::number(m_liveFrames[i]->y()) + "\" width=\"";
        panel += QString::number(m_liveFrames[i]->width()) + "\" height=\"";
        panel += QString::number(m_liveFrames[i]->height()) + "\">";
        panel += m_liveFrames[i]->getPlainText();
        panel += "</EventPanel>";
        liveEventsText += panel;
      }
      fullText += liveEventsText;
    }
  }
  else { // Not a csd file

  }
  return fullText;
}

QString DocumentPage::getBasicText()
{
  return m_view->getBasicText();
}

QString DocumentPage::getOptionsText()
{
  return m_view->getOptionsText();
}

QString DocumentPage::getDotText()
{
  if (fileName.endsWith("sco")) {
    qDebug() << "DocumentPage::getDotText(): No dot for sco files";
    return QString();
  }
  QString orcText = getFullText();
  if (!fileName.endsWith("orc")) { //asume csd
    if (orcText.contains("<CsInstruments>") && orcText.contains("</CsInstruments>")) {
      orcText = orcText.mid(orcText.indexOf("<CsInstruments>") + 15,
                            orcText.indexOf("</CsInstruments>") - orcText.indexOf("<CsInstruments>") - 15);
    }
  }
  DotGenerator dot(fileName, orcText, m_opcodeTree);
  return dot.getDotText();
}

QString DocumentPage::getMacWidgetsText()
{
  return m_widgetLayout->getMacWidgetsText();
}

QString DocumentPage::getMacPresetsText()
{
  return macPresets;
}

QString DocumentPage::getMacOptionsText()
{
  return macOptions.join("\n");
}

QString DocumentPage::getMacOptions(QString option)
{
  if (!option.endsWith(":"))
    option += ":";
  if (!option.endsWith(" "))
    option += " ";
  int index = macOptions.indexOf(QRegExp(option + ".*"));
  if (index < 0) {
    qDebug("DocumentPage::getMacOptions() Option %s not found!", option.toStdString().c_str());
    return QString("");
  }
  return macOptions[index].mid(option.size());
}

QString DocumentPage::getLiveEventsText()
{
  QString text;
  for (int i = 0; i < m_liveFrames.size(); i++) {
    text += m_liveFrames[i]->getPlainText();
  }
  return text;
}

QString DocumentPage::wordUnderCursor()
{
  return m_view->wordUnderCursor();
}

QRect DocumentPage::getWidgetPanelGeometry()
{
  int index = macOptions.indexOf(QRegExp("WindowBounds: .*"));
  if (index < 0) {
//     qDebug ("DocumentPage::getWidgetPanelGeometry() no Geometry!");
    return QRect();
  }
  QString line = macOptions[index];
  QStringList values = line.split(" ");
  values.removeFirst();  //remove property name
  return QRect(values[0].toInt(),
               values[1].toInt(),
               values[2].toInt(),
               values[3].toInt());
}

QString DocumentPage::getFilePath()
{
  return fileName.left(fileName.lastIndexOf("/"));
}

QStringList DocumentPage::getScheduledEvents(unsigned long ksmps)
{
  QStringList events;
  // Called once after every Csound control pass
  for (int i = 0; i < m_liveFrames.size(); i++) {
    m_liveFrames[i]->getEvents(ksmps, &events);
  }
//  m_ksmpscount = ksmpscount;
  return events;
}

void DocumentPage::setModified(bool mod)
{
  // This slot is triggered by the document children whenever they are modified
  // It is also called from the main application when the file is saved to set as unmodified.
  // FIXME live frame modification should also affect here
  if (mod == true) {
    emit modified();
  }
  else {
    m_view->setModified(false);
    m_widgetLayout->setModified(false);
    for (int i = 0; i < m_liveFrames.size(); i++) {
      m_liveFrames[i]->setModified(false);
    }
  }
}

bool DocumentPage::isModified()
{
  if (m_view->isModified())
    return true;
  if (m_widgetLayout->isModified())
    return true;
  for (int i = 0; i < m_liveFrames.size(); i++) {
    if (m_liveFrames[i]->isModified())
      return true;
  }
  return false;
}

bool DocumentPage::isRunning()
{
  // TODO what to do with pause?
  return m_csEngine->isRunning();
}

bool DocumentPage::isRecording()
{
  // TODO what to do with pause?
  return m_csEngine->isRecording();
}

bool DocumentPage::usesFltk()
{
  return m_view->getBasicText().contains("FLpanel");
}

void DocumentPage::updateCsLadspaText()
{
  QString text = "<csLADSPA>\nName=";
  text += fileName.mid(fileName.lastIndexOf("/") + 1) + "\n";
  text += "Maker=QuteCsound\n";
  text += "UniqueID=69873\n";  // FIXME generate a proper id
  text += "Copyright=none\n";
  text += m_widgetLayout->getCsladspaLines();
  text += "</csLADSPA>";
  m_view->setLadspaText(text);
}

void DocumentPage::focusWidgets()
{
  m_widgetLayout->setFocus();
}

void DocumentPage::copy()
{
  // For some strange reason, the shortcuts are not being intercepted by the widget layout directly...
  // so they are going through here.
  m_widgetLayout->copy();
}

void DocumentPage::cut()
{
  m_widgetLayout->cut();
}

void DocumentPage::paste()
{
  m_widgetLayout->paste();
}

void DocumentPage::undo()
{
  m_widgetLayout->undo();
}

void DocumentPage::redo()
{
  m_widgetLayout->redo();
}

DocumentView *DocumentPage::getView()
{
  return m_view;
}

//void DocumentPage::setWidgetLayout(WidgetLayout *w)
//{
//  if (w != 0) {
//    m_widgetLayout = w;
//  }
//  else {
//    qDebug() << "DocumentPage::setWidgetLayout()  NULL widget.";
//  }
//}

WidgetLayout *DocumentPage::getWidgetLayout()
{
  return m_widgetLayout;
}

ConsoleWidget *DocumentPage::getConsole()
{
  return m_console;
}

void DocumentPage::setTextFont(QFont font)
{
  m_view->setFont(font);
}

void DocumentPage::setTabStopWidth(int tabWidth)
{
  m_view->setTabStopWidth(tabWidth);
}

void DocumentPage::setLineWrapMode(QTextEdit::LineWrapMode wrapLines)
{
  m_view->setLineWrapMode(wrapLines);
}

void DocumentPage::setColorVariables(bool colorVariables)
{
  m_view->setColorVariables(colorVariables);
}

void DocumentPage::setOpcodeNameList(QStringList opcodeNameList)
{
  m_view->setOpcodeNameList(opcodeNameList);
}

void DocumentPage::print(QPrinter *printer)
{
  m_view->print(printer);
}

void DocumentPage::findReplace()
{
  m_view->findReplace();
}

void DocumentPage::findString()
{
  m_view->findString();
}

void DocumentPage::getToIn()
{
  m_view->getToIn();
}

void DocumentPage::inToGet()
{
  m_view->inToGet();
}


//void DocumentPage::setEditAct(QAction *editAct)
//{
//  m_widgetLayout->setEditAct(editAct);
//}

//void DocumentPage::setCsoundOptions(CsoundOptions &options)
//{
//  m_csEngine->setCsoundOptions(options);
//}

void DocumentPage::showWidgetTooltips(bool visible)
{
  m_widgetLayout->showWidgetTooltips(visible);
}

void DocumentPage::setKeyRepeatMode(bool keyRepeat)
{
  m_widgetLayout->setKeyRepeatMode(keyRepeat);
  m_console->setKeyRepeatMode(keyRepeat);
}

void DocumentPage::passWidgetClipboard(QString text)
{
  m_widgetLayout->passWidgetClipboard(text);
}

void DocumentPage::setConsoleFont(QFont font)
{
  m_console->setDefaultFont(font);
}

void DocumentPage::setConsoleColors(QColor fontColor, QColor bgColor)
{
  m_console->setColors(fontColor, bgColor);
}

//DocumentView * DocumentPage::view()
//{
//  return m_view;
//}
//
//CsoundEngine *DocumentPage::engine()
//{
//  return m_csEngine;
//}
//
//WidgetLayout * DocumentPage::widgetLayout()
//{
//  return m_widgetLayout;
//}

void DocumentPage::setScriptDirectory(QString dir)
{
  for (int i = 0; i < m_liveFrames.size(); i++) {
    m_liveFrames[i]->getSheet()->setScriptDirectory(dir);
  }
}

void DocumentPage::setConsoleBufferSize(int size)
{
  m_csEngine->setConsoleBufferSize(size);
}

void DocumentPage::setWidgetEnabled(bool enabled)
{
  // TODO disable widgetLayout if its not being used?
//  m_widgetLayout->setEnabled(enabled);
  m_csEngine->enableWidgets(enabled);
}

void DocumentPage::setRunThreaded(bool threaded)
{
  m_csEngine->setThreaded(threaded);  // This will take effect on next run of the engine
}

void DocumentPage::useInvalue(bool use)
{
  m_csEngine->useInvalue(use);  // This will take effect on next run of the engine
}

void DocumentPage::showLiveEventFrames(bool visible)
{
  for (int i = 0; i < m_liveFrames.size(); i++) {
    if (visible) {
      //  qDebug() << "DocumentPage::showLiveEventFrames  " << visible << (int) this;
      if (m_liveFrames[i]->isVisible())
        m_liveFrames[i]->raise();
      else
        m_liveFrames[i]->show();
        m_liveFrames[i]->raise();
    }
    else {
      m_liveFrames[i]->hide();
    }
  }
}

int DocumentPage::play(CsoundOptions *options)
{
  if (!m_csEngine->isRunning()) {
    m_view->unmarkErrorLines();  // Clear error lines when running
    m_console->reset(); // Clear consoles
    m_widgetLayout->flush();   // Flush accumulated values
    m_widgetLayout->clearGraphs();
  }
  return m_csEngine->play(options);
}

void DocumentPage::pause()
{
  m_csEngine->pause();
}

void DocumentPage::stop()
{
  if (m_csEngine->isRunning())
    m_csEngine->stop();
}

void DocumentPage::perfEnded()
{
  emit stopSignal();
}

void DocumentPage::record(int format)
{
  if (fileName.startsWith(":/")) {
    QMessageBox::critical(static_cast<QWidget *>(parent()),
                          tr("QuteCsound"),
                          tr("You must save the examples to use Record."),
                          QMessageBox::Ok);
    //FIXME connect rec act
//    recAct->setChecked(false);
    return;
  }
  int number = 0;
  QString recName = fileName + "-000.wav";
  while (QFile::exists(recName)) {
    number++;
    recName = fileName + "-";
    if (number < 10)
      recName += "0";
    if (number < 100)
      recName += "0";
    recName += QString::number(number) + ".wav";
  }
  m_csEngine->startRecording(format, recName);

  // FIXME setup stopping of recording!
  // FIXME connect this so that the current audio file for external editor, etc is set
  emit setCurrentAudioFile(recName);
}

void DocumentPage::stopRecording()
{
  m_csEngine->stopRecording();
}

void DocumentPage::playParent()
{
  static_cast<qutecsound *>(parent())->play();
}

void DocumentPage::renderParent()
{
  static_cast<qutecsound *>(parent())->render();
}

void DocumentPage::setMacWidgetsText(QString widgetText)
{
//   qDebug() << "DocumentPage::setMacWidgetsText: ";
  //FIXME put back
//  macGUI = widgetText;
//   document()->setModified(true);
}

void DocumentPage::setMacOptionsText(QString text)
{
  macOptions = text.split('\n');
}

void DocumentPage::setMacOption(QString option, QString newValue)
{
  if (!option.endsWith(":"))
    option += ":";
  if (!option.endsWith(" "))
    option += " ";
  int index = macOptions.indexOf(QRegExp(option + ".*"));
  if (index < 0) {
    qDebug("DocumentPage::setMacOption() Option not found!");
    return;
  }
  macOptions[index] = option + newValue;
  qDebug("DocumentPage::setMacOption() %s", macOptions[index].toStdString().c_str());
}

void DocumentPage::setWidgetPanelPosition(QPoint position)
{
  int index = macOptions.indexOf(QRegExp("WindowBounds: .*"));
  if (index < 0) {
    qDebug ("DocumentPage::getWidgetPanelGeometry() no Geometry!");
    return;
  }
  QStringList parts = macOptions[index].split(" ");
  parts.removeFirst();
  QString newline = "WindowBounds: " + QString::number(position.x()) + " ";
  newline += QString::number(position.y()) + " ";
  newline += parts[2] + " " + parts[3];
  macOptions[index] = newline;
//   qDebug("DocumentPage::setWidgetPanelPosition() %i %i", position.x(), position.y());
}

void DocumentPage::setWidgetPanelSize(QSize size)
{
  // FIXME move this so that only updated when needed (e.g. full text read)
  int index = macOptions.indexOf(QRegExp("WindowBounds: .*"));
  if (index < 0) {
    qDebug ("DocumentPage::getWidgetPanelGeometry() no Geometry!");
    return;
  }
  QStringList parts = macOptions[index].split(" ");
  parts.removeFirst();
  QString newline = "WindowBounds: ";
  newline += parts[0] + " " + parts[1] + " ";
  newline += QString::number(size.width()) + " ";
  newline += QString::number(size.height());
  macOptions[index] = newline;
//   qDebug("DocumentPage::setWidgetPanelSize() %i %i", size.width(), size.height());
}

void DocumentPage::setWidgetEditMode(bool active)
{
//  qDebug() << "DocumentPage::setWidgetEditMode";
  m_widgetLayout->setEditMode(active);
}

//void DocumentPage::toggleWidgetEditMode()
//{
//  qDebug() << "DocumentPage::toggleWidgetEditMode";
//  m_widgetLayout->toggleEditMode();
//}

void DocumentPage::duplicateWidgets()
{
  if (m_widgetLayout->hasFocus()) {
    m_widgetLayout->duplicate();
  }
}

void DocumentPage::jumpToLine(int line)
{
//  qDebug() << "DocumentPage::jumpToLine " << line;
  m_view->jumpToLine(line);
}

void DocumentPage::comment()
{
  qDebug() << "DocumentPage::comment()";
  if (m_view->hasFocus()) {   // Keyboard shortcut clashes with duplicate, so check for focus
    m_view->comment();
  }
}

void DocumentPage::uncomment()
{
  m_view->uncomment();
}

void DocumentPage::indent()
{
  m_view->indent();
}

void DocumentPage::unindent()
{
  m_view->unindent();
}

void DocumentPage::autoComplete()
{
  m_view->autoComplete();
}

void DocumentPage::newLiveEventFrame(QString text)
{
  LiveEventFrame *e = createLiveEventFrame(text);
  e->show();  //Assume that since slot was called ust be visible
}

LiveEventFrame * DocumentPage::createLiveEventFrame(QString text)
{
//  qDebug() << "DocumentPage::newLiveEventFrame()";
  LiveEventFrame *e = new LiveEventFrame("Live Event", 0, Qt::Window);  //FIXME is it OK to have no parent?
//  e->setAttribute(Qt::WA_DeleteOnClose, false);
  e->hide();

  if (!text.isEmpty()) {
    e->setFromText(text);
  }
  m_liveFrames.append(e);
  connect(e, SIGNAL(closed()), this, SLOT(liveEventFrameClosed()));
  connect(e, SIGNAL(newFrameSignal(QString)), this, SLOT(newLiveEventFrame(QString)));
  connect(e, SIGNAL(deleteFrameSignal(LiveEventFrame *)), this, SLOT(deleteLiveEventFrame(LiveEventFrame *)));
  connect(e->getSheet(), SIGNAL(sendEvent(QString)),this,SLOT(queueEvent(QString)));
  return e;
}

void DocumentPage::deleteLiveEventFrame(LiveEventFrame *frame)
{
//  qDebug() << "deleteLiveEventFrame(LiveEventFrame *frame)";
  int index = m_liveFrames.indexOf(frame);
  if (index >= 0) {
    disconnect(frame, 0,0,0);
    disconnect(frame->getSheet(), 0,0,0);
    m_liveFrames.remove(index);
    frame->deleteLater();
  }
  else {
    qDebug() << "DocumentPage::deleteLiveEventFrame frame not found";
  }
}

void DocumentPage::textChanged()
{
//  qDebug() << "DocumentPage::textChanged()";
  emit currentTextUpdated();
}

void DocumentPage::liveEventFrameClosed()
{
//  qDebug() << "DocumentPage::liveEventFrameClosed()";
  LiveEventFrame *e = dynamic_cast<LiveEventFrame *>(QObject::sender());
//  if (e != 0) { // This shouldn't really be necessary but just in case
  bool shown = false;
  for (int i = 0; i < m_liveFrames.size(); i++) {
    if (m_liveFrames[i]->isVisible()
      && m_liveFrames[i] != e)  // frame that called has not been called yet
      shown = true;
  }
  emit liveEventsVisible(shown);
//  }
}

void DocumentPage::opcodeSyntax(QString message)
{
  emit opcodeSyntaxSignal(message);
}

void DocumentPage::setWidgetClipboard(QString message)
{
  emit setWidgetClipboardSignal(message);
}

void DocumentPage::queueEvent(QString eventLine, int delay)
{
//   qDebug("WidgetPanel::queueEvent %s", eventLine.toStdString().c_str());
  m_csEngine->queueEvent(eventLine, delay);  //TODO  implement passing of timestamp
}
