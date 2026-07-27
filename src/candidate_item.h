#ifndef CANDIDATE_ITEM_H
#define CANDIDATE_ITEM_H

#include <string>
#include <vector>

class CandidateItem {
public:
    CandidateItem() : pinyin_parts_(), text_(""), timestamp_(0), count_(1) {}

    CandidateItem(const std::vector<std::string>& pinyin_parts,
                  const std::string& text,
                  long long timestamp,
                  int count)
        : pinyin_parts_(pinyin_parts), text_(text), timestamp_(timestamp), count_(count) {}

    const std::vector<std::string>& getPinyinParts() const {
        return pinyin_parts_;
    }

    void setPinyinParts(const std::vector<std::string>& pinyin_parts) {
        pinyin_parts_ = pinyin_parts;
    }

    size_t getPinyinLength() const {
        return pinyin_parts_.size();
    }

    const std::string& getText() const {
        return text_;
    }

    void setText(const std::string& text) {
        text_ = text;
    }

    int getCount() const {
        return count_;
    }

    void setCount(int count) {
        count_ = count;
    }

    long long getTimestamp() const {
        return timestamp_;
    }

    void setTimestamp(long long timestamp) {
        timestamp_ = timestamp;
    }

    long long computeScore(long long now) const {
        long long gap = now - timestamp_;
        if (gap < 0) gap = 0;
        long long gap_minutes = gap / 60;
        const long long DECAY = 100000;
        long long multiplier = 1 + DECAY / (1 + gap_minutes);
        return (long long)count_ * multiplier;
    }

    std::string toString() const;
    std::string getSourceFileName() const;
    int findSourceLineNumber() const;

    static CandidateItem fromCharDictLineNumber(int line_number);
    static CandidateItem fromWordDictLineNumber(int line_number);
    static CandidateItem mergeCandidateItems(const std::vector<CandidateItem>& items);
    static void quickSortByTimestampDesc(std::vector<CandidateItem>& candidates);

private:
    std::vector<std::string> pinyin_parts_;
    std::string text_;
    long long timestamp_;
    int count_;
};

#endif // CANDIDATE_ITEM_H
