/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010, 2011, 2012, 2013
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
 * Joe <cuchac@email.cz>
 * Daniel Schaal <farbing@web.de>
 * Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
 * Frédéric Bour <frederic.bour@lakaban.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZOOMDIALOG_H
#define ZOOMDIALOG_H

#include "config.h"

#include <QWidget>
#include "dispatch.h"

class QDoubleSpinBox;
class QSlider;

class zoom_dialog : public QWidget, public controller
{
    Q_OBJECT

private:
    bool _lock;
    QDoubleSpinBox *_z_spinbox;
    QSlider *_z_slider;

private slots:
    void z_slider_changed(int val);
    void z_spinbox_changed(double val);

public:
    zoom_dialog(QWidget *parent = 0);

    virtual void receive_notification(const notification &note);
};

#endif
