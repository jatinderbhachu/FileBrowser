#pragma once
#include "ThreadedQueue.h"
#include "Path.h"

#include <thread>

enum class FileOpType {
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_RENAME,
    FILE_OP_DELETE
};

class BatchFileOperation {
public:
    struct Operation {
        FileOpType opType;
        Path from;
        Path to;
    };
    std::vector<Operation> operations;
    int currentProgress = 0;
    int totalProgress = INT32_MAX;
    int idx = -1;
};

enum FileOpProgressType {
    FILE_OP_PROGRESS_UPDATE = 0,
    FILE_OP_PROGRESS_FINISH_SUCCESS,
    FILE_OP_PROGRESS_FINISH_ERROR,
};

struct FileOpProgress {
    FileOpProgressType type;
    int fileOpIdx;
    int currentProgress;
    int totalProgress;
};

class FileOpProgressSink;

class FileOpsWorker {
public:
    FileOpsWorker();
    ~FileOpsWorker();
    
    void addFileOperation(BatchFileOperation newOp);
    void syncProgress();

    inline int numOperationsInProgress() const { return mOperationsInProgress; }
    std::vector<BatchFileOperation> mFileOperations;

    ThreadedQueue<FileOpProgress> ResultQueue;
private:
    void Run();

    std::vector<BatchFileOperation> mHistory;

    int mOperationsInProgress = 0;

    std::thread mThread;
    ThreadedQueue<BatchFileOperation> WorkQueue;
    std::atomic_bool mAlive{ true };
    std::condition_variable mWakeCondition;
    std::unique_ptr<FileOpProgressSink> mProgressSink = nullptr;
};
