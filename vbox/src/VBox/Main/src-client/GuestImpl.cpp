/* $Id: GuestImpl.cpp 39882 2012-01-26 00:54:50Z vboxsync $ */
/** @file
 * VirtualBox COM class implementation: Guest
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

#include "GuestImpl.h"

#include "Global.h"
#include "ConsoleImpl.h"
#include "ProgressImpl.h"
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "GuestDnDImpl.h"
#endif
#include "VMMDev.h"

#include "AutoCaller.h"
#include "Logging.h"

#include <VBox/VMMDev.h>
#ifdef VBOX_WITH_GUEST_CONTROL
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
#endif
#include <iprt/cpp/utils.h>
#include <VBox/vmm/pgm.h>
#include <VBox/version.h>

// defines
/////////////////////////////////////////////////////////////////////////////

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (Guest)

HRESULT Guest::FinalConstruct()
{
    return BaseFinalConstruct();
}

void Guest::FinalRelease()
{
    uninit ();
    BaseFinalRelease();
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the guest object.
 */
HRESULT Guest::init(Console *aParent)
{
    LogFlowThisFunc(("aParent=%p\n", aParent));

    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    /* Confirm a successful initialization when it's the case */
    autoInitSpan.setSucceeded();

    ULONG aMemoryBalloonSize;
    HRESULT ret = mParent->machine()->COMGETTER(MemoryBalloonSize)(&aMemoryBalloonSize);
    if (ret == S_OK)
        mMemoryBalloonSize = aMemoryBalloonSize;
    else
        mMemoryBalloonSize = 0;                     /* Default is no ballooning */

    BOOL fPageFusionEnabled;
    ret = mParent->machine()->COMGETTER(PageFusionEnabled)(&fPageFusionEnabled);
    if (ret == S_OK)
        mfPageFusionEnabled = fPageFusionEnabled;
    else
        mfPageFusionEnabled = false;                /* Default is no page fusion*/

    mStatUpdateInterval = 0;                    /* Default is not to report guest statistics at all */

    /* Clear statistics. */
    for (unsigned i = 0 ; i < GUESTSTATTYPE_MAX; i++)
        mCurrentGuestStat[i] = 0;

#ifdef VBOX_WITH_GUEST_CONTROL
    /* Init the context ID counter at 1000. */
    mNextContextID = 1000;
#endif

#ifdef VBOX_WITH_DRAG_AND_DROP
    m_pGuestDnD = new GuestDnD(this);
#endif

    return S_OK;
}

/**
 * Uninitializes the instance and sets the ready flag to FALSE.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void Guest::uninit()
{
    LogFlowThisFunc(("\n"));

#ifdef VBOX_WITH_GUEST_CONTROL
    /* r=poetzsch: Not sure if this is really right. Please note that
     * IGuest::uninit is called twice (which I also consider a bug). So the
     * test for uninitDone should be always first! */
    /* Scope write lock as much as possible. */
    {
        /*
         * Cleanup must be done *before* AutoUninitSpan to cancel all
         * all outstanding waits in API functions (which hold AutoCaller
         * ref counts).
         */
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        /* Notify left over callbacks that we are about to shutdown ... */
        CallbackMapIter it;
        for (it = mCallbackMap.begin(); it != mCallbackMap.end(); it++)
        {
            int rc2 = callbackNotifyEx(it->first, VERR_CANCELLED,
                                       Guest::tr("VM is shutting down, canceling uncompleted guest requests ..."));
            AssertRC(rc2);
        }

        /* Destroy left over callback data. */
        for (it = mCallbackMap.begin(); it != mCallbackMap.end(); it++)
            callbackDestroy(it->first);

        /* Clear process map. */
        mGuestProcessMap.clear();
    }
#endif

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

#ifdef VBOX_WITH_DRAG_AND_DROP
    delete m_pGuestDnD;
    m_pGuestDnD = NULL;
#endif

    unconst(mParent) = NULL;
}

// IGuest properties
/////////////////////////////////////////////////////////////////////////////

STDMETHODIMP Guest::COMGETTER(OSTypeId)(BSTR *a_pbstrOSTypeId)
{
    CheckComArgOutPointerValid(a_pbstrOSTypeId);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (!mData.mInterfaceVersion.isEmpty())
            mData.mOSTypeId.cloneTo(a_pbstrOSTypeId);
        else
        {
            /* Redirect the call to IMachine if no additions are installed. */
            ComPtr<IMachine> ptrMachine(mParent->machine());
            alock.release();
            hrc = ptrMachine->COMGETTER(OSTypeId)(a_pbstrOSTypeId);
        }
    }
    return hrc;
}

STDMETHODIMP Guest::COMGETTER(AdditionsRunLevel) (AdditionsRunLevelType_T *aRunLevel)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aRunLevel = mData.mAdditionsRunLevel;

    return S_OK;
}

STDMETHODIMP Guest::COMGETTER(AdditionsVersion)(BSTR *a_pbstrAdditionsVersion)
{
    CheckComArgOutPointerValid(a_pbstrAdditionsVersion);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Return the ReportGuestInfo2 version info if available.
         */
        if (   !mData.mAdditionsVersionNew.isEmpty()
            || mData.mAdditionsRunLevel <= AdditionsRunLevelType_None)
            mData.mAdditionsVersionNew.cloneTo(a_pbstrAdditionsVersion);
        else
        {
            /*
             * If we're running older guest additions (< 3.2.0) try get it from
             * the guest properties.
             */
            ComPtr<IMachine> ptrMachine = mParent->machine();
            alock.release(); /* No need to hold this during the IPC fun. */

            Bstr bstr;
            hrc = ptrMachine->GetGuestPropertyValue(Bstr("/VirtualBox/GuestAdd/Version").raw(), bstr.asOutParam());
            if (SUCCEEDED(hrc))
                bstr.detachTo(a_pbstrAdditionsVersion);
            else
            {
                /* Returning 1.4 is better than nothing. */
                alock.acquire();
                mData.mInterfaceVersion.cloneTo(a_pbstrAdditionsVersion);
                hrc = S_OK;
            }
        }
    }
    return hrc;
}

STDMETHODIMP Guest::COMGETTER(AdditionsRevision)(ULONG *a_puAdditionsRevision)
{
    CheckComArgOutPointerValid(a_puAdditionsRevision);

    AutoCaller autoCaller(this);
    HRESULT hrc = autoCaller.rc();
    if (SUCCEEDED(hrc))
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        /*
         * Return the ReportGuestInfo2 version info if available.
         */
        if (   !mData.mAdditionsVersionNew.isEmpty()
            || mData.mAdditionsRunLevel <= AdditionsRunLevelType_None)
            *a_puAdditionsRevision = mData.mAdditionsRevision;
        else
        {
            /*
             * If we're running older guest additions (< 3.2.0) try get it from
             * the guest properties.
             */
            ComPtr<IMachine> ptrMachine = mParent->machine();
            alock.release(); /* No need to hold this during the IPC fun. */

            Bstr bstr;
            hrc = ptrMachine->GetGuestPropertyValue(Bstr("/VirtualBox/GuestAdd/Revision").raw(), bstr.asOutParam());
            if (SUCCEEDED(hrc))
            {
                Utf8Str str(bstr);
                uint32_t uRevision;
                int vrc = RTStrToUInt32Full(str.c_str(), 0, &uRevision);
                if (vrc == VINF_SUCCESS)
                    *a_puAdditionsRevision = uRevision;
                else
                    hrc = VBOX_E_IPRT_ERROR;
            }
            if (FAILED(hrc))
            {
                /* Return 0 if we don't know. */
                *a_puAdditionsRevision = 0;
                hrc = 0;
            }
        }
    }
    return hrc;
}

STDMETHODIMP Guest::COMGETTER(Facilities)(ComSafeArrayOut(IAdditionsFacility*, aFacilities))
{
    CheckComArgOutPointerValid(aFacilities);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    SafeIfaceArray<IAdditionsFacility> fac(mData.mFacilityMap);
    fac.detachTo(ComSafeArrayOutArg(aFacilities));

    return S_OK;
}

BOOL Guest::isPageFusionEnabled()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return false;

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return mfPageFusionEnabled;
}

STDMETHODIMP Guest::COMGETTER(MemoryBalloonSize)(ULONG *aMemoryBalloonSize)
{
    CheckComArgOutPointerValid(aMemoryBalloonSize);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aMemoryBalloonSize = mMemoryBalloonSize;

    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(MemoryBalloonSize)(ULONG aMemoryBalloonSize)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /* We must be 100% sure that IMachine::COMSETTER(MemoryBalloonSize)
     * does not call us back in any way! */
    HRESULT ret = mParent->machine()->COMSETTER(MemoryBalloonSize)(aMemoryBalloonSize);
    if (ret == S_OK)
    {
        mMemoryBalloonSize = aMemoryBalloonSize;
        /* forward the information to the VMM device */
        VMMDev *pVMMDev = mParent->getVMMDev();
        /* MUST release all locks before calling VMM device as its critsect
         * has higher lock order than anything in Main. */
        alock.release();
        if (pVMMDev)
        {
            PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
            if (pVMMDevPort)
                pVMMDevPort->pfnSetMemoryBalloon(pVMMDevPort, aMemoryBalloonSize);
        }
    }

    return ret;
}

STDMETHODIMP Guest::COMGETTER(StatisticsUpdateInterval)(ULONG *aUpdateInterval)
{
    CheckComArgOutPointerValid(aUpdateInterval);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aUpdateInterval = mStatUpdateInterval;
    return S_OK;
}

STDMETHODIMP Guest::COMSETTER(StatisticsUpdateInterval)(ULONG aUpdateInterval)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mStatUpdateInterval = aUpdateInterval;
    /* forward the information to the VMM device */
    VMMDev *pVMMDev = mParent->getVMMDev();
    /* MUST release all locks before calling VMM device as its critsect
     * has higher lock order than anything in Main. */
    alock.release();
    if (pVMMDev)
    {
        PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
        if (pVMMDevPort)
            pVMMDevPort->pfnSetStatisticsInterval(pVMMDevPort, aUpdateInterval);
    }

    return S_OK;
}

STDMETHODIMP Guest::InternalGetStatistics(ULONG *aCpuUser, ULONG *aCpuKernel, ULONG *aCpuIdle,
                                          ULONG *aMemTotal, ULONG *aMemFree, ULONG *aMemBalloon, ULONG *aMemShared,
                                          ULONG *aMemCache, ULONG *aPageTotal,
                                          ULONG *aMemAllocTotal, ULONG *aMemFreeTotal, ULONG *aMemBalloonTotal, ULONG *aMemSharedTotal)
{
    CheckComArgOutPointerValid(aCpuUser);
    CheckComArgOutPointerValid(aCpuKernel);
    CheckComArgOutPointerValid(aCpuIdle);
    CheckComArgOutPointerValid(aMemTotal);
    CheckComArgOutPointerValid(aMemFree);
    CheckComArgOutPointerValid(aMemBalloon);
    CheckComArgOutPointerValid(aMemShared);
    CheckComArgOutPointerValid(aMemCache);
    CheckComArgOutPointerValid(aPageTotal);
    CheckComArgOutPointerValid(aMemAllocTotal);
    CheckComArgOutPointerValid(aMemFreeTotal);
    CheckComArgOutPointerValid(aMemBalloonTotal);
    CheckComArgOutPointerValid(aMemSharedTotal);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCpuUser    = mCurrentGuestStat[GUESTSTATTYPE_CPUUSER];
    *aCpuKernel  = mCurrentGuestStat[GUESTSTATTYPE_CPUKERNEL];
    *aCpuIdle    = mCurrentGuestStat[GUESTSTATTYPE_CPUIDLE];
    *aMemTotal   = mCurrentGuestStat[GUESTSTATTYPE_MEMTOTAL] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aMemFree    = mCurrentGuestStat[GUESTSTATTYPE_MEMFREE] * (_4K/_1K);       /* page (4K) -> 1KB units */
    *aMemBalloon = mCurrentGuestStat[GUESTSTATTYPE_MEMBALLOON] * (_4K/_1K); /* page (4K) -> 1KB units */
    *aMemCache   = mCurrentGuestStat[GUESTSTATTYPE_MEMCACHE] * (_4K/_1K);     /* page (4K) -> 1KB units */
    *aPageTotal  = mCurrentGuestStat[GUESTSTATTYPE_PAGETOTAL] * (_4K/_1K);   /* page (4K) -> 1KB units */

    /* MUST release all locks before calling any PGM statistics queries,
     * as they are executed by EMT and that might deadlock us by VMM device
     * activity which waits for the Guest object lock. */
    alock.release();
    Console::SafeVMPtr pVM (mParent);
    if (pVM.isOk())
    {
        uint64_t uFreeTotal, uAllocTotal, uBalloonedTotal, uSharedTotal;
        *aMemFreeTotal = 0;
        int rc = PGMR3QueryGlobalMemoryStats(pVM.raw(), &uAllocTotal, &uFreeTotal, &uBalloonedTotal, &uSharedTotal);
        AssertRC(rc);
        if (rc == VINF_SUCCESS)
        {
            *aMemAllocTotal   = (ULONG)(uAllocTotal / _1K);  /* bytes -> KB */
            *aMemFreeTotal    = (ULONG)(uFreeTotal / _1K);
            *aMemBalloonTotal = (ULONG)(uBalloonedTotal / _1K);
            *aMemSharedTotal  = (ULONG)(uSharedTotal / _1K);
        }
        else
            return E_FAIL;

        /* Query the missing per-VM memory statistics. */
        *aMemShared  = 0;
        uint64_t uTotalMem, uPrivateMem, uSharedMem, uZeroMem;
        rc = PGMR3QueryMemoryStats(pVM.raw(), &uTotalMem, &uPrivateMem, &uSharedMem, &uZeroMem);
        if (rc == VINF_SUCCESS)
        {
            *aMemShared = (ULONG)(uSharedMem / _1K);
        }
        else
            return E_FAIL;
    }
    else
    {
        *aMemAllocTotal   = 0;
        *aMemFreeTotal    = 0;
        *aMemBalloonTotal = 0;
        *aMemSharedTotal  = 0;
        *aMemShared       = 0;
        return E_FAIL;
    }

    return S_OK;
}

HRESULT Guest::setStatistic(ULONG aCpuId, GUESTSTATTYPE enmType, ULONG aVal)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (enmType >= GUESTSTATTYPE_MAX)
        return E_INVALIDARG;

    mCurrentGuestStat[enmType] = aVal;
    return S_OK;
}

/**
 * Returns the status of a specified Guest Additions facility.
 *
 * @return  aStatus         Current status of specified facility.
 * @param   aType           Facility to get the status from.
 * @param   aTimestamp      Timestamp of last facility status update in ms (optional).
 */
STDMETHODIMP Guest::GetFacilityStatus(AdditionsFacilityType_T aType, LONG64 *aTimestamp, AdditionsFacilityStatus_T *aStatus)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    CheckComArgNotNull(aStatus);
    /* Not checking for aTimestamp is intentional; it's optional. */

    FacilityMapIterConst it = mData.mFacilityMap.find(aType);
    if (it != mData.mFacilityMap.end())
    {
        AdditionsFacility *pFacility = it->second;
        ComAssert(pFacility);
        *aStatus = pFacility->getStatus();
        if (aTimestamp)
            *aTimestamp = pFacility->getLastUpdated();
    }
    else
    {
        /*
         * Do not fail here -- could be that the facility never has been brought up (yet) but
         * the host wants to have its status anyway. So just tell we don't know at this point.
         */
        *aStatus = AdditionsFacilityStatus_Unknown;
        if (aTimestamp)
            *aTimestamp = RTTimeMilliTS();
    }
    return S_OK;
}

STDMETHODIMP Guest::GetAdditionsStatus(AdditionsRunLevelType_T aLevel, BOOL *aActive)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    switch (aLevel)
    {
        case AdditionsRunLevelType_System:
            *aActive = (mData.mAdditionsRunLevel > AdditionsRunLevelType_None);
            break;

        case AdditionsRunLevelType_Userland:
            *aActive = (mData.mAdditionsRunLevel >= AdditionsRunLevelType_Userland);
            break;

        case AdditionsRunLevelType_Desktop:
            *aActive = (mData.mAdditionsRunLevel >= AdditionsRunLevelType_Desktop);
            break;

        default:
            rc = setError(VBOX_E_NOT_SUPPORTED,
                          tr("Invalid status level defined: %u"), aLevel);
            break;
    }

    return rc;
}

STDMETHODIMP Guest::SetCredentials(IN_BSTR aUserName, IN_BSTR aPassword,
                                   IN_BSTR aDomain, BOOL aAllowInteractiveLogon)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    /* forward the information to the VMM device */
    VMMDev *pVMMDev = mParent->getVMMDev();
    if (pVMMDev)
    {
        PPDMIVMMDEVPORT pVMMDevPort = pVMMDev->getVMMDevPort();
        if (pVMMDevPort)
        {
            uint32_t u32Flags = VMMDEV_SETCREDENTIALS_GUESTLOGON;
            if (!aAllowInteractiveLogon)
                u32Flags = VMMDEV_SETCREDENTIALS_NOLOCALLOGON;

            pVMMDevPort->pfnSetCredentials(pVMMDevPort,
                                           Utf8Str(aUserName).c_str(),
                                           Utf8Str(aPassword).c_str(),
                                           Utf8Str(aDomain).c_str(),
                                           u32Flags);
            return S_OK;
        }
    }

    return setError(VBOX_E_VM_ERROR,
                    tr("VMM device is not available (is the VM running?)"));
}

STDMETHODIMP Guest::DragHGEnter(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction)
{
    /* Input validation */
    CheckComArgSafeArrayNotNull(allowedActions);
    CheckComArgSafeArrayNotNull(formats);
    CheckComArgOutPointerValid(pResultAction);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragHGEnter(uScreenId, uX, uY, defaultAction, ComSafeArrayInArg(allowedActions), ComSafeArrayInArg(formats), pResultAction);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragHGMove(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), DragAndDropAction_T *pResultAction)
{
    /* Input validation */
    CheckComArgSafeArrayNotNull(allowedActions);
    CheckComArgSafeArrayNotNull(formats);
    CheckComArgOutPointerValid(pResultAction);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragHGMove(uScreenId, uX, uY, defaultAction, ComSafeArrayInArg(allowedActions), ComSafeArrayInArg(formats), pResultAction);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragHGLeave(ULONG uScreenId)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragHGLeave(uScreenId);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragHGDrop(ULONG uScreenId, ULONG uX, ULONG uY, DragAndDropAction_T defaultAction, ComSafeArrayIn(DragAndDropAction_T, allowedActions), ComSafeArrayIn(IN_BSTR, formats), BSTR *pstrFormat, DragAndDropAction_T *pResultAction)
{
    /* Input validation */
    CheckComArgSafeArrayNotNull(allowedActions);
    CheckComArgSafeArrayNotNull(formats);
    CheckComArgOutPointerValid(pstrFormat);
    CheckComArgOutPointerValid(pResultAction);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragHGDrop(uScreenId, uX, uY, defaultAction, ComSafeArrayInArg(allowedActions), ComSafeArrayInArg(formats), pstrFormat, pResultAction);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragHGPutData(ULONG uScreenId, IN_BSTR bstrFormat, ComSafeArrayIn(BYTE, data), IProgress **ppProgress)
{
    /* Input validation */
    CheckComArgStrNotEmptyOrNull(bstrFormat);
    CheckComArgSafeArrayNotNull(data);
    CheckComArgOutPointerValid(ppProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragHGPutData(uScreenId, bstrFormat, ComSafeArrayInArg(data), ppProgress);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragGHPending(ULONG uScreenId, ComSafeArrayOut(BSTR, formats), ComSafeArrayOut(DragAndDropAction_T, allowedActions), DragAndDropAction_T *pDefaultAction)
{
    /* Input validation */
    CheckComArgSafeArrayNotNull(formats);
    CheckComArgSafeArrayNotNull(allowedActions);
    CheckComArgOutPointerValid(pDefaultAction);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragGHPending(uScreenId, ComSafeArrayOutArg(formats), ComSafeArrayOutArg(allowedActions), pDefaultAction);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragGHDropped(IN_BSTR bstrFormat, DragAndDropAction_T action, IProgress **ppProgress)
{
    /* Input validation */
    CheckComArgStrNotEmptyOrNull(bstrFormat);
    CheckComArgOutPointerValid(ppProgress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragGHDropped(bstrFormat, action, ppProgress);
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

STDMETHODIMP Guest::DragGHGetData(ComSafeArrayOut(BYTE, data))
{
    /* Input validation */
    CheckComArgSafeArrayNotNull(data);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

#ifdef VBOX_WITH_DRAG_AND_DROP
    return m_pGuestDnD->dragGHGetData(ComSafeArrayOutArg(data));
#else /* VBOX_WITH_DRAG_AND_DROP */
    ReturnComNotImplemented();
#endif /* !VBOX_WITH_DRAG_AND_DROP */
}

// public methods only for internal purposes
/////////////////////////////////////////////////////////////////////////////

/**
 * Sets the general Guest Additions information like
 * API (interface) version and OS type.  Gets called by
 * vmmdevUpdateGuestInfo.
 *
 * @param aInterfaceVersion
 * @param aOsType
 */
void Guest::setAdditionsInfo(Bstr aInterfaceVersion, VBOXOSTYPE aOsType)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Note: The Guest Additions API (interface) version is deprecated
     * and will not be used anymore!  We might need it to at least report
     * something as version number if *really* ancient Guest Additions are
     * installed (without the guest version + revision properties having set).
     */
    mData.mInterfaceVersion = aInterfaceVersion;

    /*
     * Older Additions rely on the Additions API version whether they
     * are assumed to be active or not.  Since newer Additions do report
     * the Additions version *before* calling this function (by calling
     * VMMDevReportGuestInfo2, VMMDevReportGuestStatus, VMMDevReportGuestInfo,
     * in that order) we can tell apart old and new Additions here. Old
     * Additions never would set VMMDevReportGuestInfo2 (which set mData.mAdditionsVersion)
     * so they just rely on the aInterfaceVersion string (which gets set by
     * VMMDevReportGuestInfo).
     *
     * So only mark the Additions as being active (run level = system) when we
     * don't have the Additions version set.
     */
    if (mData.mAdditionsVersionNew.isEmpty())
    {
        if (aInterfaceVersion.isEmpty())
            mData.mAdditionsRunLevel = AdditionsRunLevelType_None;
        else
        {
            mData.mAdditionsRunLevel = AdditionsRunLevelType_System;

            /*
             * To keep it compatible with the old Guest Additions behavior we need to set the
             * "graphics" (feature) facility to active as soon as we got the Guest Additions
             * interface version.
             */
            facilityUpdate(VBoxGuestFacilityType_Graphics, VBoxGuestFacilityStatus_Active);
        }
    }

    /*
     * Older Additions didn't have this finer grained capability bit,
     * so enable it by default. Newer Additions will not enable this here
     * and use the setSupportedFeatures function instead.
     */
    facilityUpdate(VBoxGuestFacilityType_Graphics, facilityIsActive(VBoxGuestFacilityType_VBoxGuestDriver) ?
                   VBoxGuestFacilityStatus_Active : VBoxGuestFacilityStatus_Inactive);

    /*
     * Note! There is a race going on between setting mAdditionsRunLevel and
     * mSupportsGraphics here and disabling/enabling it later according to
     * its real status when using new(er) Guest Additions.
     */
    mData.mOSTypeId = Global::OSTypeId(aOsType);
}

/**
 * Sets the Guest Additions version information details.
 *
 * Gets called by vmmdevUpdateGuestInfo2 and vmmdevUpdateGuestInfo (to clear the
 * state).
 *
 * @param   a_uFullVersion          VBoxGuestInfo2::additionsMajor,
 *                                  VBoxGuestInfo2::additionsMinor and
 *                                  VBoxGuestInfo2::additionsBuild combined into
 *                                  one value by VBOX_FULL_VERSION_MAKE.
 *
 *                                  When this is 0, it's vmmdevUpdateGuestInfo
 *                                  calling to reset the state.
 *
 * @param   a_pszName               Build type tag and/or publisher tag, empty
 *                                  string if neiter of those are present.
 * @param   a_uRevision             See VBoxGuestInfo2::additionsRevision.
 * @param   a_fFeatures             See VBoxGuestInfo2::additionsFeatures.
 */
void Guest::setAdditionsInfo2(uint32_t a_uFullVersion, const char *a_pszName, uint32_t a_uRevision, uint32_t a_fFeatures)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (a_uFullVersion)
    {
        mData.mAdditionsVersionNew  = BstrFmt(*a_pszName ? "%u.%u.%u_%s" : "%u.%u.%u",
                                              VBOX_FULL_VERSION_GET_MAJOR(a_uFullVersion),
                                              VBOX_FULL_VERSION_GET_MINOR(a_uFullVersion),
                                              VBOX_FULL_VERSION_GET_BUILD(a_uFullVersion),
                                              a_pszName);
        mData.mAdditionsVersionFull = a_uFullVersion;
        mData.mAdditionsRevision    = a_uRevision;
        mData.mAdditionsFeatures    = a_fFeatures;
    }
    else
    {
        Assert(!a_fFeatures && !a_uRevision && !*a_pszName);
        mData.mAdditionsVersionNew.setNull();
        mData.mAdditionsVersionFull = 0;
        mData.mAdditionsRevision    = 0;
        mData.mAdditionsFeatures    = 0;
    }
}

bool Guest::facilityIsActive(VBoxGuestFacilityType enmFacility)
{
    Assert(enmFacility < INT32_MAX);
    FacilityMapIterConst it = mData.mFacilityMap.find((AdditionsFacilityType_T)enmFacility);
    if (it != mData.mFacilityMap.end())
    {
        AdditionsFacility *pFac = it->second;
        return (pFac->getStatus() == AdditionsFacilityStatus_Active);
    }
    return false;
}

HRESULT Guest::facilityUpdate(VBoxGuestFacilityType enmFacility, VBoxGuestFacilityStatus enmStatus)
{
    ComAssertRet(enmFacility < INT32_MAX, E_INVALIDARG);

    HRESULT rc;
    RTTIMESPEC tsNow;
    RTTimeNow(&tsNow);

    FacilityMapIter it = mData.mFacilityMap.find((AdditionsFacilityType_T)enmFacility);
    if (it != mData.mFacilityMap.end())
    {
        AdditionsFacility *pFac = it->second;
        rc = pFac->update((AdditionsFacilityStatus_T)enmStatus, tsNow);
    }
    else
    {
        ComObjPtr<AdditionsFacility> pFacility;
        pFacility.createObject();
        ComAssert(!pFacility.isNull());
        rc = pFacility->init(this,
                             (AdditionsFacilityType_T)enmFacility,
                             (AdditionsFacilityStatus_T)enmStatus);
        if (SUCCEEDED(rc))
            mData.mFacilityMap.insert(std::make_pair((AdditionsFacilityType_T)enmFacility, pFacility));
    }

    LogFlowFunc(("Returned with rc=%Rrc\n"));
    return rc;
}

/**
 * Sets the status of a certain Guest Additions facility.
 * Gets called by vmmdevUpdateGuestStatus.
 *
 * @param enmFacility   Facility to set the status for.
 * @param enmStatus     Actual status to set.
 * @param aFlags
 */
void Guest::setAdditionsStatus(VBoxGuestFacilityType enmFacility, VBoxGuestFacilityStatus enmStatus, ULONG aFlags)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    /*
     * Set overall additions run level.
     */

    /* First check for disabled status. */
    uint32_t uCurFacility = enmFacility + (enmStatus == VBoxGuestFacilityStatus_Active ? 0 : -1);
    if (   enmFacility < VBoxGuestFacilityType_VBoxGuestDriver
        || (   enmFacility == VBoxGuestFacilityType_All
            && enmStatus   == VBoxGuestFacilityStatus_Inactive)
       )
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_None;
    }
    else if (uCurFacility >= VBoxGuestFacilityType_VBoxTrayClient)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_Desktop;
    }
    else if (uCurFacility >= VBoxGuestFacilityType_VBoxService)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_Userland;
    }
    else if (uCurFacility >= VBoxGuestFacilityType_VBoxGuestDriver)
    {
        mData.mAdditionsRunLevel = AdditionsRunLevelType_System;
    }
    else /* Should never happen! */
        AssertMsgFailed(("Invalid facility status/run level detected! uCurFacility=%d\n", uCurFacility));

    /** @todo Above is wrong. The runlevel has to be recalculated from the
     *        facilities.  A user can stop VBoxService before logging out.
     *        The runlevel should then drop to System, not Userland.
     *
     * Also, if given enmFacility = VBoxGuestFacilityType_Unknown, we'll be
     * bosting the runlevel to Desktop (0 - 1 = UINT32_MAX; UINT32_MAX >=
     * VBoxGuestFacilityType_VBoxTrayClient). */

    /** @todo VBoxGuest (the driver) should track VMMDevReq_ReportGuestStatus
     * calls per session and automatically send VBoxGuestFacilityStatus_Failed
     * for the facilites that does not have the status
     * VBoxGuestFacilityStatus_Terminated, VBoxGuestFacilityStatus_Failed or
     * VBoxGuestFacilityStatus_Inactive.  Not doing so means this
     * information is not reliable. */

    /*
     * Set a specific facility status.
     */
    if (enmFacility > VBoxGuestFacilityType_Unknown)
    {
        if (enmFacility == VBoxGuestFacilityType_All)
        {
            FacilityMapIter it = mData.mFacilityMap.begin();
            while (it != mData.mFacilityMap.end())
            {
                facilityUpdate((VBoxGuestFacilityType)it->first, enmStatus);
                it++;
            }
        }
        else /* Update one facility only. */
            facilityUpdate(enmFacility, enmStatus);
    }
}

/**
 * Sets the supported features (and whether they are active or not).
 *
 * @param   fCaps       Guest capability bit mask (VMMDEV_GUEST_SUPPORTS_XXX).
 */
void Guest::setSupportedFeatures(uint32_t aCaps)
{
    AutoCaller autoCaller(this);
    AssertComRCReturnVoid(autoCaller.rc());

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    facilityUpdate(VBoxGuestFacilityType_Seamless, aCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS ?
                   VBoxGuestFacilityStatus_Active : VBoxGuestFacilityStatus_Inactive);
    /** @todo Add VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING */
    facilityUpdate(VBoxGuestFacilityType_Graphics, aCaps & VMMDEV_GUEST_SUPPORTS_GRAPHICS ?
                   VBoxGuestFacilityStatus_Active : VBoxGuestFacilityStatus_Inactive);
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
