/*
 * This file is part of bino, a 3D video player.
 *
 * Copyright (C) 2010-2011
 * Martin Lambers <marlam@marlam.de>
 * Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
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

#include "config.h"

#include <QCoreApplication>
#include <QApplication>
#include <QMainWindow>
#include <QGridLayout>
#include <QCloseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QGridLayout>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QFile>
#include <QByteArray>
#include <QCryptographicHash>
#include <QUrl>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QSpinBox>

#include "player_qt.h"
#include "qt_app.h"
#include "video_output_opengl_qt.h"
#include "lib_versions.h"


player_qt_internal::player_qt_internal(video_container_widget *container_widget)
    : player(player::master), _container_widget(container_widget), _playing(false)
{
}

player_qt_internal::~player_qt_internal()
{
}

void player_qt_internal::open(const player_init_data &init_data)
{
    // This is the same as player::open except for the creation
    // of the video output. Here, we must pass the container widget.
    reset_playstate();
    set_benchmark(init_data.benchmark);
    create_decoders(init_data.filenames);
    create_input(init_data.input_mode, init_data.audio_stream);
    create_audio_output();
    set_video_output(new video_output_opengl_qt(_container_widget));
    video_state() = init_data.video_state;
    open_video_output(init_data.video_mode, init_data.video_flags);
}

void player_qt_internal::receive_cmd(const command &cmd)
{
    if (cmd.type == command::toggle_play && !_playing)
    {
        notify(notification::play, false, true);
    }
    else if (_playing)
    {
        player::receive_cmd(cmd);
    }
}

void player_qt_internal::receive_notification(const notification &note)
{
    if (note.type == notification::play)
    {
        _playing = note.current.flag;
    }
}

bool player_qt_internal::playloop_step()
{
    bool more_steps;
    int64_t seek_to;
    bool prep_frame;
    bool drop_frame;
    bool display_frame;

    run_step(&more_steps, &seek_to, &prep_frame, &drop_frame, &display_frame);
    if (!more_steps)
    {
        return false;
    }
    if (prep_frame)
    {
        get_video_frame();
        prepare_video_frame(get_video_output());
        release_video_frame();
    }
    if (drop_frame)
    {
        get_input()->release_video_frame();
    }
    if (display_frame)
    {
        get_video_output()->activate();
    }
    return true;
}

void player_qt_internal::force_stop()
{
    notify(notification::play, false, false);
}

void player_qt_internal::move_event()
{
    video_output_opengl_qt *vo = static_cast<video_output_opengl_qt *>(get_video_output());
    if (vo)
    {
        vo->move_event();
    }
}


in_out_widget::in_out_widget(QSettings *settings, QWidget *parent)
    : QWidget(parent), _settings(settings), _lock(false)
{
    QGridLayout *layout0 = new QGridLayout;
    QLabel *input_label = new QLabel("Input:");
    layout0->addWidget(input_label, 0, 0);
    _input_combobox = new QComboBox(this);
    _input_combobox->addItem("2D");
    _input_combobox->addItem("Separate streams");
    _input_combobox->addItem("Top/bottom");
    _input_combobox->addItem("Top/bottom, half height");
    _input_combobox->addItem("Left/right");
    _input_combobox->addItem("Left/right, half width");
    _input_combobox->addItem("Even/odd rows");
    connect(_input_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(input_changed()));
    layout0->addWidget(_input_combobox, 0, 1);
    layout0->setColumnStretch(1, 1);
    QLabel *audio_label = new QLabel("Audio:");
    layout0->addWidget(audio_label, 0, 2);
    _audio_spinbox = new QSpinBox();
    _audio_spinbox->setRange(1, 999);
    _audio_spinbox->setValue(1);
    layout0->addWidget(_audio_spinbox, 0, 3);
    QLabel *output_label = new QLabel("Output:");
    QGridLayout *layout1 = new QGridLayout;
    layout1->addWidget(output_label, 1, 0);
    _output_combobox = new QComboBox(this);
    _output_combobox->addItem("Left view");
    _output_combobox->addItem("Right view");
    _output_combobox->addItem("Top/bottom");
    _output_combobox->addItem("Top/bottom, half height");
    _output_combobox->addItem("Left/right");
    _output_combobox->addItem("Left/right, half width");
    _output_combobox->addItem("Even/odd rows");
    _output_combobox->addItem("Even/odd columns");
    _output_combobox->addItem("Checkerboard pattern");
    _output_combobox->addItem("Red/cyan glasses, Dubois method");
    _output_combobox->addItem("Red/cyan glasses, monochrome method");
    _output_combobox->addItem("Red/cyan glasses, full-color method");
    _output_combobox->addItem("Red/cyan glasses, half-color method");
    _output_combobox->addItem("OpenGL stereo");
    layout1->addWidget(_output_combobox, 1, 1);
    layout1->setColumnStretch(1, 1);
    QGridLayout *layout2 = new QGridLayout;
    _swap_eyes_button = new QPushButton("Swap eyes");
    _swap_eyes_button->setCheckable(true);
    connect(_swap_eyes_button, SIGNAL(toggled(bool)), this, SLOT(swap_eyes_changed()));
    layout2->addWidget(_swap_eyes_button, 0, 0, 1, 2);
    _fullscreen_button = new QPushButton("Fullscreen");
    connect(_fullscreen_button, SIGNAL(pressed()), this, SLOT(fullscreen_pressed()));
    layout2->addWidget(_fullscreen_button, 0, 2, 1, 2);
    _center_button = new QPushButton("Center");
    connect(_center_button, SIGNAL(pressed()), this, SLOT(center_pressed()));
    layout2->addWidget(_center_button, 0, 4, 1, 2);
    _parallax_label = new QLabel("Parallax:");
    layout2->addWidget(_parallax_label, 0, 6, 1, 1);
    _parallax_spinbox = new QDoubleSpinBox();
    _parallax_spinbox->setRange(-1.0, +1.0);
    _parallax_spinbox->setValue(0);
    _parallax_spinbox->setDecimals(2);
    _parallax_spinbox->setSingleStep(0.01);
    connect(_parallax_spinbox, SIGNAL(valueChanged(double)), this, SLOT(parallax_changed()));
    layout2->addWidget(_parallax_spinbox, 0, 7, 1, 1);
    _ghostbust_label = new QLabel("Ghostbusting:");
    layout2->addWidget(_ghostbust_label, 0, 8, 1, 1);
    _ghostbust_spinbox = new QSpinBox();
    _ghostbust_spinbox->setRange(0, 100);
    _ghostbust_spinbox->setValue(0);
    connect(_ghostbust_spinbox, SIGNAL(valueChanged(int)), this, SLOT(ghostbust_changed()));
    layout2->addWidget(_ghostbust_spinbox, 0, 9, 1, 1);
    layout2->setRowStretch(0, 1);
    QGridLayout *layout = new QGridLayout;
    layout->addLayout(layout0, 0, 0);
    layout->addLayout(layout1, 1, 0);
    layout->addLayout(layout2, 2, 0);
    setLayout(layout);

    // Align the input and output labels
    output_label->setMinimumSize(output_label->minimumSizeHint());
    input_label->setMinimumSize(output_label->minimumSizeHint());

    _input_combobox->setEnabled(false);
    _audio_spinbox->setEnabled(false);
    _output_combobox->setEnabled(false);
    _swap_eyes_button->setEnabled(false);
    _fullscreen_button->setEnabled(false);
    _center_button->setEnabled(false);
    _parallax_label->setEnabled(false);
    _parallax_spinbox->setEnabled(false);
    _ghostbust_label->setEnabled(false);
    _ghostbust_spinbox->setEnabled(false);
}

in_out_widget::~in_out_widget()
{
}

void in_out_widget::set_input(enum input::mode m)
{
    switch (m)
    {
    default:
    case input::mono:
        _input_combobox->setCurrentIndex(0);
        break;
    case input::separate:
        _input_combobox->setCurrentIndex(1);
        break;
    case input::top_bottom:
        _input_combobox->setCurrentIndex(2);
        break;
    case input::top_bottom_half:
        _input_combobox->setCurrentIndex(3);
        break;
    case input::left_right:
        _input_combobox->setCurrentIndex(4);
        break;
    case input::left_right_half:
        _input_combobox->setCurrentIndex(5);
        break;
    case input::even_odd_rows:
        _input_combobox->setCurrentIndex(6);
        break;
    }
}

void in_out_widget::set_output(enum video_output::mode m)
{
    switch (m)
    {
    default:
    case video_output::mono_left:
        _output_combobox->setCurrentIndex(0);
        break;
    case video_output::mono_right:
        _output_combobox->setCurrentIndex(1);
        break;
    case video_output::top_bottom:
        _output_combobox->setCurrentIndex(2);
        break;
    case video_output::top_bottom_half:
        _output_combobox->setCurrentIndex(3);
        break;
    case video_output::left_right:
        _output_combobox->setCurrentIndex(4);
        break;
    case video_output::left_right_half:
        _output_combobox->setCurrentIndex(5);
        break;
    case video_output::even_odd_rows:
        _output_combobox->setCurrentIndex(6);
        break;
    case video_output::even_odd_columns:
        _output_combobox->setCurrentIndex(7);
        break;
    case video_output::checkerboard:
        _output_combobox->setCurrentIndex(8);
        break;
    case video_output::anaglyph_red_cyan_dubois:
        _output_combobox->setCurrentIndex(9);
        break;
    case video_output::anaglyph_red_cyan_monochrome:
        _output_combobox->setCurrentIndex(10);
        break;
    case video_output::anaglyph_red_cyan_full_color:
        _output_combobox->setCurrentIndex(11);
        break;
    case video_output::anaglyph_red_cyan_half_color:
        _output_combobox->setCurrentIndex(12);
        break;
    case video_output::stereo:
        _output_combobox->setCurrentIndex(13);
        break;
    }
}

void in_out_widget::input_changed()
{
    if (input::mode_is_2d(input_mode()) && !video_output::mode_is_2d(video_mode()))
    {
        _settings->beginGroup("Session");
        QString fallback_mode_name = QString(video_output::mode_name(video_output::mono_left).c_str());
        QString mode_name = _settings->value(QString("2d-output-mode"), fallback_mode_name).toString();
        set_output(video_output::mode_from_name(mode_name.toStdString()));
        _settings->endGroup();
    }
    else if (!input::mode_is_2d(input_mode()) && video_output::mode_is_2d(video_mode()))
    {
        _settings->beginGroup("Session");
        QString fallback_mode_name = QString(video_output::mode_name(video_output::anaglyph_red_cyan_dubois).c_str());
        QString mode_name = _settings->value(QString("3d-output-mode"), fallback_mode_name).toString();
        set_output(video_output::mode_from_name(mode_name.toStdString()));
        _settings->endGroup();
    }
}

void in_out_widget::swap_eyes_changed()
{
    if (!_lock)
    {
        send_cmd(command::toggle_swap_eyes);
    }
}

void in_out_widget::fullscreen_pressed()
{
    send_cmd(command::toggle_fullscreen);
}

void in_out_widget::center_pressed()
{
    send_cmd(command::center);
}

void in_out_widget::parallax_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_parallax, _parallax_spinbox->value());
    }
}

void in_out_widget::ghostbust_changed()
{
    if (!_lock)
    {
        send_cmd(command::set_ghostbust, _ghostbust_spinbox->value() / 100.0f);
    }
}

void in_out_widget::update(const player_init_data &init_data, bool have_valid_input, bool playing)
{
    set_input(init_data.input_mode);
    set_output(init_data.video_mode);
    _lock = true;
    _audio_spinbox->setValue(init_data.audio_stream + 1);
    _swap_eyes_button->setChecked(init_data.video_state.swap_eyes);
    _parallax_spinbox->setValue(init_data.video_state.parallax);
    _ghostbust_spinbox->setValue(qRound(init_data.video_state.ghostbust * 100.0f));
    _lock = false;
    if (have_valid_input)
    {
        receive_notification(notification(notification::play, !playing, playing));
    }
    else
    {
        _input_combobox->setEnabled(false);
        _audio_spinbox->setEnabled(false);
        _output_combobox->setEnabled(false);
        _swap_eyes_button->setEnabled(false);
        _fullscreen_button->setEnabled(false);
        _center_button->setEnabled(false);
        _parallax_label->setEnabled(false);
        _parallax_spinbox->setEnabled(false);
        _ghostbust_label->setEnabled(false);
        _ghostbust_spinbox->setEnabled(false);
    }
}

enum input::mode in_out_widget::input_mode()
{
    switch (_input_combobox->currentIndex())
    {
    case 0:
        return input::mono;
    case 1:
        return input::separate;
    case 2:
        return input::top_bottom;
    case 3:
        return input::top_bottom_half;
    case 4:
        return input::left_right;
    case 5:
        return input::left_right_half;
    case 6:
    default:
        return input::even_odd_rows;
    }
}

int in_out_widget::audio_stream()
{
    return _audio_spinbox->value() - 1;
}

enum video_output::mode in_out_widget::video_mode()
{
    switch (_output_combobox->currentIndex())
    {
    case 0:
        return video_output::mono_left;
    case 1:
        return video_output::mono_right;
    case 2:
        return video_output::top_bottom;
    case 3:
        return video_output::top_bottom_half;
    case 4:
        return video_output::left_right;
    case 5:
        return video_output::left_right_half;
    case 6:
        return video_output::even_odd_rows;
    case 7:
        return video_output::even_odd_columns;
    case 8:
        return video_output::checkerboard;
    case 9:
        return video_output::anaglyph_red_cyan_dubois;
    case 10:
        return video_output::anaglyph_red_cyan_monochrome;
    case 11:
        return video_output::anaglyph_red_cyan_full_color;
    case 12:
        return video_output::anaglyph_red_cyan_half_color;
    case 13:
    default:
        return video_output::stereo;
    }
}

void in_out_widget::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::play:
        _input_combobox->setEnabled(!note.current.flag);
        _audio_spinbox->setEnabled(!note.current.flag);
        _output_combobox->setEnabled(!note.current.flag);
        _swap_eyes_button->setEnabled(note.current.flag);
        _fullscreen_button->setEnabled(note.current.flag);
        _center_button->setEnabled(note.current.flag);
        {
            bool parallax = (note.current.flag && input_mode() != input::mono);
            _parallax_label->setEnabled(parallax);
            _parallax_spinbox->setEnabled(parallax);
        }
        {
            bool ghostbust = (note.current.flag
                    && video_mode() != video_output::anaglyph_red_cyan_dubois
                    && video_mode() != video_output::anaglyph_red_cyan_monochrome
                    && video_mode() != video_output::anaglyph_red_cyan_full_color
                    && video_mode() != video_output::anaglyph_red_cyan_half_color);
            _ghostbust_label->setEnabled(ghostbust);
            _ghostbust_spinbox->setEnabled(ghostbust);
        }
        break;
    case notification::swap_eyes:
        _lock = true;
        _swap_eyes_button->setChecked(note.current.flag);
        _lock = false;
        break;
    case notification::parallax:
        _lock = true;
        if (_parallax_spinbox->isEnabled())
        {
            _parallax_spinbox->setValue(note.current.value);
        }
        _lock = false;
    case notification::ghostbust:
        _lock = true;
        if (_ghostbust_spinbox->isEnabled())
        {
            _ghostbust_spinbox->setValue(qRound(note.current.value * 100.0f));
        }
        _lock = false;
    default:
        break;
    }
}


controls_widget::controls_widget(QSettings *settings, QWidget *parent)
    : QWidget(parent), _settings(settings), _playing(false)
{
    QGridLayout *layout = new QGridLayout;
    _play_button = new QPushButton(QIcon(":icons/play.png"), "");
    connect(_play_button, SIGNAL(pressed()), this, SLOT(play_pressed()));
    layout->addWidget(_play_button, 0, 0);
    _pause_button = new QPushButton(QIcon(":icons/pause.png"), "");
    connect(_pause_button, SIGNAL(pressed()), this, SLOT(pause_pressed()));
    layout->addWidget(_pause_button, 0, 1);
    _stop_button = new QPushButton(QIcon(":icons/stop.png"), "");
    connect(_stop_button, SIGNAL(pressed()), this, SLOT(stop_pressed()));
    layout->addWidget(_stop_button, 0, 2);
    layout->addWidget(new QWidget, 0, 3);
    _bbb_button = new QPushButton(QIcon(":icons/bbb.png"), "");
    connect(_bbb_button, SIGNAL(pressed()), this, SLOT(bbb_pressed()));
    layout->addWidget(_bbb_button, 0, 4);
    _bb_button = new QPushButton(QIcon(":icons/bb.png"), "");
    connect(_bb_button, SIGNAL(pressed()), this, SLOT(bb_pressed()));
    layout->addWidget(_bb_button, 0, 5);
    _b_button = new QPushButton(QIcon(":icons/b.png"), "");
    connect(_b_button, SIGNAL(pressed()), this, SLOT(b_pressed()));
    layout->addWidget(_b_button, 0, 6);
    _f_button = new QPushButton(QIcon(":icons/f.png"), "");
    connect(_f_button, SIGNAL(pressed()), this, SLOT(f_pressed()));
    layout->addWidget(_f_button, 0, 7);
    _ff_button = new QPushButton(QIcon(":icons/ff.png"), "");
    connect(_ff_button, SIGNAL(pressed()), this, SLOT(ff_pressed()));
    layout->addWidget(_ff_button, 0, 8);
    _fff_button = new QPushButton(QIcon(":icons/fff.png"), "");
    connect(_fff_button, SIGNAL(pressed()), this, SLOT(fff_pressed()));
    layout->addWidget(_fff_button, 0, 9);
    layout->setRowStretch(0, 0);
    layout->setColumnStretch(3, 1);
    setLayout(layout);

    _play_button->setEnabled(false);
    _pause_button->setEnabled(false);
    _stop_button->setEnabled(false);
    _bbb_button->setEnabled(false);
    _bb_button->setEnabled(false);
    _b_button->setEnabled(false);
    _f_button->setEnabled(false);
    _ff_button->setEnabled(false);
    _fff_button->setEnabled(false);
}

controls_widget::~controls_widget()
{
}

void controls_widget::play_pressed()
{
    if (_playing)
    {
        send_cmd(command::toggle_pause);
    }
    else
    {
        send_cmd(command::toggle_play);
    }
}

void controls_widget::pause_pressed()
{
    send_cmd(command::toggle_pause);
}

void controls_widget::stop_pressed()
{
    send_cmd(command::toggle_play);
}

void controls_widget::bbb_pressed()
{
    send_cmd(command::seek, -600.0f);
}

void controls_widget::bb_pressed()
{
    send_cmd(command::seek, -60.0f);
}

void controls_widget::b_pressed()
{
    send_cmd(command::seek, -10.0f);
}

void controls_widget::f_pressed()
{
    send_cmd(command::seek, +10.0f);
}

void controls_widget::ff_pressed()
{
    send_cmd(command::seek, +60.0f);
}

void controls_widget::fff_pressed()
{
    send_cmd(command::seek, +600.0f);
}

void controls_widget::update(const player_init_data &, bool have_valid_input, bool playing)
{
    if (have_valid_input)
    {
        receive_notification(notification(notification::play, !playing, playing));
    }
    else
    {
        _playing = false;
        _play_button->setEnabled(false);
        _pause_button->setEnabled(false);
        _stop_button->setEnabled(false);
        _bbb_button->setEnabled(false);
        _bb_button->setEnabled(false);
        _b_button->setEnabled(false);
        _f_button->setEnabled(false);
        _ff_button->setEnabled(false);
        _fff_button->setEnabled(false);
    }
}

void controls_widget::receive_notification(const notification &note)
{
    switch (note.type)
    {
    case notification::play:
        _playing = note.current.flag;
        _play_button->setEnabled(!note.current.flag);
        _pause_button->setEnabled(note.current.flag);
        _stop_button->setEnabled(note.current.flag);
        _bbb_button->setEnabled(note.current.flag);
        _bb_button->setEnabled(note.current.flag);
        _b_button->setEnabled(note.current.flag);
        _f_button->setEnabled(note.current.flag);
        _ff_button->setEnabled(note.current.flag);
        _fff_button->setEnabled(note.current.flag);
        break;
    case notification::pause:
        _play_button->setEnabled(note.current.flag);
        _pause_button->setEnabled(!note.current.flag);
        break;
    default:
        break;
    }
}


main_window::main_window(QSettings *settings, const player_init_data &init_data)
    : _settings(settings), _player(NULL), _init_data(init_data), _stop_request(false)
{
    // Application properties
    setWindowTitle(PACKAGE_NAME);
    setWindowIcon(QIcon(":icons/appicon.png"));

    // Load preferences
    _settings->beginGroup("Session");
    _init_data.video_state.crosstalk_r = _settings->value("crosstalk_r", QString("0")).toFloat();
    _init_data.video_state.crosstalk_g = _settings->value("crosstalk_g", QString("0")).toFloat();
    _init_data.video_state.crosstalk_b = _settings->value("crosstalk_b", QString("0")).toFloat();
    _settings->endGroup();

    // Central widget
    QWidget *central_widget = new QWidget(this);
    QGridLayout *layout = new QGridLayout();
    _video_container_widget = new video_container_widget(central_widget);
    connect(_video_container_widget, SIGNAL(move_event()), this, SLOT(move_event()));
    layout->addWidget(_video_container_widget, 0, 0);
    _in_out_widget = new in_out_widget(_settings, central_widget);
    layout->addWidget(_in_out_widget, 1, 0);
    _controls_widget = new controls_widget(_settings, central_widget);
    layout->addWidget(_controls_widget, 2, 0);
    layout->setRowStretch(0, 1);
    layout->setColumnStretch(0, 1);
    central_widget->setLayout(layout);
    setCentralWidget(central_widget);

    // Menus
    QMenu *file_menu = menuBar()->addMenu(tr("&File"));
    QAction *file_open_act = new QAction(tr("&Open..."), this);
    file_open_act->setShortcut(QKeySequence::Open);
    connect(file_open_act, SIGNAL(triggered()), this, SLOT(file_open()));
    file_menu->addAction(file_open_act);
    QAction *file_open_url_act = new QAction(tr("Open &URL..."), this);
    connect(file_open_url_act, SIGNAL(triggered()), this, SLOT(file_open_url()));
    file_menu->addAction(file_open_url_act);
    file_menu->addSeparator();
    QAction *file_quit_act = new QAction(tr("&Quit..."), this);
#if QT_VERSION >= 0x040600
    file_quit_act->setShortcut(QKeySequence::Quit);
#else
    file_quit_act->setShortcut(tr("Ctrl+Q"));
#endif
    connect(file_quit_act, SIGNAL(triggered()), this, SLOT(close()));
    file_menu->addAction(file_quit_act);
    QMenu *preferences_menu = menuBar()->addMenu(tr("&Preferences"));
    QAction *preferences_crosstalk_act = new QAction(tr("&Crosstalk..."), this);
    connect(preferences_crosstalk_act, SIGNAL(triggered()), this, SLOT(preferences_crosstalk()));
    preferences_menu->addAction(preferences_crosstalk_act);
    QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
    QAction *help_manual_act = new QAction(tr("&Manual..."), this);
    help_manual_act->setShortcut(QKeySequence::HelpContents);
    connect(help_manual_act, SIGNAL(triggered()), this, SLOT(help_manual()));
    help_menu->addAction(help_manual_act);
    QAction *help_website_act = new QAction(tr("&Website..."), this);
    connect(help_website_act, SIGNAL(triggered()), this, SLOT(help_website()));
    help_menu->addAction(help_website_act);
    QAction *help_keyboard_act = new QAction(tr("&Keyboard Shortcuts"), this);
    connect(help_keyboard_act, SIGNAL(triggered()), this, SLOT(help_keyboard()));
    help_menu->addAction(help_keyboard_act);
    QAction *help_about_act = new QAction(tr("&About"), this);
    connect(help_about_act, SIGNAL(triggered()), this, SLOT(help_about()));
    help_menu->addAction(help_about_act);

    // Handle FileOpen events
    QApplication::instance()->installEventFilter(this);

    show();     // Must happen before opening initial files!
    raise();

    // Player and timer
    _player = new player_qt_internal(_video_container_widget);
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(playloop_step()));

    // Open files if any
    if (init_data.filenames.size() > 0)
    {
        QStringList filenames;
        for (size_t i = 0; i < init_data.filenames.size(); i++)
        {
            filenames.push_back(QFile::decodeName(init_data.filenames[i].c_str()));
        }
        open(filenames, false);
    }
    else
    {
        adjustSize();   // Adjust size for initial dummy video widget
    }
}

main_window::~main_window()
{
    if (_player)
    {
        try { _player->close(); } catch (...) { }
        delete _player;
    }
}

QString main_window::current_file_hash()
{
    // Return SHA1 hash of the name of the current file as a hex string
    QString name = QFileInfo(QFile::decodeName(_init_data.filenames[0].c_str())).fileName();
    QString hash = QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1).toHex();
    return hash;
}

bool main_window::open_player()
{
    try
    {
        _player->open(_init_data);
    }
    catch (std::exception &e)
    {
        QMessageBox::critical(this, "Error", tr("%1").arg(e.what()));
        return false;
    }
    adjustSize();
    return true;
}

void main_window::receive_notification(const notification &note)
{
    if (note.type == notification::play)
    {
        if (note.current.flag)
        {
            // Close and re-open the player. This resets the video state in case
            // we played it before, and it sets the input/output modes to the
            // current choice.
            _player->close();
            _init_data.input_mode = _in_out_widget->input_mode();
            _init_data.audio_stream = _in_out_widget->audio_stream();
            _init_data.video_mode = _in_out_widget->video_mode();
            if (!open_player())
            {
                _stop_request = true;
            }
            // Remember the input settings of this video, using an SHA1 hash of its filename.
            _settings->beginGroup("Video");
            if (_init_data.filenames.size() == 1)
            {
                _settings->setValue(current_file_hash(), QString(input::mode_name(_init_data.input_mode).c_str()));
            }
            _settings->setValue(current_file_hash() + "-audio-stream", QVariant(_init_data.audio_stream + 1).toString());
            _settings->endGroup();
            // Remember the 2D or 3D video output mode.
            _settings->beginGroup("Session");
            if (_init_data.input_mode == input::mono)
            {
                _settings->setValue("2d-output-mode", QString(video_output::mode_name(_init_data.video_mode).c_str()));
            }
            else
            {
                _settings->setValue("3d-output-mode", QString(video_output::mode_name(_init_data.video_mode).c_str()));
            }
            _settings->endGroup();
            // Update widgets: we're now playing
            _in_out_widget->update(_init_data, true, true);
            _controls_widget->update(_init_data, true, true);
            // Give the keyboard focus to the video widget
            _video_container_widget->setFocus(Qt::OtherFocusReason);
            // Start the play loop
            _timer->start(0);
        }
        else
        {
            _timer->stop();
            _player->close();
        }
    }
    else if (note.type == notification::contrast)
    {
        _init_data.video_state.contrast = note.current.value;
    }
    else if (note.type == notification::brightness)
    {
        _init_data.video_state.brightness = note.current.value;
    }
    else if (note.type == notification::hue)
    {
        _init_data.video_state.hue = note.current.value;
    }
    else if (note.type == notification::saturation)
    {
        _init_data.video_state.saturation = note.current.value;
    }
    else if (note.type == notification::swap_eyes)
    {
        _init_data.video_state.swap_eyes = note.current.flag;
        _settings->beginGroup("Video");
        _settings->setValue(current_file_hash() + "-swap-eyes", QVariant(_init_data.video_state.swap_eyes).toString());
        _settings->endGroup();
    }
    else if (note.type == notification::parallax)
    {
        _init_data.video_state.parallax = note.current.value;
        _settings->beginGroup("Video");
        _settings->setValue(current_file_hash() + "-parallax", QVariant(_init_data.video_state.parallax).toString());
        _settings->endGroup();
    }
    else if (note.type == notification::ghostbust)
    {
        _init_data.video_state.ghostbust = note.current.value;
        _settings->beginGroup("Video");
        _settings->setValue(current_file_hash() + "-ghostbust", QVariant(_init_data.video_state.ghostbust).toString());
        _settings->endGroup();
    }
}

void main_window::moveEvent(QMoveEvent *)
{
    move_event();
}

void main_window::closeEvent(QCloseEvent *event)
{
    // Remember the preferences
    _settings->beginGroup("Session");
    _settings->setValue("crosstalk_r", QVariant(_init_data.video_state.crosstalk_r).toString());
    _settings->setValue("crosstalk_g", QVariant(_init_data.video_state.crosstalk_g).toString());
    _settings->setValue("crosstalk_b", QVariant(_init_data.video_state.crosstalk_b).toString());
    _settings->endGroup();
    event->accept();
}

void main_window::move_event()
{
    if (_player)
    {
        _player->move_event();
    }
}

void main_window::playloop_step()
{
    if (_stop_request)
    {
        _timer->stop();
        _player->force_stop();
        _stop_request = false;
    }
    else if (!_player->playloop_step())
    {
        _timer->stop();
    }
}

void main_window::open(QStringList filenames, bool automatic)
{
    _player->force_stop();
    _player->close();
    _init_data.filenames.clear();
    for (int i = 0; i < filenames.size(); i++)
    {
        _init_data.filenames.push_back(filenames[i].toLocal8Bit().constData());
    }
    if (automatic)
    {
        _init_data.input_mode = input::automatic;
        _init_data.video_mode = video_output::automatic;
    }
    if (open_player())
    {
        _settings->beginGroup("Video");
        if (_init_data.filenames.size() == 1)
        {
            QString fallback_mode_name = QString(input::mode_name(_player->input_mode()).c_str());
            QString mode_name = _settings->value(current_file_hash(), fallback_mode_name).toString();
            _init_data.input_mode = input::mode_from_name(mode_name.toStdString());
        }
        else
        {
            _init_data.input_mode = _player->input_mode();
        }
        _init_data.audio_stream = QVariant(_settings->value(current_file_hash() + "-audio-stream", QString("1")).toString()).toInt() - 1;
        _init_data.video_state.swap_eyes = QVariant(_settings->value(current_file_hash() + "-swap-eyes", QString("false")).toString()).toBool();
        _init_data.video_state.parallax = QVariant(_settings->value(current_file_hash() + "-parallax", QString("0")).toString()).toFloat();
        _init_data.video_state.ghostbust = QVariant(_settings->value(current_file_hash() + "-ghostbust", QString("0")).toString()).toFloat();
        _settings->endGroup();
        _settings->beginGroup("Session");
        if (automatic)
        {
            if (_init_data.input_mode == input::mono)
            {
                QString fallback_mode_name = QString(video_output::mode_name(video_output::mono_left).c_str());
                QString mode_name = _settings->value(QString("2d-output-mode"), fallback_mode_name).toString();
                _init_data.video_mode = video_output::mode_from_name(mode_name.toStdString());
            }
            else
            {
                QString fallback_mode_name = QString(video_output::mode_name(video_output::anaglyph_red_cyan_dubois).c_str());
                QString mode_name = _settings->value(QString("3d-output-mode"), fallback_mode_name).toString();
                _init_data.video_mode = video_output::mode_from_name(mode_name.toStdString());
            }
        }
        else
        {
            _init_data.video_mode = _player->video_mode();
        }
        _settings->endGroup();
        _in_out_widget->update(_init_data, true, false);
        _controls_widget->update(_init_data, true, false);
    }
    else
    {
        _in_out_widget->update(_init_data, false, false);
        _controls_widget->update(_init_data, false, false);
    }
}

void main_window::file_open()
{
    QFileDialog *file_dialog = new QFileDialog(this);
    _settings->beginGroup("Session");
    file_dialog->setDirectory(_settings->value("file-open-dir", QDir::currentPath()).toString());
    _settings->endGroup();
    file_dialog->setWindowTitle("Open up to three files");
    file_dialog->setAcceptMode(QFileDialog::AcceptOpen);
    file_dialog->setFileMode(QFileDialog::ExistingFiles);
    if (!file_dialog->exec())
    {
        return;
    }
    QStringList file_names = file_dialog->selectedFiles();
    if (file_names.empty())
    {
        return;
    }
    if (file_names.size() > 3)
    {
        QMessageBox::critical(this, "Error", "Cannot open more than 3 files");
        return;
    }
    _settings->beginGroup("Session");
    _settings->setValue("file-open-dir", file_dialog->directory().path());
    _settings->endGroup();
    open(file_names, true);
}

void main_window::file_open_url()
{
    QDialog *url_dialog = new QDialog(this);
    url_dialog->setWindowTitle("Open URL");
    QLabel *url_label = new QLabel("URL:");
    QLineEdit *url_edit = new QLineEdit("");
    url_edit->setMinimumWidth(256);
    QPushButton *ok_btn = new QPushButton("OK");
    QPushButton *cancel_btn = new QPushButton("Cancel");
    connect(ok_btn, SIGNAL(pressed()), url_dialog, SLOT(accept()));
    connect(cancel_btn, SIGNAL(pressed()), url_dialog, SLOT(reject()));
    QGridLayout *layout = new QGridLayout();
    layout->addWidget(url_label, 0, 0);
    layout->addWidget(url_edit, 0, 1, 1, 3);
    layout->addWidget(ok_btn, 2, 2);
    layout->addWidget(cancel_btn, 2, 3);
    layout->setColumnStretch(1, 1);
    url_dialog->setLayout(layout);
    url_dialog->exec();
    if (url_dialog->result() == QDialog::Accepted
            && !url_edit->text().isEmpty())
    {
        QString url = url_edit->text();
        open(QStringList(url), true);
    }
}

void main_window::preferences_crosstalk()
{
    QDialog *dialog = new QDialog(this);
    dialog->setModal(true);
    dialog->setWindowTitle("Set crosstalk levels");
    QGridLayout *layout = new QGridLayout;
    QLabel *rtfm_label = new QLabel(
            "<p>Please read the manual to find out<br>"
            "how to measure the crosstalk levels<br>"
            "of your display.</p>");
    layout->addWidget(rtfm_label, 0, 0, 1, 6);
    QLabel *red_label = new QLabel("Red:");
    layout->addWidget(red_label, 1, 0, 1, 2);
    QSpinBox *red_spinbox = new QSpinBox();
    red_spinbox->setRange(0, 100);
    red_spinbox->setValue(_init_data.video_state.crosstalk_r * 100.0f);
    layout->addWidget(red_spinbox, 1, 2, 1, 4);
    QLabel *green_label = new QLabel("Green:");
    layout->addWidget(green_label, 2, 0, 1, 3);
    QSpinBox *green_spinbox = new QSpinBox();
    green_spinbox->setRange(0, 100);
    green_spinbox->setValue(_init_data.video_state.crosstalk_g * 100.0f);
    layout->addWidget(green_spinbox, 2, 2, 1, 4);
    QLabel *blue_label = new QLabel("Blue:");
    layout->addWidget(blue_label, 3, 0, 1, 2);
    QSpinBox *blue_spinbox = new QSpinBox();
    blue_spinbox->setRange(0, 100);
    blue_spinbox->setValue(_init_data.video_state.crosstalk_b * 100.0f);
    layout->addWidget(blue_spinbox, 3, 2, 1, 4);
    QPushButton *ok_button = new QPushButton(tr("&OK"));
    ok_button->setDefault(true);
    connect(ok_button, SIGNAL(clicked()), dialog, SLOT(accept()));
    layout->addWidget(ok_button, 4, 0, 1, 3);
    QPushButton *cancel_button = new QPushButton(tr("&Cancel"), dialog);
    connect(cancel_button, SIGNAL(clicked()), dialog, SLOT(reject()));
    layout->addWidget(cancel_button, 4, 3, 1, 3);
    dialog->setLayout(layout);
    if (dialog->exec() == QDialog::Rejected)
    {
        return;
    }
    _init_data.video_state.crosstalk_r = red_spinbox->value() / 100.0f;
    _init_data.video_state.crosstalk_g = green_spinbox->value() / 100.0f;
    _init_data.video_state.crosstalk_b = blue_spinbox->value() / 100.0f;
    delete dialog;
}

void main_window::help_manual()
{
    QUrl manual_url;
#if defined(Q_WS_WIN32)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../doc/bino.html");
#elif defined(Q_WS_MAC)
    manual_url = QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + "/../Resources/Bino Help/bino.html");
#else
    manual_url = QUrl::fromLocalFile(DOCDIR "/bino.html");
#endif
    if (!QDesktopServices::openUrl(manual_url))
    {
        QMessageBox::critical(this, "Error", "Cannot open manual");
    }
}

void main_window::help_website()
{
    if (!QDesktopServices::openUrl(QUrl(PACKAGE_URL)))
    {
        QMessageBox::critical(this, "Error", "Cannot open website");
    }
}

void main_window::help_keyboard()
{
    QMessageBox::information(this, tr("Keyboard Shortcuts"), tr(
                "<p>Keyboard control:"
                "<table>"
                "<tr><td>q or ESC</td><td>Stop</td></tr>"
                "<tr><td>p or SPACE</td><td>Pause / unpause</td></tr>"
                "<tr><td>f</td><td>Toggle fullscreen</td></tr>"
                "<tr><td>c</td><td>Center window</td></tr>"
                "<tr><td>s</td><td>Swap left/right view</td></tr>"
                "<tr><td>1, 2</td><td>Adjust contrast</td></tr>"
                "<tr><td>3, 4</td><td>Adjust brightness</td></tr>"
                "<tr><td>5, 6</td><td>Adjust hue</td></tr>"
                "<tr><td>7, 8</td><td>Adjust saturation</td></tr>"
                "<tr><td><, ></td><td>Adjust parallax</td></tr>"
                "<tr><td>(, )</td><td>Adjust ghostbusting</td></tr>"
                "<tr><td>left, right</td><td>Seek 10 seconds backward / forward</td></tr>"
                "<tr><td>up, down</td><td>Seek 1 minute backward / forward</td></tr>"
                "<tr><td>page up, page down</td><td>Seek 10 minutes backward / forward</td></tr>"
                "</table>"
                "</p>"
                "<p>"
                "(Click into the video area to give it the keyboard focus if necessary.)"
                "</p>"));
}

void main_window::help_about()
{
    QString blurb = tr(
            "<p>%1 version %2</p>"
            "<p>Copyright (C) 2011 the Bino developers.<br>"
            "This is free software. You may redistribute copies of it<br>"
            "under the terms of the <a href=\"http://www.gnu.org/licenses/gpl.html\">"
            "GNU General Public License</a>.<br>"
            "There is NO WARRANTY, to the extent permitted by law.</p>"
            "See <a href=\"%3\">%3</a> for more information on this software.</p>")
        .arg(PACKAGE_NAME).arg(VERSION).arg(PACKAGE_URL);
    blurb += tr("<p>Platform:<ul><li>%1</li></ul></p>").arg(PLATFORM);
    blurb += QString("<p>Libraries used:<ul>");
    std::vector<std::string> libs = lib_versions();
    for (size_t i = 0; i < libs.size(); i++)
    {
        blurb += tr("<li>%1</li>").arg(libs[i].c_str());
    }
    blurb += QString("</ul></p>");
    QMessageBox::about(this, tr("About " PACKAGE_NAME), blurb);
}

bool main_window::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::FileOpen)
    {
        open(QStringList(static_cast<QFileOpenEvent *>(event)->file()), true);
        return true;
    }
    else
    {
        // pass the event on to the parent class
        return QMainWindow::eventFilter(obj, event);
    }
}

player_qt::player_qt() : player(player::slave)
{
    _qt_app_owner = init_qt();
    QCoreApplication::setOrganizationName(PACKAGE_NAME);
    QCoreApplication::setApplicationName(PACKAGE_NAME);
    _settings = new QSettings;
}

player_qt::~player_qt()
{
    if (_qt_app_owner)
    {
        exit_qt();
    }
    delete _settings;
}

void player_qt::open(const player_init_data &init_data)
{
    msg::set_level(init_data.log_level);
    _main_window = new main_window(_settings, init_data);
}

void player_qt::run()
{
    exec_qt();
    delete _main_window;
    _main_window = NULL;
}

void player_qt::close()
{
}
