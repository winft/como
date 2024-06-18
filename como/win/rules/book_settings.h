/*
    SPDX-FileCopyrightText: 2020 Henri Chain <henri.chain@enioka.com>
    SPDX-FileCopyrightText: 2020 Roman Gilg <subdiff@gmail.com>
    SPDX-FileCopyrightText: 2021 Ismael Asensio <isma.af@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef KWIN_RULES_RULE_BOOK_SETTINGS_H
#define KWIN_RULES_RULE_BOOK_SETTINGS_H

#include "rules_book_settings_base.h"

#include <KSharedConfig>
#include <como_export.h>
#include <deque>
#include <vector>

namespace como::win::rules
{

class ruling;
class settings;

class COMO_EXPORT book_settings : public book_settings_base
{
public:
    book_settings(KSharedConfig::Ptr config, QObject* parent = nullptr);
    book_settings(QObject* parent = nullptr);
    ~book_settings();

    void setRules(std::vector<ruling*> const&);
    std::deque<ruling*> rules() const;

    bool usrSave() override;
    void usrRead() override;
    bool usrIsSaveNeeded() const;

    size_t ruleCount() const;
    std::optional<size_t> indexForId(QString const& id) const;
    settings* ruleSettingsAt(size_t row) const;
    settings* insertRuleSettingsAt(size_t row);
    void removeRuleSettingsAt(size_t row);
    void moveRuleSettings(size_t srcRow, size_t destRow);

private:
    static QString generateGroupName();

    std::deque<settings*> m_list;
    QStringList m_storedGroups;
};

}

#endif
