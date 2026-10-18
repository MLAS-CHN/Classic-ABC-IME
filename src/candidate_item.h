#ifndef CANDIDATE_ITEM_H
#define CANDIDATE_ITEM_H

#include <cmath>
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

    // 拼音字母总数：Σ 各段长度（如 hou,xuan = 7）。用于"消耗字母数"排序。
    size_t getPinyinLetterCount() const {
        size_t total = 0;
        for (const auto& part : pinyin_parts_) total += part.size();
        return total;
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

    double computeScore(long long now) const {
        long long gap = now - timestamp_;
        if (gap < 0) gap = 0;
        const double kBoost = 3.0;
        const double kTauDays = 14.0;
        double gap_days = static_cast<double>(gap) / 86400.0;
        double multiplier = 1.0 + kBoost * std::exp(-gap_days / kTauDays);
        return static_cast<double>(count_) * multiplier;
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
