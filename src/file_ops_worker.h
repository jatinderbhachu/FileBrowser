#pragma once
#include "types.h"
#include "threaded_queue.h"

#include <thread>
#include <string>

enum FileOpType {
    FILE_OP_COPY,
    FILE_OP_MOVE,
    FILE_OP_DELETE
};

class FileOp {
    public:
    int idx = -1;
    FileOpType opType;
    std::string from;
    std::string to;
    std::string newName;
    int currentProgress = 0;
    int totalProgress = INT32_MAX;
};


struct FileOpProgress {
    int fileOpIdx;
    int currentProgress;
    int totalProgress;
};

class FileOpProgressSink;

class FileOpsWorker {
    static constexpr int MAX_FILE_OPS = 64;
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
