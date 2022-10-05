#include "FileOpsProgressSink.h"
#include <combaseapi.h>
#include <assert.h>
#include <dmerror.h>

#include <sherrors.h>
#include <shlwapi.h>
#include <winerror.h>
#include "FileOpsWorker.h"


inline static void PrintError(HRESULT hr) {
    switch(hr) {
        case COPYENGINE_E_FLD_IS_FILE_DEST: printf("[ERROR] Existing destination file with same name as folder\n"); break;
        case COPYENGINE_E_FILE_IS_FLD_DEST: printf("[ERROR] Existing destination folder with same name as file\n"); break;
        case COPYENGINE_E_SAME_FILE:        printf("[ERROR] Source and destination file are the same\n"); break;
        case COPYENGINE_E_ACCESS_DENIED_SRC: printf("[ERROR] Access denied\n"); break;
        default:
            printf("Unhandled or Unknown error: %ld.\n", hr);
    };
}


FileOpProgressSink::FileOpProgressSink() : _cRef(1)
{
}

// IUnknown
IFACEMETHODIMP FileOpProgressSink::QueryInterface(REFIID riid, void **ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(FileOpProgressSink, IFileOperationProgressSink),
        {},
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) FileOpProgressSink::AddRef()
{
    return InterlockedIncrement(&_cRef);
}

IFACEMETHODIMP_(ULONG) FileOpProgressSink::Release()
{
    ULONG cRef = InterlockedDecrement(&_cRef);
    if (0 == cRef)
    {
        delete this;
    }
    return cRef;
}

// IFileOperationProgressSink
IFACEMETHODIMP FileOpProgressSink::StartOperations() {

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::FinishOperations(HRESULT hrResult) {
    assert(currentOperationIdx != -1);

    FileOpProgress progress{};
    progress.type = FILE_OP_PROGRESS_FINISH;
    progress.fileOpIdx = currentOperationIdx;
    fileOpsWorker->ResultQueue.push(progress);


    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PreRenameItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, PCWSTR /*pszNewName*/)
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PostRenameItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, PCWSTR /*pszNewName*/, HRESULT hr, IShellItem * /*psiNewlyCreated*/)
{

    if(!SUCCEEDED(hr)) {
        PrintError(hr);
    }

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PreMoveItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/)
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PostMoveItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/,
        IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/, HRESULT hr, IShellItem * /*psiNewlyCreated*/)
{
    if(!SUCCEEDED(hr)) {
        PrintError(hr);
    }

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PreCopyItem(DWORD dwFlags, IShellItem *psiItem,
        IShellItem *psiDestinationFolder, PCWSTR pszNewName) {

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PostCopyItem(DWORD dwFlags, IShellItem *psiItem,
        IShellItem *psiDestinationFolder, PCWSTR pwszNewName, HRESULT hr,
        IShellItem *psiNewlyCreated) {

    if(!SUCCEEDED(hr)) {
        PrintError(hr);
    }

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PreDeleteItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/)
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PostDeleteItem(DWORD /*dwFlags*/, IShellItem * /*psiItem*/, HRESULT hr, IShellItem * /*psiNewlyCreated*/)
{
    if(!SUCCEEDED(hr)) {
        PrintError(hr);
    }

    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PreNewItem(DWORD /*dwFlags*/, IShellItem * /*psiDestinationFolder*/, PCWSTR /*pszNewName*/)
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PostNewItem(DWORD /*dwFlags*/, IShellItem * /*psiDestinationFolder*/,
        PCWSTR /*pszNewName*/, PCWSTR /*pszTemplateName*/, DWORD /*dwFileAttributes*/, HRESULT /*hrNew*/, IShellItem * /*psiNewItem*/)
{
    return S_OK;
}

IFACEMETHODIMP FileOpProgressSink::UpdateProgress(UINT iWorkTotal, UINT iWorkSoFar) {
    assert(currentOperationIdx != -1);

    FileOpProgress progress{};
    progress.type = FILE_OP_PROGRESS_UPDATE;
    progress.fileOpIdx = currentOperationIdx;
    progress.totalProgress = iWorkTotal;
    progress.currentProgress = iWorkSoFar;
    fileOpsWorker->ResultQueue.push(progress);

    return S_OK;
}

IFACEMETHODIMP FileOpProgressSink::ResetTimer()
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::PauseTimer()
{
    return S_OK;
}
IFACEMETHODIMP FileOpProgressSink::ResumeTimer()
{
    return S_OK;
}

HRESULT FileOpProgressSink::DoModal()
{
    return S_OK;
}


