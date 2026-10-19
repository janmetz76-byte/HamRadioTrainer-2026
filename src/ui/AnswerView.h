// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"

#include <QDir>
#include <QWidget>

class QAbstractButton;
class QCheckBox;
class QLabel;
class QVBoxLayout;

namespace hrt {

/// Shows one question with its illustrations and the answers as selectable
/// controls.
///
/// The answer text is placed in a wrapping label next to the control, because
/// QRadioButton and QCheckBox cannot wrap their own text and would cut a long
/// answer off at the window edge. The order in which the answers appear is the
/// order the caller passes in, which is how a fixed source order is kept away
/// from the learner.
class AnswerView : public QWidget
{
    Q_OBJECT

public:
    explicit AnswerView(QWidget *parent = nullptr);

    /// Shows a question. "order" maps the displayed position to the index in
    /// Question::answers. Passing an empty order clears the view.
    void setQuestion(const Question *question, const QVector<int> &order,
                     const QDir &catalogDir);

    /// Position of the selected answer, or -1 when nothing is selected.
    int selection() const { return m_selection; }

    /// Marks the chosen and the correct answer after a round. Must be called
    /// after the learner committed, otherwise the solution would leak.
    void reveal(int correctPosition, int selectedPosition);

    /// Marks an answer as selected without emitting "selected", e.g. when a
    /// question the learner already answered is shown again.
    void setSelection(int position);

    /// Enables or disables every answer control, e.g. while the solution is
    /// being shown.
    void setAnswersEnabled(bool enabled);

    bool isEmpty() const { return m_answerCount == 0; }

    /// The letter shown in front of each answer, empty when there is no
    /// question. Lets a caller name an answer without knowing how the row is
    /// built.
    QStringList badges() const;

signals:
    /// Emitted with the displayed position of the newly selected answer.
    void selected(int position);

private:
    void clear();
    QPixmap renderImage(const QString &relativePath) const;

    const Question *m_question = nullptr;
    QVector<int> m_order;
    QDir m_catalogDir;
    int m_selection = -1;
    int m_answerCount = 0;

    QLabel *m_text = nullptr;
    QVBoxLayout *m_layout = nullptr;
    QVector<QAbstractButton *> m_controls;
    QVector<QLabel *> m_badges;
    /// Labels that carry the correct or wrong mark after a round. They are
    /// plain labels, not disabled buttons, so they do not show up in the
    /// keyboard order.
    QVector<QLabel *> m_marks;
};

} // namespace hrt