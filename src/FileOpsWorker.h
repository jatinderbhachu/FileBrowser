#pragma once
#include "types.h"
#include "ThreadedQueue.h"
#include "Path.h"

#include <thread>
#include <string>

enum FileOpType {
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_RENAME,
    FILE_OP_DELETE
};

class FileOp {
    public:
    int idx = -1;
    FileOpType opType;
    Path from;
    Path to;
    int currentProgress = 0;
    int totalProgress = INT32_MAX;
};

enum FileOpProgressType {
    FILE_OP_PROGRESS_UPDATE,
    FILE_OP_PROGRESS_FINISH,
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
    
    void addFileOperation(FileOp newOp);
    void syncProgress();
    std::vector<FileOp> mFileOperations;

    ThreadedQueue<FileOpProgress> ResultQueue;
private:

    void Run();

    void CompleteFileOperation(const FileOp& fileOp);

    std::thread mThread;
    ThreadedQueue<FileOp> WorkQueue;
    std::atomic_bool mAlive{ true };
    std::condition_variable mWakeCondition;
    std::unique_ptr<FileOpProgressSink> mProgressSink = nullptr;
};
