/* $Id: UIWizardImportAppPageBasic2.cpp 40870 2012-04-11 15:44:29Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardImportAppPageBasic2 class implementation
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
#include <QVBoxLayout>

/* Local includes: */
#include "UIWizardImportAppPageBasic2.h"
#include "UIWizardImportApp.h"
#include "QIRichTextLabel.h"

UIWizardImportAppPageBasic2::UIWizardImportAppPageBasic2(const QString &strFileName)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pApplianceWidget = new UIApplianceImportEditorWidget(this);
            m_pApplianceWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
            m_pApplianceWidget->setFile(strFileName);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pApplianceWidget);

    /* Register ImportAppliancePointer class: */
    qRegisterMetaType<ImportAppliancePointer>();
    /* Register 'applianceWidget' field! */
    registerField("applianceWidget", this, "applianceWidget");
}

void UIWizardImportAppPageBasic2::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardImportApp::tr("Appliance Import Settings"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardImportApp::tr("These are the virtual machines contained in the appliance "
                                            "and the suggested settings of the imported VirtualBox machines. "
                                            "You can change many of the properties shown by double-clicking "
                                            "on the items and disable others using the check boxes below."));
}

void UIWizardImportAppPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

void UIWizardImportAppPageBasic2::cleanupPage()
{
    /* Rollback settings: */
    m_pApplianceWidget->restoreDefaults();
    /* Call to base-class: */
    UIWizardPage::cleanupPage();
}

bool UIWizardImportAppPageBasic2::validatePage()
{
    /* Import appliance: */
    startProcessing();
    bool fResult = qobject_cast<UIWizardImportApp*>(wizard())->importAppliance();
    endProcessing();
    return fResult;
}

