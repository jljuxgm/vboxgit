/* $Id: UIWizardPage.cpp 41021 2012-04-23 11:02:30Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardPage class implementation
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes: */
#include <QAbstractButton>

/* Local includes: */
#include "UIWizardPage.h"
#include "UIWizard.h"
#include "VBoxGlobal.h"

UIWizard* UIWizardPageBase::wizardImp()
{
    /* Should be reimplemented in sub-class to enable access to wizard! */
    AssertMsgFailed(("UIWizardPageBase::wizardImp() should be reimplemented!"));
    return 0;
}

UIWizardPage* UIWizardPageBase::thisImp()
{
    /* Should be reimplemented in sub-class to enable access to wizard page! */
    AssertMsgFailed(("UIWizardPageBase::thisImp() should be reimplemented!"));
    return 0;
}

QVariant UIWizardPageBase::fieldImp(const QString &) const
{
    /* Should be reimplemented in sub-class to enable access to wizard field! */
    AssertMsgFailed(("UIWizardPageBase::fieldImp(const QString &) should be reimplemented!"));
    return QVariant();
}

UIWizardPage::UIWizardPage()
    : m_fReady(false)
{
}

void UIWizardPage::markReady()
{
    m_fReady = true;
    QWizardPage::setTitle(m_strTitle);
}

void UIWizardPage::setTitle(const QString &strTitle)
{
    m_strTitle = strTitle;
    if (m_fReady)
        QWizardPage::setTitle(m_strTitle);
}

UIWizard* UIWizardPage::wizard() const
{
    return qobject_cast<UIWizard*>(QWizardPage::wizard());
}

QString UIWizardPage::standardHelpText() const
{
    return tr("Use the <b>%1</b> button to go to the next page of the wizard and the "
              "<b>%2</b> button to return to the previous page. "
              "You can also press <b>%3</b> if you want to cancel the execution "
              "of this wizard.</p>")
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::NextButton).remove(" >"))))
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::BackButton).remove("< "))))
#ifdef Q_WS_MAC
        .arg(QKeySequence("ESC").toString()); /* There is no button shown on Mac OS X, so just say the key sequence. */
#else /* Q_WS_MAC */
        .arg(VBoxGlobal::replaceHtmlEntities(VBoxGlobal::removeAccelMark(wizard()->buttonText(QWizard::CancelButton))));
#endif /* Q_WS_MAC */
}

void UIWizardPage::startProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(false);
}

void UIWizardPage::endProcessing()
{
    if (isFinalPage())
        wizard()->button(QWizard::FinishButton)->setEnabled(true);
}

