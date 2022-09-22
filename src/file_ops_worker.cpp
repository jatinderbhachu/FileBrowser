#include "file_ops_worker.h"
#include "file_ops.h"
#include "path.h"
#include "file_ops_progress_sink.h"
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
            // do the file operation and register a progress sink to report progress
            switch(fileOp.opType) {
                case FileOpType::FILE_OP_COPY: 
                {
                    Path fromPath(fileOp.from); fromPath.toAbsolute();
                    Path toPath(fileOp.to); toPath.toAbsolute();

                    FileOps::copyFileOrDirectory(fromPath, toPath, fileOp.newName, mProgressSink.get());
                } break;
                case FileOpType::FILE_OP_MOVE: 
                {
                    ResultQueue.push({fileOp.idx, 10, 10});
                } break;
                case FileOpType::FILE_OP_DELETE: 
                {
                    ResultQueue.push({fileOp.idx, 10, 10});
                } break;
            }

        } else {
            //printf("going to slep\n");
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
            op = newOp;
            WorkQueue.push(newOp);
            mProgressSink->currentOperationIdx = newOp.idx;
            op.from = newOp.from;
            op.to = newOp.to;
            op.newName = newOp.newName;
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
        op.currentProgress = result.currentProgress;
        op.totalProgress = result.totalProgress;


        if(op.totalProgress > 0 && op.currentProgress == op.totalProgress) {
            CompleteFileOperation(op);
        }
    }
}

void FileOpsWorker::CompleteFileOperation(const FileOp& fileOp) {
    assert(fileOp.idx >= 0);

    // just invalidate the file operation at that index
    mFileOperations[fileOp.idx].idx = -1;
    mProgressSink->currentOperationIdx = -1;
}


