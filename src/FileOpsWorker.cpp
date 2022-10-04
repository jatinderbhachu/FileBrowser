#include "FileOpsWorker.h"
#include "FileOps.h"
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

    std::vector<FileOp> fileOpsBatch;
    while(mAlive.load()) {
        FileOp fileOp;

        if(WorkQueue.pop(fileOp)) {

            mProgressSink->currentOperationIdx = fileOp.idx;

            // do the file operation and register a progress sink to report progress
            switch(fileOp.opType) {
                case FileOpType::FILE_OP_COPY: 
                {
                    assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                    Path fromPath(fileOp.from); fromPath.toAbsolute();
                    Path toPath(fileOp.to); toPath.toAbsolute();

                    if(!FileOps::copyFileOrDirectory(fromPath, toPath, mProgressSink.get())) {
                        FileOpProgress progress;
                        progress.fileOpIdx = fileOp.idx;
                        progress.type = FileOpProgressType::FILE_OP_PROGRESS_FINISH; // TODO: replace with FAIL
                        progress.currentProgress = 1;
                        progress.totalProgress = 1;

                        ResultQueue.push(progress);
                    }
                } break;
                case FileOpType::FILE_OP_MOVE: 
                {
                    assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                    assert(!fileOp.from.isEmpty() && !fileOp.to.isEmpty());
                    Path fromPath(fileOp.from); fromPath.toAbsolute();
                    Path toPath(fileOp.to); toPath.toAbsolute();

                    if(!FileOps::moveFileOrDirectory(fromPath, toPath, mProgressSink.get())) {
                        FileOpProgress progress;
                        progress.fileOpIdx = fileOp.idx;
                        progress.type = FileOpProgressType::FILE_OP_PROGRESS_FINISH; // TODO: replace with FAIL
                        progress.currentProgress = 1;
                        progress.totalProgress = 1;

                        ResultQueue.push(progress);
                    }
                } break;
                case FileOpType::FILE_OP_RENAME: 
                {
                    assert(!fileOp.from.isEmpty());
                    Path fromPath(fileOp.from); fromPath.toAbsolute();

                    if(!FileOps::renameFileOrDirectory(fromPath, fileOp.to.wstr(), mProgressSink.get())) {
                        FileOpProgress progress;
                        progress.fileOpIdx = fileOp.idx;
                        progress.type = FileOpProgressType::FILE_OP_PROGRESS_FINISH; // TODO: replace with FAIL
                        progress.currentProgress = 1;
                        progress.totalProgress = 1;

                        ResultQueue.push(progress);
                    }

                } break;
                case FileOpType::FILE_OP_DELETE: 
                {
                    assert(!fileOp.from.isEmpty());
                    Path fromPath(fileOp.from); fromPath.toAbsolute();

                    if(!FileOps::deleteFileOrDirectory(fromPath, true, mProgressSink.get())) {
                        FileOpProgress progress;
                        progress.fileOpIdx = fileOp.idx;
                        progress.type = FileOpProgressType::FILE_OP_PROGRESS_FINISH; // TODO: replace with FAIL
                        progress.currentProgress = 1;
                        progress.totalProgress = 1;

                        ResultQueue.push(progress);
                    }
                    
                } break;
            }

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

void FileOpsWorker::addFileOperation(FileOp newOp) {

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
        mFileOperations.push_back(FileOp());
        newOpIdx = mFileOperations.size() - 1;
    }

    FileOp& op = mFileOperations[newOpIdx];
    newOp.idx = newOpIdx;

    WorkQueue.push(newOp);

    op = newOp;
    op.from = newOp.from;
    op.to = newOp.to;

    mWakeCondition.notify_all();
    return;
}

void FileOpsWorker::syncProgress() {
    FileOpProgress result;
    while(ResultQueue.pop(result)) {
        FileOp& op = mFileOperations[result.fileOpIdx];

        if(result.type == FILE_OP_PROGRESS_FINISH) {
            CompleteFileOperation(op);
        } else {
            op.currentProgress = result.currentProgress;
            op.totalProgress = result.totalProgress;
        }
 
    }
}

void FileOpsWorker::CompleteFileOperation(const FileOp& fileOp) {
    assert(fileOp.idx >= 0);

    // just invalidate the file operation at that index
    mFileOperations[fileOp.idx].idx = -1;
}


