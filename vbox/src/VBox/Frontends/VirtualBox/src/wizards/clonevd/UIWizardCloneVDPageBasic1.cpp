/* $Id: UIWizardCloneVDPageBasic1.cpp 41021 2012-04-23 11:02:30Z vboxsync $ */
/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * UIWizardCloneVDPageBasic1 class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include "UIWizardCloneVDPageBasic1.h"
#include "UIWizardCloneVD.h"
#include "UIIconPool.h"
#include "QIRichTextLabel.h"
#include "VBoxMediaComboBox.h"
#include "QIToolButton.h"

UIWizardCloneVDPage1::UIWizardCloneVDPage1()
{
}

void UIWizardCloneVDPage1::onHandleOpenSourceDiskClick()
{
    /* Get source virtual-disk using file-open dialog: */
    QString strMediumId = vboxGlobal().openMediumWithFileOpenDialog(VBoxDefs::MediumType_HardDisk, thisImp());
    if (!strMediumId.isNull())
    {
        /* Update medium-combo if necessary: */
        m_pSourceDiskSelector->setCurrentItem(strMediumId);
        /* Focus on virtual-disk combo: */
        m_pSourceDiskSelector->setFocus();
    }
}

CMedium UIWizardCloneVDPage1::sourceVirtualDisk() const
{
    return vboxGlobal().findMedium(m_pSourceDiskSelector->id()).medium();
}

void UIWizardCloneVDPage1::setSourceVirtualDisk(const CMedium &sourceVirtualDisk)
{
    m_pSourceDiskSelector->setCurrentItem(sourceVirtualDisk.GetId());
}

UIWizardCloneVDPageBasic1::UIWizardCloneVDPageBasic1(const CMedium &sourceVirtualDisk)
{
    /* Create widgets: */
    QVBoxLayout *pMainLayout = new QVBoxLayout(this);
    {
        m_pLabel = new QIRichTextLabel(this);
        m_pSourceDiskCnt = new QGroupBox(this);
        {
            m_pSourceDiskCnt->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
            QHBoxLayout *pSourceDiskCntLayout = new QHBoxLayout(m_pSourceDiskCnt);
            {
                m_pSourceDiskSelector = new VBoxMediaComboBox(m_pSourceDiskCnt);
                {
                    m_pSourceDiskSelector->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
                    m_pSourceDiskSelector->setType(VBoxDefs::MediumType_HardDisk);
                    m_pSourceDiskSelector->setCurrentItem(sourceVirtualDisk.GetId());
                    m_pSourceDiskSelector->repopulate();
                }
                m_pSourceDiskOpenButton = new QIToolButton(m_pSourceDiskCnt);
                {
                    m_pSourceDiskOpenButton->setAutoRaise(true);
                    m_pSourceDiskOpenButton->setIcon(UIIconPool::iconSet(":/select_file_16px.png", ":/select_file_dis_16px.png"));
                }
                pSourceDiskCntLayout->addWidget(m_pSourceDiskSelector);
                pSourceDiskCntLayout->addWidget(m_pSourceDiskOpenButton);
            }
        }
        pMainLayout->addWidget(m_pLabel);
        pMainLayout->addWidget(m_pSourceDiskCnt);
        pMainLayout->addStretch();
    }

    /* Setup connections: */
    connect(m_pSourceDiskSelector, SIGNAL(currentIndexChanged(int)), this, SIGNAL(completeChanged()));
    connect(m_pSourceDiskOpenButton, SIGNAL(clicked()), this, SLOT(sltHandleOpenSourceDiskClick()));

    /* Register classes: */
    qRegisterMetaType<CMedium>();
    /* Register fields: */
    registerField("sourceVirtualDisk", this, "sourceVirtualDisk");
}

void UIWizardCloneVDPageBasic1::sltHandleOpenSourceDiskClick()
{
    /* Call to base-class: */
    onHandleOpenSourceDiskClick();
}

void UIWizardCloneVDPageBasic1::retranslateUi()
{
    /* Translate page: */
    setTitle(UIWizardCloneVD::tr("Welcome to the Copy Virtual Disk wizard!"));

    /* Translate widgets: */
    m_pLabel->setText(UIWizardCloneVD::tr("<p>This wizard will help you to copy a virtual disk.</p>"));
    m_pLabel->setText(m_pLabel->text() + QString("<p>%1</p>").arg(standardHelpText()));
    m_pLabel->setText(m_pLabel->text() + UIWizardCloneVD::tr("Please select the virtual disk which you would like "
                                                             "to copy if it is not already selected. You can either "
                                                             "choose one from the list or use the folder icon "
                                                             "beside the list to select a virtual disk file."));
    m_pSourceDiskCnt->setTitle(UIWizardCloneVD::tr("Virtual disk to copy"));
    m_pSourceDiskOpenButton->setToolTip(UIWizardCloneVD::tr("Choose a virtual hard disk file..."));
}

void UIWizardCloneVDPageBasic1::initializePage()
{
    /* Translate page: */
    retranslateUi();
}

bool UIWizardCloneVDPageBasic1::isComplete() const
{
    /* Make sure source virtual-disk feats the rules: */
    return !sourceVirtualDisk().isNull();
}

