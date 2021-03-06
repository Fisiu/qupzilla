/* ============================================================
* GnomeKeyringPasswords - gnome-keyring support plugin for QupZilla
* Copyright (C) 2013  David Rosca <nowrep@gmail.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
* ============================================================ */
#ifndef GNOMEKEYRINGPASSWORDBACKEND_H
#define GNOMEKEYRINGPASSWORDBACKEND_H

#include <QVector>

#include "passwordbackends/passwordbackend.h"
#include "passwordmanager.h"

class GnomeKeyringPasswordBackend : public PasswordBackend
{
public:
    explicit GnomeKeyringPasswordBackend();

    QString name() const;

    QVector<PasswordEntry> getEntries(const QUrl &url);
    QVector<PasswordEntry> getAllEntries();

    void addEntry(const PasswordEntry &entry);
    bool updateEntry(const PasswordEntry &entry);
    void updateLastUsed(PasswordEntry &entry);

    void removeEntry(const PasswordEntry &entry);
    void removeAll();

private:
    void initialize();

    bool m_loaded;
    QVector<PasswordEntry> m_allEntries;
};

#endif // GNOMEKEYRINGPASSWORDBACKEND_H
