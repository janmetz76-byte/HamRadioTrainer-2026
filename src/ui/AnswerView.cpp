// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#include "AnswerView.h"

#include "QuestionText.h"

#include "ImageLoader.h"
#include "core/LatexFormatter.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QLayout>
#include <QPainter>
#include <QRadioButton>

namespace {

/// Bild, das selbst antwortet.
///
/// QLabel hat erst ab Qt 6.9 ein clicked-Signal. Damit die Bildantworten
/// unabhaengig von der Qt-Version anklickbar sind, faengt diese Klasse den
/// Mausklick selbst ab.
class ClickableImageLabel : public QLabel
{
    Q_OBJECT

public:
    using QLabel::QLabel;

signals:
    void activated();

protected:
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()))
            emit activated();
        QLabel::mouseReleaseEvent(event);
    }
};

} // namespace

namespace hrt {
namespace {

/// Marks a control row as right or wrong once the solution has been shown.
void tintRow(QWidget *row, const QColor &colour)
{
    QPalette palette = row->palette();
    palette.setColor(QPalette::Window, colour);
    row->setAutoFillBackground(true);
    row->setPalette(palette);
}

/// Takes the colour off a row again.
///
/// Setting a QPalette would not do: the palette belongs to the whole row, and
/// leaving the copied one behind would carry the tint into the next question
/// that reuses the widget. Only the two things tintRow() switched have to be
/// switched off again.
void untintRow(QWidget *row)
{
    row->setAutoFillBackground(false);
    QPalette palette = row->palette();
    palette.setColor(QPalette::Window, row->parentWidget()->palette().color(QPalette::Window));
    row->setPalette(palette);
}

/// The colour of a chosen answer.
///
/// Blau, nicht die Farbe der Loesung: nach dem Zeigen der Loesung wird die
/// Zeile gruen oder rot, und eine ausgewaehlte Zeele, die vorher blau war,
/// muss sich nicht erst ein Schritt spaeter erklaeren lassen.
constexpr int AUSWAHL_R = 0xdd;
constexpr int AUSWAHL_G = 0xe8;
constexpr int AUSWAHL_B = 0xf5;

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

    for (QLabel *badge : m_badges)
        badge->deleteLater();
    m_badges.clear();

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
        // Ohne Text bleibt nur die Nummer als Fuehlfuellung, sonst haette der
        // Knopf gar keinen Text und keinen Tooltip.
        const QString answerHint = answer ? answer->text.simplified() : QString();
        control->setToolTip(answerHint.isEmpty()
                                ? QObject::tr("Antwort %1").arg(position + 1)
                                : answerHint);
        // Der Buchstabe steht links vor der Auswahl und nicht im Text der
        // Antwort: eine Antwort, die selbst mit "A." beginnt, oder eine mit
        // einem Bild, vertrage keinen zusaetzlichen Anhang am Text. So
        // benennt er jede Antwort gleichermassen, auch die ohne Text.
        auto *badge = new QLabel(row);
        badge->setFixedSize(24, 24);
        badge->setAlignment(Qt::AlignCenter);
        badge->setText(QString(QChar(QLatin1Char('A').unicode() + position)));
        // Ueber die Palette statt ueber feste Farben, sonst waere das Kästchen
        // in einem dunklen Fenster ein heller Fleck.
        badge->setStyleSheet(QStringLiteral(
            "QLabel { border: 1px solid palette(mid); border-radius: 4px;"
            " padding: 0px; font-weight: bold; }"));
        rowLayout->addWidget(badge, 0, Qt::AlignTop);
        m_badges.append(badge);

        rowLayout->addWidget(control, 0, Qt::AlignTop);
        if (auto *radio = qobject_cast<QRadioButton *>(control))
            group->addButton(radio, position);
        else
            group->addButton(control, position);

        const QString answerText = answer
                                        ? answer->text
                                        : QObject::tr("Antwort %1").arg(position + 1);

        // Bei bildbehafteten Antworten ist der Text leer. Ein leeres Label
        // mit Stretzfaktor wuerde trotzdem Platz beanspruchen und das Bild
        // von der Auswahl wegschieben - genau dann, wo man es anklicken muss.
        const QString plainText = answerText.simplified();
        if (!plainText.isEmpty()) {
            auto *label = new QLabel(row);
            label->setWordWrap(true);
            setQuestionText(label, plainText, question->textFormat);
            rowLayout->addWidget(label, 1);
        }

        const QPixmap answerImage = answer ? renderImage(answer->image) : QPixmap();
        if (!answerImage.isNull()) {
            auto *imageLabel = new ClickableImageLabel(row);
            imageLabel->setPixmap(answerImage);
            imageLabel->setAlignment(Qt::AlignLeft);
            // Ohne Text davor entfaellt der Einzug, der den Abstand zur
            // Auswahl herstellt.
            imageLabel->setIndent(plainText.isEmpty() ? 0 : 24);
            imageLabel->setToolTip(QObject::tr("Antwort %1").arg(position + 1));
            rowLayout->addWidget(imageLabel, 1);

            // Das Bild ist bei diesen Fragen das einzige, was man treffen
            // kann. Es muss selbst antworten, sonst muesste man auf einen
            // fingerbreiten Punkt links daneben zielen.
            imageLabel->setCursor(Qt::PointingHandCursor);
            connect(imageLabel, &ClickableImageLabel::activated, control,
                    [control] { control->click(); });
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

    // Die ganze Zeile faerbt sich mit der Auswahl, nicht nur der Punkt links.
    // Ein Klick auf den Antworttext soll als Treffer gelten - bei einer
    // mehrzeiligen Antwort ist der Punkt winzig daneben, und das Bild einer
    // Bildantwort hat gar keinen.
    const auto zeilenFarben = [this] {
        for (int position = 0; position < m_controls.size(); ++position) {
            QWidget *row = m_controls.at(position)->parentWidget();
            if (m_controls.at(position)->isChecked())
                tintRow(row, QColor(AUSWAHL_R, AUSWAHL_G, AUSWAHL_B));
            else
                untintRow(row);
        }
    };
    for (QAbstractButton *control : std::as_const(m_controls))
        connect(control, &QAbstractButton::toggled, this, zeilenFarben);

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

QStringList AnswerView::badges() const
{
    QStringList briefe;
    for (QLabel *badge : m_badges)
        briefe.append(badge->text());
    return briefe;
}

void AnswerView::setAnswersEnabled(bool enabled)
{
    for (QAbstractButton *control : m_controls)
        control->setEnabled(enabled);
}

} // namespace hrt

#include "AnswerView.moc"