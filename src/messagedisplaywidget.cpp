/*
    Copyright (C) 2013 by Martin Kröll <technikschlumpf@web.de>

    This file is part of Tox Qt GUI.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    See the COPYING file for more details.
*/

#include "messagedisplaywidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTime>
#include <QScrollBar>
#include <QRegularExpression>
#include <QFrame>
#include <QPropertyAnimation>
#include <QFontDatabase>

#include "Settings/settings.hpp"
#include "smileypack.hpp"
#include "elidelabel.hpp"
#include "messagelabel.hpp"
#include "opacitywidget.hpp"

MessageDisplayWidget::MessageDisplayWidget(QWidget *parent) :
    QScrollArea(parent),
    lastMessageIsOurs(true) // the initial value doesn't matter, just silencing Valgrind
{
    // Scroll down on new Message
    QScrollBar* scrollbar = verticalScrollBar();
    connect(scrollbar, &QScrollBar::rangeChanged, this, &MessageDisplayWidget::moveScrollBarToBottom, Qt::UniqueConnection);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);

    QWidget *widget = new QWidget(this);
    widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setWidget(widget);

    mainlayout = new QVBoxLayout(widget);
    mainlayout->setSpacing(1);
    mainlayout->setContentsMargins(1,1,1,1);

    // Animation
    if (Settings::getInstance().isAnimationEnabled())
    {
        animation = new QPropertyAnimation(this, "scrollPos");
        animation->setDuration(200);
        animation->setLoopCount(1);
        mainlayout->setMargin(1);
    }
}

void MessageDisplayWidget::appendMessage(const QString &name, const QString &message/*, const QString &timestamp*/, int messageId, bool isOur)
{
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &MessageDisplayWidget::moveScrollBarToBottom, Qt::UniqueConnection);
    QWidget *row = createNewRow(name, message, messageId, isOur);
    mainlayout->addWidget(row);
}

void MessageDisplayWidget::prependMessage(const QString &name, const QString &message/*, const QString &timestamp*/, int messageId, bool isOur)
{
    disconnect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &MessageDisplayWidget::moveScrollBarToBottom);
    mainlayout->insertWidget(0, createNewRow(name, message, messageId, isOur));
}

void MessageDisplayWidget::appendAction(const QString &name, const QString &message, bool isOur)
{
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &MessageDisplayWidget::moveScrollBarToBottom, Qt::UniqueConnection);
    QWidget *row = createNewRow("*", name+" "+message, -2, isOur);
    mainlayout->addWidget(row);
}

int MessageDisplayWidget::scrollPos() const
{
    return mScrollPos;
}

void MessageDisplayWidget::setScrollPos(int arg)
{
    mScrollPos = arg;
    verticalScrollBar()->setSliderPosition(arg);
}

void MessageDisplayWidget::moveScrollBarToBottom(int min, int max)
{
    Q_UNUSED(min);
    if (Settings::getInstance().isAnimationEnabled())
    {
        animation->setKeyValueAt(0, verticalScrollBar()->sliderPosition());
        animation->setKeyValueAt(1, max);
        animation->start();
    }
    else
    {
        this->verticalScrollBar()->setValue(max);
    }
}

QString MessageDisplayWidget::urlify(QString string)
{
    return string.replace(QRegularExpression("((?:https?|ftp)://\\S+)"), "<a href=\"\\1\">\\1</a>");
}

QWidget *MessageDisplayWidget::createNewRow(const QString &name, const QString &message/*, const QString &timestamp*/, int messageId, bool isOur)
{
    OpacityWidget *widget = new OpacityWidget(this);
    widget->setProperty("class", "msgRow"); // for CSS styling

    ElideLabel *nameLabel = new ElideLabel(widget);
    nameLabel->setMaximumWidth(50);
    nameLabel->setTextElide(true);
    nameLabel->setTextElideMode(Qt::ElideRight);
    nameLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    nameLabel->setShowToolTipOnElide(true);
    nameLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignTop);

    MessageLabel *messageLabel = new MessageLabel(widget);
    messageLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignTop);
    QString messageText = Smileypack::smilify(urlify(message.toHtmlEscaped())).replace('\n', "<br>");
    // Action
    if (messageId < -1) {
        QPalette actionPal;
        actionPal.setColor(QPalette::Foreground, Qt::darkGreen);
        messageLabel->setPalette(actionPal);
        messageLabel->setProperty("class", "msgAction"); // for CSS styling
        messageLabel->setText(QString("<i>%1</i>").arg(messageText));
    }
    // Message
    else if (messageId != 0) {
        messageLabel->setMessageId(messageId);
        messageLabel->setProperty("class", "msgMessage"); // for CSS styling
        messageLabel->setText(messageText);
    }
    // Error
    else {
        QPalette errorPal;
        errorPal.setColor(QPalette::Foreground, Qt::red);
        messageLabel->setPalette(errorPal);
        messageLabel->setProperty("class", "msgError"); // for CSS styling
        messageLabel->setText(QString("<img src=\":/icons/error.png\" /> %1").arg(messageText));
        messageLabel->setToolTip(tr("Couldn't send the message!"));
    }

    QLabel *timeLabel = new QLabel(widget);
    timeLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    timeLabel->setForegroundRole(QPalette::Mid);
    timeLabel->setProperty("class", "msgTimestamp"); // for CSS styling
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignTop | Qt::AlignTrailing);
    timeLabel->setText(QTime::currentTime().toString("hh:mm:ss"));

    // Insert name if sender changed.
    if (lastMessageIsOurs != isOur || mainlayout->count() < 1 || messageId < -1) {
        nameLabel->setText(name);

        if (isOur) {
            nameLabel->setForegroundRole(QPalette::Mid);
            nameLabel->setProperty("class", "msgUserName"); // for CSS styling
        } else {
            nameLabel->setProperty("class", "msgFriendName"); // for CSS styling
        }

        // Create line
        if (lastMessageIsOurs != isOur && mainlayout->count() > 0) {
            QFrame *line = new QFrame(this);
            line->setFrameShape(QFrame::HLine);
            line->setFrameShadow(QFrame::Plain);
            line->setForegroundRole(QPalette::Midlight);
            line->setProperty("class", "msgLine"); // for CSS styling
            mainlayout->addWidget(line);
        }

        lastMessageIsOurs = isOur;
    }

    // Return new line
    QHBoxLayout *hlayout = new QHBoxLayout(widget);
    hlayout->setContentsMargins(0, 0, 0, 0);
    hlayout->setMargin(0);
    hlayout->addWidget(nameLabel, 0, Qt::AlignTop);
    hlayout->addWidget(messageLabel, 0, Qt::AlignTop);
    hlayout->addWidget(timeLabel, 0, Qt::AlignTop);
    widget->setLayout(hlayout);

    return widget;
}
