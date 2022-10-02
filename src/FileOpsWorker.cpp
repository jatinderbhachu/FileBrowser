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
        printf("Failed to initialize COM library\n");
        return;
    }

    mFileOperations.resize(MAX_FILE_OPS);

    std::mutex wakeMutex;
    printf("Starting worker thread..\n");

    while(mAlive.load()) {
        FileOp fileOp;

        if(WorkQueue.pop(fileOp)) {

            mProgressSink->currentOperationIdx = fileOp.idx;

            // do the file operation and register a progress sink to report progress
            switch(fileOp.opType) {
                case FileOpType::FILE_OP_COPY: 
                {
                    Path fromPath(fileOp.from); fromPath.toAbsolute();
                    Path toPath(fileOp.to); toPath.toAbsolute();

                    FileOps::copyFileOrDirectory(fromPath, toPath, mProgressSink.get());
                } break;
                case FileOpType::FILE_OP_MOVE: 
                {
                    Path fromPath(fileOp.from); fromPath.toAbsolute();
                    Path toPath(fileOp.to); toPath.toAbsolute();

                    FileOps::moveFileOrDirectory(fromPath, toPath, mProgressSink.get());
                } break;
                case FileOpType::FILE_OP_RENAME: 
                {
                    Path fromPath(fileOp.from); fromPath.toAbsolute();

                    FileOps::renameFileOrDirectory(fromPath, fileOp.to.wstr(), mProgressSink.get());
                } break;
                case FileOpType::FILE_OP_DELETE: 
                {
                    Path fromPath(fileOp.from); fromPath.toAbsolute();

                    FileOps::deleteFileOrDirectory(fromPath, false, mProgressSink.get());
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
    for (int i = 0; i < mFileOperations.size(); i++) {
        FileOp& op = mFileOperations[i];
        if(op.idx < 0) {
            newOp.idx = i;
            
            WorkQueue.push(newOp);

            op = newOp;
            op.from = newOp.from;
            op.to = newOp.to;

            
            mWakeCondition.notify_all();
            return;
        }
    }

    // shouldn't reach this point
    assert(false && "Failed to queue up file operation");
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


