#include "FileOpsWorker.h"
#include "FileSystem.h"
#include "Path.h"
#include "FileOpsProgressSink.h"
#include <memory>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <combaseapi.h>
#include <assert.h>

void FileOpsWorker::Run() {
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if(hr != S_OK) {
        assert(false && "Failed to initialize COM library\n");
        return;
    }

    mFileOperations.resize(64);

    std::mutex wakeMutex;
    printf("Starting worker thread..\n");

    std::vector<BatchFileOperation> fileOpsBatch;
    while(mAlive.load()) {
        BatchFileOperation batchOp;

        if(WorkQueue.pop(batchOp)) {

            mProgressSink->currentOperationIdx = batchOp.idx;
            FileSystem::FileOperation op;
            op.init(mProgressSink.get());
            for(BatchFileOperation::Operation fileOp : batchOp.operations) {
                // do the file operation and register a progress sink to report progress
                switch(fileOp.opType) {
                    case FileOpType::FILE_OP_COPY: 
                    {
                        assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                        Path fromPath(fileOp.from); fromPath.toAbsolute();
                        Path toPath(fileOp.to); toPath.toAbsolute();
                        op.copy(fromPath, toPath);
                    } break;
                    case FileOpType::FILE_OP_MOVE: 
                    {
                        assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                        assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                        Path fromPath(fileOp.from); fromPath.toAbsolute();
                        Path toPath(fileOp.to); toPath.toAbsolute();
                        op.move(fromPath, toPath);
                    } break;
                    case FileOpType::FILE_OP_RENAME: 
                    {
                        assert(!fileOp.from.isEmpty());
                        Path fromPath(fileOp.from); fromPath.toAbsolute();
                        op.rename(fromPath, fileOp.to.str());
                    } break;
                    case FileOpType::FILE_OP_DELETE: 
                    {
                        assert(!fileOp.from.isEmpty());
                        Path fromPath(fileOp.from); fromPath.toAbsolute();
                        op.remove(fromPath);
                    } break;
                }
            }

            op.allowUndo(true);
            op.execute();

        } else {
            std::unique_lock<std::mutex> lock(wakeMutex);
            mWakeCondition.wait(lock);
        }
    }

    printf("Exiting thread...\n");

    CoUninitialize();
}

FileOpsWorker::FileOpsWorker() {
    mThread = std::thread(&FileOpsWorker::Run, this);

    mProgressSink = std::make_unique<FileOpProgressSink>();
    mProgressSink->currentOperationIdx = -1;
    mProgressSink->fileOpsWorker = this;
}

FileOpsWorker::~FileOpsWorker() {
    // cleanup worker thread
    mAlive.store(false);
    mWakeCondition.notify_all();
    mThread.join();
}

void FileOpsWorker::addFileOperation(BatchFileOperation newOp) {
    if(newOp.operations.empty()) return; 

    // check if there is an open spot to add the file operation
    int newOpIdx = -1;
    for (int i = 0; i < mFileOperations.size(); i++) {
        if(mFileOperations[i].idx < 0) {
            newOpIdx = i;
            break;
        }
    }

    // allocate new index if there is no open spot
    if(newOpIdx < 0) {
        mFileOperations.push_back(BatchFileOperation());
        newOpIdx = mFileOperations.size() - 1;
    }

    BatchFileOperation& op = mFileOperations[newOpIdx];
    op.idx = newOpIdx;
    op.operations = newOp.operations;

    mOperationsInProgress++;

    WorkQueue.push(op);
    mWakeCondition.notify_all();
    return;
}

void FileOpsWorker::syncProgress() {
    FileOpProgress result;
    while(ResultQueue.pop(result)) {
        BatchFileOperation& op = mFileOperations[result.fileOpIdx];

        switch(result.type) {
            case FILE_OP_PROGRESS_UPDATE: 
                {
                    op.currentProgress = result.currentProgress;
                    op.totalProgress = result.totalProgress;
                } break;
            case FILE_OP_PROGRESS_FINISH_SUCCESS:
            case FILE_OP_PROGRESS_FINISH_ERROR: 
                {
                    mOperationsInProgress--;

                    // just invalidate the file operation at that index
                    op.idx = -1;
                    
                    // TODO: replace with another struct specifically for history?
                    BatchFileOperation historyOp{};
                    historyOp.idx = -1;
                    historyOp.operations = op.operations;

                    printf("Finish operation\n");

                    mHistory.push_back(historyOp);
                } break;
            default:
                break;
        }
    }
}

