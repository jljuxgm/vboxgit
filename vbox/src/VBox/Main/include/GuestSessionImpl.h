
/* $Id: GuestSessionImpl.h 42090 2012-07-10 12:19:49Z vboxsync $ */
/** @file
 * VirtualBox Main - XXX.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_GUESTSESSIONIMPL
#define ____H_GUESTSESSIONIMPL

#include "VirtualBoxBase.h"

#include "GuestProcessImpl.h"
#include "GuestDirectoryImpl.h"
#include "GuestFileImpl.h"
#include "GuestFsObjInfoImpl.h"

/**
 * TODO
 */
class ATL_NO_VTABLE GuestSession :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IGuestSession)
{
public:
    /** @name COM and internal init/term/mapping cruft.
     * @{ */
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(GuestSession, IGuestSession)
    DECLARE_NOT_AGGREGATABLE(GuestSession)
    DECLARE_PROTECT_FINAL_CONSTRUCT()
    BEGIN_COM_MAP(GuestSession)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IGuestSession)
    END_COM_MAP()
    DECLARE_EMPTY_CTOR_DTOR(GuestSession)

    HRESULT init(const ComPtr<IGuest> pGuest, Utf8Str aUser, Utf8Str aPassword, Utf8Str aDomain, Utf8Str aName);
    void    uninit(void);
    HRESULT FinalConstruct(void);
    void    FinalRelease(void);
    /** @}  */

    /** @name IGuestSession properties.
     * @{ */
    STDMETHOD(COMGETTER(User))(IN_BSTR *aName);
    STDMETHOD(COMGETTER(Domain))(IN_BSTR *aDomain);
    STDMETHOD(COMGETTER(Name))(IN_BSTR *aName);
    STDMETHOD(COMGETTER(Id))(ULONG *aId);
    STDMETHOD(COMGETTER(Timeout))(ULONG *aTimeout);
    STDMETHOD(COMGETTER(Environment))(ComSafeArrayOut(IN_BSTR, aEnvironment));
    STDMETHOD(COMGETTER(Processes))(ComSafeArrayOut(IGuestProcess *, aProcesses));
    STDMETHOD(COMGETTER(Directories))(ComSafeArrayOut(IGuestDirectory *, aDirectories));
    STDMETHOD(COMGETTER(Files))(ComSafeArrayOut(IGuestFile *, aFiles));
    /** @}  */

    /** @name IGuestSession methods.
     * @{ */
    STDMETHOD(Close)(void);
    STDMETHOD(CopyFrom)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(ULONG, aFlags), IProgress **aProgress);
    STDMETHOD(CopyTo)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(ULONG, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryCreate)(IN_BSTR aPath, ULONG aMode, ULONG aFlags, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryCreateTemp)(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aName, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryExists)(IN_BSTR aPath, BOOL *aExists);
    STDMETHOD(DirectoryOpen)(IN_BSTR aPath, IN_BSTR aFilter, IN_BSTR aFlags, IGuestDirectory **aDirectory);
    STDMETHOD(DirectoryQueryInfo)(IN_BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(DirectoryRemove)(IN_BSTR aPath);
    STDMETHOD(DirectoryRemoveRecursive)(IN_BSTR aPath, ComSafeArrayIn(DirectoryRemoveRecFlag, aFlags), IProgress **aProgress);
    STDMETHOD(DirectoryRename)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag, aFlags));
    STDMETHOD(DirectorySetACL)(IN_BSTR aPath, IN_BSTR aACL);
    STDMETHOD(EnvironmentClear)(void);
    STDMETHOD(EnvironmentSet)(IN_BSTR aName, IN_BSTR aValue);
    STDMETHOD(EnvironmentSetArray)(ComSafeArrayIn(IN_BSTR, aValues));
    STDMETHOD(EnvironmentUnset)(IN_BSTR aName);
    STDMETHOD(FileCreateTemp)(IN_BSTR aTemplate, ULONG aMode, IN_BSTR aName, IGuestFile **aFile);
    STDMETHOD(FileExists)(IN_BSTR aPath, BOOL *aExists);
    STDMETHOD(FileOpen)(IN_BSTR aPath, IN_BSTR aOpenMode, IN_BSTR aDisposition, ULONG aCreationMode, LONG64 aOffset, IGuestFile **aFile);
    STDMETHOD(FileQueryInfo)(IN_BSTR aPath, IGuestFsObjInfo **aInfo);
    STDMETHOD(FileQuerySize)(IN_BSTR aPath, LONG64 *aSize);
    STDMETHOD(FileRemove)(IN_BSTR aPath);
    STDMETHOD(FileRename)(IN_BSTR aSource, IN_BSTR aDest, ComSafeArrayIn(PathRenameFlag, aFlags));
    STDMETHOD(FileSetACL)(IN_BSTR aPath, IN_BSTR aACL);
    STDMETHOD(ProcessCreate)(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                             ComSafeArrayIn(ProcessCreateFlag, aFlags), ULONG aTimeoutMS, IGuestProcess **IGuestProcess);
    STDMETHOD(ProcessCreateEx)(IN_BSTR aCommand, ComSafeArrayIn(IN_BSTR, aArguments), ComSafeArrayIn(IN_BSTR, aEnvironment),
                               ComSafeArrayIn(ProcessCreateFlag, aFlags), ULONG aTimeoutMS,
                               ProcessPriority aPriority, ComSafeArrayIn(ULONG, aAffinity),
                               IGuestProcess **IGuestProcess);
    STDMETHOD(ProcessGet)(ULONG aPID, IGuestProcess **IGuestProcess);
    STDMETHOD(SetTimeout)(ULONG aTimeoutMS);
    STDMETHOD(SymlinkCreate)(IN_BSTR aSource, IN_BSTR aTarget, SymlinkType aType);
    STDMETHOD(SymlinkExists)(IN_BSTR aSymlink, BOOL *aExists);
    STDMETHOD(SymlinkRead)(IN_BSTR aSymlink, ComSafeArrayIn(SymlinkReadFlag, aFlags), IN_BSTR *aTarget);
    STDMETHOD(SymlinkRemoveDirectory)(IN_BSTR aPath);
    STDMETHOD(SymlinkRemoveFile)(IN_BSTR aFile);
    /** @}  */

public:
    /** @name Public internal methods.
     * @{ */
    /** @}  */

private:

    typedef std::map <Utf8Str, Utf8Str> SessionEnvironment;
    typedef std::list <ComObjPtr<GuestProcess> > SessionProcesses;
    typedef std::list <ComObjPtr<GuestDirectory> > SessionDirectories;
    typedef std::list <ComObjPtr<GuestFile> > SessionFiles;

    struct Data
    {
        ComPtr<IGuest>       mParent;
        Utf8Str              mUser;
        Utf8Str              mDomain;
        Utf8Str              mPassword;
        Utf8Str              mName;
        ULONG                mId;
        ULONG                mTimeout;
        SessionEnvironment   mEnvironment;
        SessionProcesses     mProcesses;
        SessionDirectories   mDirectories;
        SessionFiles         mFiles;
    } mData;
};

#endif /* !____H_GUESTSESSIONIMPL */

