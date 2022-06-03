/* $Id: UIWizardCloneVMPageBasic3.cpp 40870 2012-04-11 15:44:29Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVMPageBasic3 class implementation
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
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
#include <QRadioButton>

/* Local includes: */
#include "UIWizardCloneVMPageBasic3.h"
#include "UIWizardCloneVM.h"
#include "QIRichTextLabel.h"

UIWizardCloneVMPageBasic3::UIWizardCloneVMPageBasic3(bool fShowChildsOption /* = true */)
    : m_fShowChildsOption(fShowChildsOption)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
        m_pLabel = new QIRichTextLabel(this);
        m_pMachineRadio = new QRadioButton(this);
            m_pMachineRadio->setChecked(true);
        m_pMachineAndChildsRadio = new QRadioButton(this);
            if (!m_fShowChildsOption)
               m_pMachineAndChildsRadio->hide();
        m_pAllRadio = new QRadioButton(this);
    pMainLayout->addWidget(m_pLabel);
    pMainLayout->addWidget(m_pMachineRadio);
    pMainLayout->addWidget(m_pMachineAndChildsRadio);
    pMainLayout->addWidget(m_pAllRadio);
    pMainLayout->addStretch();

    /* Register KCloneMode class: */
    qRegisterMetaType<KCloneMode>();
    registerField("cloneMode", this, "cloneMode");
}

void UIWizardCloneVMPageBasic3::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVM::tr("Cloning Configuration"));

    /* Translate widgets: */
    m_pMachineRadio->setText(UIWizardCloneVM::tr("Current machine state"));
    m_pMachineAndChildsRadio->setText(UIWizardCloneVM::tr("Current machine and all child states"));
    m_pAllRadio->setText(UIWizardCloneVM::tr("All states"));

    const QString strGeneral = UIWizardCloneVM::tr("Please choose which parts of the virtual machine should be cloned.");
    const QString strOpt1    = UIWizardCloneVM::tr("If you select <b>Current machine state</b>, only the current state "
                                                   "of the virtual machine is cloned.");
    const QString strOpt2    = UIWizardCloneVM::tr("If you select <b>Current machine and all child states</b> the current state "
                                                   "of the virtual machine and any states of child snapshots are cloned.");
    const QString strOpt3    = UIWizardCloneVM::tr("If you select <b>All states</b>, the current machine state "
                                                   "and all snapshots are cloned.");
    if (m_fShowChildsOption)
        m_pLabel->setText(QString("<p>%1</p><p>%2 %3 %4</p>")
                          .arg(strGeneral)
                          .arg(strOpt1)
                          .arg(strOpt2)
                          .arg(strOpt3));
    else
        m_pLabel->setText(QString("<p>%1</p><p>%2 %3</p>")
                          .arg(strGeneral)
                          .arg(strOpt1)
                          .arg(strOpt3));
}

void UIWizardCloneVMPageBasic3::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVMPageBasic3::validatePage()
{
    /* Try to clone VM: */
    startProcessing();
    bool fResult = qobject_cast<UIWizardCloneVM*>(wizard())->cloneVM();
    endProcessing();
    return fResult;
}

KCloneMode UIWizardCloneVMPageBasic3::cloneMode() const
{
    if (m_pMachineAndChildsRadio->isChecked())
        return KCloneMode_MachineAndChildStates;
    else if (m_pAllRadio->isChecked())
        return KCloneMode_AllStates;
    return KCloneMode_MachineState;
}

void UIWizardCloneVMPageBasic3::setCloneMode(KCloneMode cloneMode)
{
    switch (cloneMode)
    {
        case KCloneMode_MachineState: m_pMachineRadio->setChecked(true); break;
        case KCloneMode_MachineAndChildStates: m_pMachineAndChildsRadio->setChecked(true); break;
        case KCloneMode_AllStates: m_pAllRadio->setChecked(true); break;
    }
}

