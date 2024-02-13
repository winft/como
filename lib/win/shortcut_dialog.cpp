/*
    SPDX-FileCopyrightText: 1999, 2000 Matthias Ettrich <ettrich@kde.org>
    SPDX-FileCopyrightText: 2003 Lubos Lunak <l.lunak@kde.org>
    SPDX-FileCopyrightText: 2022 Roman Gilg <subdiff@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "shortcut_dialog.h"

#include "ui_shortcut_dialog.h"

#include <QPushButton>

namespace como::win
{

shortcut_dialog::shortcut_dialog(const QKeySequence& cut)
    : _shortcut(cut)
    , m_ui{std::make_unique<Ui::ShortcutDialog>()}
{
    m_ui->setupUi(this);
    m_ui->keySequenceEdit->setKeySequence(cut);
    m_ui->warning->hide();

    // Listen to changed shortcuts
    connect(m_ui->keySequenceEdit,
            &QKeySequenceEdit::editingFinished,
            this,
            &shortcut_dialog::keySequenceChanged);
    connect(m_ui->clearButton, &QToolButton::clicked, [this] { _shortcut = QKeySequence(); });

    m_ui->keySequenceEdit->setFocus();
    setWindowFlags(Qt::Popup | Qt::X11BypassWindowManagerHint);
}

shortcut_dialog::~shortcut_dialog() = default;

void shortcut_dialog::accept()
{
    QKeySequence seq = shortcut();

    if (!seq.isEmpty()) {
        if (seq[0] == QKeyCombination(Qt::Key_Escape)) {
            reject();
            return;
        }
        if (seq[0] == QKeyCombination(Qt::Key_Space)
            || seq[0].keyboardModifiers() == Qt::NoModifier) {
            // clear
            m_ui->keySequenceEdit->clear();
            QDialog::accept();
            return;
        }
    }

    QDialog::accept();
}

void shortcut_dialog::done(int r)
{
    QDialog::done(r);
    Q_EMIT dialogDone(r == Accepted);
}

void shortcut_dialog::keySequenceChanged()
{
    activateWindow(); // where is the kbd focus lost? cause of popup state?
    QKeySequence seq = m_ui->keySequenceEdit->keySequence();
    if (_shortcut == seq)
        return; // don't try to update the same

    if (seq.isEmpty()) { // clear
        _shortcut = seq;
        return;
    }
    if (seq.count() > 1) {
        seq = QKeySequence(seq[0]);
        m_ui->keySequenceEdit->setKeySequence(seq);
    }

    Q_EMIT shortcut_changed(seq);
}

QKeySequence shortcut_dialog::shortcut() const
{
    return _shortcut;
}

void shortcut_dialog::allow_shortcut(QKeySequence const& seq)
{
    if (seq != m_ui->keySequenceEdit->keySequence()) {
        // Already changed again
        return;
    }

    m_ui->warning->hide();
    if (auto ok = m_ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setFocus();
    }

    _shortcut = seq;
}

void shortcut_dialog::reject_shortcut(QKeySequence const& seq,
                                      std::string const& action,
                                      std::string const& app)
{
    if (seq != m_ui->keySequenceEdit->keySequence()) {
        // Already changed again
        return;
    }

    auto const seq_string = seq.toString();

    m_ui->warning->setText(i18nc(
        "'%1' is a keyboard shortcut like 'ctrl+w'", "<b>%1</b> is already in use", seq_string));
    m_ui->warning->setToolTip(
        i18nc("keyboard shortcut '%1' is used by action '%2' in application '%3'",
              "<b>%1</b> is used by %2 in %3",
              seq_string,
              QString::fromStdString(action),
              QString::fromStdString(app)));
    m_ui->warning->show();

    m_ui->keySequenceEdit->setKeySequence(shortcut());
}

}
