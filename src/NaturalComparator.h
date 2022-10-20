#pragma once
#include <string>

inline static int GetChunk(const std::string str, int start) {
    if(start >= str.size()) return 1;
    char startChar = str[start];
    int length = 1;

    if(Util::isDigit(startChar)) {
        // move until next char or end
        while(start++ < str.size()) {
            if(!Util::isDigit(str[start])) {
                break;
            }
            length++;
        }
    } else {
        // move until next digit or end
        while(start++ < str.size()) {
            if(Util::isDigit(str[start])) {
                break;
            }
            length++;
        }
    }

    return length;
}

inline static bool NaturalComparator(const std::string& lhs, const std::string& rhs) {
    if (lhs.empty())
        return false;
    if (rhs.empty())
        return true;

    int thisPos = 0;
    int thatPos = 0;

    while(thisPos < lhs.size() && thatPos < rhs.size()) {
        int thisChunkSize = GetChunk(lhs, thisPos);
        int thatChunkSize = GetChunk(rhs, thatPos);

        if (Util::isDigit(lhs[thisPos]) && !Util::isDigit(rhs[thatPos])) {
            return false;
        }
        if (!Util::isDigit(lhs[thisPos]) && Util::isDigit(rhs[thatPos])) {
            return true;
        }

        if(Util::isDigit(lhs[thisPos]) && Util::isDigit(rhs[thatPos])) {
            if(thisChunkSize == thatChunkSize) {
                for(int i = 0; i < thisChunkSize; i++) {
                    int res = lhs[thisPos + i] - rhs[thatPos + i];
                    if(res != 0) return res > 0;
                }
            } else {
                return thisChunkSize > thatChunkSize;
            }
        } else {
            int res = lhs.compare(thisPos, thisChunkSize, rhs, thatPos, thatChunkSize);
            if(res != 0) return res > 0;
        }

        if (thatChunkSize == 0) return false;
        if (thisChunkSize == 0) return true;

        thisPos += thisChunkSize;
        thatPos += thatChunkSize;
    }

    return lhs.size() > rhs.size();
};
