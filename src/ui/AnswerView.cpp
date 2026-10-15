// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "AnswerView.h"

#include "ImageLoader.h"
#include "core/LatexFormatter.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QRadioButton>

namespace hrt {
namespace {

/// Puts a text into a label, rendering the math when the question carries it.
///
/// A QLabel in AutoText mode treats a string that looks like markup as markup,
/// so plain questions are switched to PlainText explicitly. Otherwise a
/// question containing a less-than sign would be swallowed.
void setQuestionText(QLabel *label, const QString &text, TextFormat format)
{
    if (format == TextFormat::Latex) {
        label->setTextFormat(Qt::RichText);
        label->setText(LatexFormatter::toRichText(text));
        return;
    }
    label->setTextFormat(Qt::PlainText);
    label->setText(text);
}

/// Marks a control row as right or wrong once the solution has been shown.
void tintRow(QWidget *row, const QColor &colour)
{
    QPalette palette = row->palette();
    palette.setColor(QPalette::Window, colour);
    row->setAutoFillBackground(true);
    row->setPalette(palette);
}

} // namespace

AnswerView::AnswerView(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(6);

    m_text = new QLabel(this);
    m_text->setTextFormat(Qt::PlainText);
    m_text->setWordWrap(true);
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_layout->addWidget(m_text);
}

QPixmap AnswerView::renderImage(const QString &relativePath) const
{
    return images::load(m_catalogDir, relativePath);
}

void AnswerView::clear()
{
    m_selection = -1;
    m_answerCount = 0;
    m_order.clear();
    m_question = nullptr;

    for (QAbstractButton *control : m_controls)
        control->deleteLater();
    m_controls.clear();

    for (QLabel *mark : m_marks)
        mark->deleteLater();
    m_marks.clear();

    // The question text stays at index 0. Starting the walk at index 1 is what
    // keeps it in the layout: re-adding it inside the loop would make takeAt
    // hand out the very same item again and never end.
    while (m_layout->count() > 1) {
        QLayoutItem *item = m_layout->takeAt(1);
        if (QWidget *widget = item->widget()) {
            widget->hide();
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    m_text->clear();
    m_text->setTextFormat(Qt::PlainText);
    m_text->hide();
}

void AnswerView::setQuestion(const Question *question, const QVector<int> &order,
                             const QDir &catalogDir)
{
    clear();
    if (!question || order.isEmpty())
        return;

    m_question = question;
    m_order = order;
    m_catalogDir = catalogDir;
    m_answerCount = order.size();

    setQuestionText(m_text, question->text.simplified(), question->textFormat);
    m_text->show();

    const QPixmap questionImage = renderImage(question->image);
    if (!questionImage.isNull()) {
        auto *label = new QLabel(this);
        label->setPixmap(questionImage);
        label->setAlignment(Qt::AlignLeft);
        m_layout->addWidget(label);
    }

    auto *group = new QButtonGroup(this);
    group->setExclusive(!question->isMultipleChoice());

    for (int position = 0; position < order.size(); ++position) {
        const int answerIndex = order.at(position);
        const Answer *answer = answerIndex >= 0 && answerIndex < question->answers.size()
                                   ? &question->answers.at(answerIndex)
                                   : nullptr;

        auto *row = new QWidget(this);
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(2, 2, 2, 2);
        rowLayout->setSpacing(6);

        QAbstractButton *control = question->isMultipleChoice()
                                       ? static_cast<QAbstractButton *>(new QCheckBox(row))
                                       : static_cast<QAbstractButton *>(new QRadioButton(row));
        control->setText(QString());
        control->setToolTip(answer ? answer->text
                                   : QObject::tr("Antwort %1").arg(position + 1));
        rowLayout->addWidget(control, 0, Qt::AlignTop);
        if (auto *radio = qobject_cast<QRadioButton *>(control))
            group->addButton(radio, position);
        else
            group->addButton(control, position);

        const QString answerText = answer
                                        ? answer->text
                                        : QObject::tr("Antwort %1").arg(position + 1);
        auto *label = new QLabel(row);
        label->setWordWrap(true);
        setQuestionText(label, answerText.simplified(), question->textFormat);
        rowLayout->addWidget(label, 1);

        const QPixmap answerImage = answer ? renderImage(answer->image) : QPixmap();
        if (!answerImage.isNull()) {
            auto *imageLabel = new QLabel(row);
            imageLabel->setPixmap(answerImage);
            imageLabel->setAlignment(Qt::AlignLeft);
            imageLabel->setIndent(24);
            rowLayout->addWidget(imageLabel, 1);
        }

        // The mark belongs at the end of the row. Without adding it to the
        // layout it would float over the selection control at position 0,0.
        auto *mark = new QLabel(row);
        mark->setFixedWidth(24);
        mark->setAlignment(Qt::AlignCenter);
        mark->hide();
        rowLayout->addWidget(mark, 0);

        m_layout->addWidget(row);
        m_controls.append(control);
        m_marks.append(mark);
    }

    m_layout->addStretch(1);

    if (question->isMultipleChoice()) {
        connect(group, &QButtonGroup::buttonToggled, this, [this, group](QAbstractButton *button,
                                                                          bool on) {
            if (!on)
                return;
            int marked = 0;
            for (QAbstractButton *control : std::as_const(m_controls)) {
                if (auto *box = qobject_cast<QCheckBox *>(control); box && box->isChecked())
                    ++marked;
            }
            m_selection = marked > 0 ? group->id(button) : -1;
            emit selected(m_selection);
        });
    } else {
        connect(group, &QButtonGroup::buttonClicked, this, [this, group](QAbstractButton *button) {
            m_selection = group->id(button);
            emit selected(m_selection);
        });
    }

    // The rows were built after the widget became visible, so the layout has
    // to be activated explicitly, otherwise they stay invisible.
    m_layout->activate();
    for (int i = 0; i < m_layout->count(); ++i) {
        if (QWidget *widget = m_layout->itemAt(i)->widget())
            widget->show();
    }
}

void AnswerView::setSelection(int position)
{
    m_selection = position;
    const QSignalBlocker blocker(this);
    if (position < 0 || position >= m_controls.size())
        return;
    QAbstractButton *control = m_controls.at(position);
    if (auto *box = qobject_cast<QCheckBox *>(control))
        box->setChecked(true);
    else if (auto *radio = qobject_cast<QRadioButton *>(control))
        radio->setChecked(true);
}

void AnswerView::reveal(int correctPosition, int selectedPosition)
{
    if (m_answerCount <= 0)
        return;

    // The mark labels the answer, so the row is where the colour belongs.
    for (int position = 0; position < m_controls.size(); ++position) {
        QAbstractButton *control = m_controls.at(position);
        control->setEnabled(false);
        QWidget *row = control->parentWidget();
        if (position == correctPosition) {
            if (auto *mark = m_marks.value(position)) {
                mark->setText(QStringLiteral("✓"));
                mark->show();
            }
            tintRow(row, QColor(0xdd, 0xf5, 0xdd));
        } else if (position == selectedPosition) {
            if (auto *mark = m_marks.value(position)) {
                mark->setText(QStringLiteral("✗"));
                mark->show();
            }
            tintRow(row, QColor(0xfb, 0xdd, 0xdd));
        }
    }
}

void AnswerView::setAnswersEnabled(bool enabled)
{
    for (QAbstractButton *control : m_controls)
        control->setEnabled(enabled);
}

} // namespace hrt

#include "AnswerView.moc"