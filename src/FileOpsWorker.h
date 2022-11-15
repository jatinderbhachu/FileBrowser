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
        std::string newName = "";
    };
    std::vector<Operation> operations;
    int idx = -1;
    bool allowUndo = true;

    int currentProgress = 0;
    int totalProgress = INT32_MAX;
    FileOpType currentOpType;
    std::string currentOpDescription;
};

class FileOpProgressSink;

class FileOpsWorker {
public:
    FileOpsWorker();
    ~FileOpsWorker();
    
    void addFileOperation(BatchFileOperation newOp);
    void syncProgress();

    inline int numOperationsInProgress() const { return mOperationsInProgress; }
    inline bool isPaused() const { return mPauseFlag; }

    BatchFileOperation& getCurrentOperation();

    void flagPauseOperation();
    void resumeOperation();
    std::vector<BatchFileOperation> mHistory;

private:
    void Run();

    void pauseOperation();

    void updateCurrentOpDescription(FileOpType type, const std::string& description);
    void updateCurrentOpProgress(int workSoFar, int workTotal);
    void finishCurrentOperation();

    int mOperationsInProgress = 0;

    ThreadedQueue<BatchFileOperation> mWorkQueue;
    std::vector<BatchFileOperation> mFileOperations;

    std::atomic_bool        mAlive{ true };
    std::condition_variable mWakeCondition;

    std::mutex          mPauseMutex;
    std::atomic_bool    mPauseFlag{ false };
    
    std::thread mThread;
    std::unique_ptr<FileOpProgressSink> mProgressSink = nullptr;

    int mCurrentOpIdx = -1;

    friend class FileOpProgressSink;
};
