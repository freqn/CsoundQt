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

#ifndef QUTEMETER_H
#define QUTEMETER_H

#include "qutewidget.h"

// This class is named "meter" internally, following MacCsound's name
// It is called "Controller" for the user of QuteCsound

class QuteMeter : public QuteWidget
{
  Q_OBJECT
  public:
    QuteMeter(QWidget *parent);

    ~QuteMeter();

    virtual QString getWidgetLine();
    virtual QString getWidgetXmlText();
    virtual QString getWidgetType();

//    void setChannel2Name(QString name);
//    void setColor(QColor color);
//    void setType(QString type);
//    void setPointSize(int size);
//    void setFadeSpeed(int speed);
//    void setBehavior(QString behavior);
    virtual void setWidgetGeometry(int x,int y,int width,int height);

    virtual void createPropertiesDialog();

    virtual void refreshWidget();
    virtual void applyProperties();
    void applyInternalProperties();

  protected:

  private:
    QLineEdit* name2LineEdit;
    QPushButton*  colorButton;
    QComboBox* typeComboBox;
    QSpinBox* pointSizeSpinBox;
    QSpinBox* fadeSpeedSpinBox;
    QComboBox* behaviorComboBox;
    QDoubleSpinBox* m_xMinBox;
    QDoubleSpinBox* m_xMaxBox;
    QDoubleSpinBox* m_yMinBox;
    QDoubleSpinBox* m_yMaxBox;

  private slots:
    void selectTextColor();
    void setValuesFromWidget(double value1, double value2);
};

class MeterWidget : public QGraphicsView
{
  Q_OBJECT
  public:
    MeterWidget(QWidget *parent);
    ~MeterWidget();

    void setValue(double value);
    void setValue2(double value2);
    void setType(QString type);
    void setRanges(double minx,double  maxx,double  miny,double  maxy);
    void setPointSize(int size);
    void setColor(QColor color);
    void setWidgetGeometry(int x,int y,int width,int height);

    QColor getColor();
    QString getType() {return m_type;}
    int getPointSize() {return m_pointSize;}
    bool m_vertical;

  protected:
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
//    virtual void mouseReleaseEvent(QMouseEvent* event);

  private:
    double m_value, m_value2;  // value1 is vertical, value2 is horizontal
    double m_xmin, m_xmax, m_ymin, m_ymax;
    QString m_type;
    int m_pointSize;
    bool m_mouseDown;

    QGraphicsScene* m_scene;

    QGraphicsRectItem* m_block;
    QGraphicsEllipseItem* m_point;
    QGraphicsLineItem* m_vline;
    QGraphicsLineItem* m_hline;

  signals:
    void newValues(double value1, double value2);
    void valueChanged(double value);
    void value2Changed(double value);
};

#endif
