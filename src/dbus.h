#ifndef PSI_DBUS_H
#define PSI_DBUS_H

#include "psicon.h"

#define PSIDBUSNAME "ru.yandex.Online"
// interface name is duplicated in dbus.cpp
#define PSIDBUSMAINIF "ru.yandex.Online.Main"

bool dbusInit(const QString profile);

void addPsiConAdapter(PsiCon *psicon);

#endif
