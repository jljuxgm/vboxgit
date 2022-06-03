/* $Id: UIWizardFirstRunPageBasic2.cpp 41372 2012-05-21 16:53:33Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardFirstRunPageBasic2 class implementation
 */

/*
 * Copyright (C) 2008-2012 Oracle Corporation
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
#include <QHBoxLayout>
#include <QGroupBox>

/* Local includes: */
#include "UIWizardFirstRunPageBasic2.h"
#include "UIWizardFirstRun.h"
#include "COMDefs.h"
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"
#include "QIRichTextLabel.h"

UIWizardFirstRunPage2::UIWizardFirstRunPage2(bool fBootHardDiskWasSet)
    : m_fBootHardDiskWasSet(fBootHardDiskWasSet)
{
}

void UIWizardFirstRunPage2::onOpenMediumWithFileOpenDialog()
{
    /* Get opened vboxMedium id: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(m_pMediaSelector->type(), thisImp());
    /* Update medium-combo if necessary: */
    if (!strMediumId.isNull())
        m_pMediaSelector->setCurrentItem(strMediumId);
}

QString UIWizardFirstRunPage2::id() const
{
    return m_pMediaSelector->id();
}

void UIWizardFirstRunPage2::setId(const QString &strId)
{
    m_pMediaSelector->setCurrentItem(strId);
}

UIWizardFirstRunPageBasic2::UIWizardFirstRunPageBasic2(const QString &strMachineId, bool fBootHardDiskWasSet)
    : UIWizardFirstRunPage2(fBootHardDiskWasSet)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        pMainLayout->setContentsMargins(8, 0, 8, 0);
        m_pLabel = new QIRichTextLabel(this);
        m_pSourceCnt = new QGroupBox(this);
        {
            m_pSourceCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pSourceCntLayout = new QHBoxLayout(m_pSourceCnt);
            {
                m_pMediaSelector = new VBoxMediaComboBox(m_pSourceCnt);
                {
                    m_pMediaSelector->setMachineId(strMachineId);
                    m_pMediaSelector->setType(VBoxDefs::MediumType_DVD);
                    m_pMediaSelector->repopulate();
                }
                m_pSelectMediaButton = new QIToolButton(m_pSourceCnt);
                {
                    m_pSelectMediaButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
                    m_pSelectMediaButton->setAutoRaise(true);
                }
                pSourceCntLayout->addWidget(m_pMediaSelector);
                pSourceCntLayout->addWidget(m_pSelectMediaButton);
            }
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pSourceCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pMediaSelector, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pSelectMediaButton, SIGNAL(clicked()), this, SLOT(sltOpenMediumWithFileOpenDialog()));

    /* Register fields: */
    registerField("source", this, "source");
    registerField("id", this, "id");
}

void UIWizardFirstRunPageBasic2::sltOpenMediumWithFileOpenDialog()
{
    /* Call to base-class: */
    onOpenMediumWithFileOpenDialog();
}

void UIWizardFirstRunPageBasic2::retranslateUi()
{
    /* Translate widgets: */
    if (m_fBootHardDiskWasSet)
        m_pLabel->setText(UIWizardFirstRun::tr("<p>Please select a virtual optical disk file "
                                               "or a physical optical drive containing a disk "
                                               "to start your new virtual machine from.</p>"
                                               "<p>The disk should be suitable for starting a computer from "
                                               "and should contain the operating system you wish to install "
                                               "on the virtual machine if you want to do that now. "
                                               "The disk will be ejected from the virtual drive "
                                               "automatically next time you switch the virtual machine off, "
                                               "but you can also do this yourself if needed using the Devices menu.</p>"));
    else
        m_pLabel->setText(UIWizardFirstRun::tr("<p>Please select a virtual optical disk file "
                                               "or a physical optical drive containing a disk "
                                               "to start your new virtual machine from.</p>"
                                               "<p>The disk should be suitable for starting a computer from. "
                                               "As this virtual machine has no hard drive "
                                               "you will not be able to install an operating system on it at the moment.</p>"));
    m_pSourceCnt->setTitle(UIWizardFirstRun::tr("Start-up disk"));
    m_pSelectMediaButton->setToolTip(UIWizardFirstRun::tr("Choose a virtual optical disk file..."));
}

void UIWizardFirstRunPageBasic2::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardFirstRunPageBasic2::isComplete() const
{
    /* Make sure valid medium chosen: */
    return !vboxGlobal().findMedium(id()).isNull();
}

bool UIWizardFirstRunPageBasic2::validatePage()
{
    /* Initial result: */
    bool fResult = true;

    /* Lock finish button: */
    startProcessing();

    /* Try to insert chosen medium: */
    if (fResult)
        fResult = qobject_cast<UIWizardFirstRun*>(wizard())->insertMedium();

    /* Unlock finish button: */
    endProcessing();

    /* Return result: */
    return fResult;
}

QString UIWizardFirstRunPageBasic2::source() const
{
    return m_pMediaSelector->currentText();
}

