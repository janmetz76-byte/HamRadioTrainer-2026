// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Jan - DC6DX and HamRadioTrainer 2026 contributors
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any later version.
#pragma once

#include "core/Catalog.h"
#include "learning/ProgressStore.h"
#include "learning/Scheduler.h"

#include <QDate>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

namespace hrt {

class ExamRound;
class StudyRound;

/// The phone interface's view of the trainer: catalog, learning progress,
/// the chosen examination and the figures derived from them.
///
/// It reads and writes the same settings and the same progress file as the
/// desktop window, so a learner who switches between the two finds the same
/// state. Everything shown is handed to QML as plain values; the widgets of
/// the desktop interface are not involved.
class Trainer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool setupDone READ setupDone NOTIFY setupChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY setupChanged)
    Q_PROPERTY(QString combinationId READ combinationId NOTIFY setupChanged)
    Q_PROPERTY(QString combinationLabel READ combinationLabel NOTIFY setupChanged)
    Q_PROPERTY(QString partsText READ partsText NOTIFY setupChanged)
    Q_PROPERTY(QDate examDate READ examDate NOTIFY setupChanged)
    Q_PROPERTY(int daysToExam READ daysToExam NOTIFY setupChanged)
    Q_PROPERTY(QVariantMap statistics READ statistics NOTIFY progressChanged)
    Q_PROPERTY(hrt::StudyRound *study READ study CONSTANT)
    Q_PROPERTY(hrt::ExamRound *exam READ exam CONSTANT)

public:
    explicit Trainer(QObject *parent = nullptr);
    ~Trainer() override;

    bool setupDone() const { return m_setupDone; }
    QString problem() const { return m_problem; }
    QString combinationId() const { return m_combinationId; }
    QString combinationLabel() const;
    QString partsText() const;
    QDate examDate() const { return m_examDate; }
    /// Days until the examination, -1 when no date is set.
    int daysToExam() const;
    QVariantMap statistics() const;
    StudyRound *study() const { return m_study; }
    ExamRound *exam() const { return m_exam; }

    /// The six cases of the regulation, with what each means for this
    /// installation: whether its catalog is there and how many questions it
    /// asks.
    Q_INVOKABLE QVariantList combinations() const;
    /// Applies the choice of the setup page. An invalid date means none.
    Q_INVOKABLE void applySetup(const QString &combinationId, const QDate &examDate);
    /// A date three months ahead, as a sensible default for the setup page.
    Q_INVOKABLE QDate suggestedExamDate() const;

    // Katalog zum Durchblaettern: Teil, Abschnitt, Frage. Nur die Teile der
    // gewaehlten Pruefung, wie beim Lernen.
    Q_INVOKABLE QVariantList catalogParts() const;
    Q_INVOKABLE QVariantList catalogSections(int part) const;
    Q_INVOKABLE QVariantList catalogQuestions(int part, int section) const;
    /// One question in its original order, the correct answer marked, with
    /// what the learner has done with it so far.
    Q_INVOKABLE QVariantMap questionDetail(const QString &id) const;

    // For the rounds.
    const Catalog &catalog() const { return m_catalog; }
    Scheduler &scheduler() { return m_scheduler; }
    /// Prüfungsteile chosen in the setup, as indexes into the catalog.
    QSet<int> selectedParts() const;
    /// Writes the progress and tells the pages that the figures changed.
    void saveProgress();
    /// Question text and answers in the order given, ready for QML.
    QVariantMap presentQuestion(const Question &question, const QVector<int> &order) const;

signals:
    void setupChanged();
    void progressChanged();

private:
    struct CatalogInfo
    {
        QString path;
        QVector<int> questionsPerPart;
    };

    void findCatalogs();
    bool openCatalog(const QString &path);
    QUrl imageUrl(const QString &relativePath) const;

    QHash<QString, CatalogInfo> m_catalogs; ///< by exam class
    Catalog m_catalog;
    QString m_catalogPath;
    QString m_catalogDir;
    Scheduler m_scheduler;
    ProgressStore m_store{QString()};
    bool m_setupDone = false;
    QString m_problem;
    QString m_combinationId;
    QDate m_examDate;
    StudyRound *m_study = nullptr;
    ExamRound *m_exam = nullptr;
};

} // namespace hrt
