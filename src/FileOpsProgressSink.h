#pragma once
#include <shobjidl_core.h>
#include "ThreadedQueue.h"

class FileOpsWorker;

class FileOpProgressSink : public IFileOperationProgressSink {
public:
    FileOpProgressSink(FileOpsWorker*);

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void **ppv);

    IFACEMETHODIMP_(ULONG) AddRef();

    IFACEMETHODIMP_(ULONG) Release();

    // IFileOperationProgressSink
    IFACEMETHODIMP StartOperations();
    IFACEMETHODIMP FinishOperations(HRESULT hrResult);
    IFACEMETHODIMP PreRenameItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, PCWSTR /*pszNewName*/);
    IFACEMETHODIMP PostRenameItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, PCWSTR /*pszNewName*/, HRESULT /*hrRename*/, IShellItem * /*psiNewlyCreated*/);
    IFACEMETHODIMP PreMoveItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/);
    IFACEMETHODIMP PostMoveItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/,
        IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/, HRESULT /*hrNewName*/, IShellItem * /*psiNewlyCreated*/);
    IFACEMETHODIMP PreCopyItem(DWORD dwFlags, IShellItem *psiItem,
        IShellItem *psiDestinationFolder, PCWSTR pszNewName);
    IFACEMETHODIMP PostCopyItem(DWORD dwFlags, IShellItem *psiItem,
        IShellItem *psiDestinationFolder, PCWSTR pwszNewName, HRESULT hrCopy,
        IShellItem *psiNewlyCreated);
    IFACEMETHODIMP PreDeleteItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/);
    IFACEMETHODIMP PostDeleteItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, HRESULT /*hrDelete*/, IShellItem * /*psiNewlyCreated*/);
    IFACEMETHODIMP PreNewItem(DWORD /*dwFlags*/, IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/);
    IFACEMETHODIMP PostNewItem(DWORD /*dwFlags*/, IShellItem * /*psiDestinationFolder*/,
        PCWSTR /*pszNewName*/, PCWSTR /*pszTemplateName*/, DWORD /*dwFileAttributes*/, HRESULT /*hrNew*/, IShellItem * /*psiNewItem*/);
    IFACEMETHODIMP UpdateProgress(UINT iWorkTotal, UINT iWorkSoFar);
    IFACEMETHODIMP ResetTimer();
    IFACEMETHODIMP PauseTimer();
    IFACEMETHODIMP ResumeTimer();

    FileOpsWorker* mFileOpsWorker;

private:
    long   _cRef;
};


