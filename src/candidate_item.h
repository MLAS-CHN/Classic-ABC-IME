#ifndef CANDIDATE_ITEM_H
#define CANDIDATE_ITEM_H

#include <string>
#include <vector>

/**
 * 候选项数据结构
 * - pinyin_parts: 拼音分段数组
 * - text: 对应词/字
 * - weight: 使用次数权重
 */
class CandidateItem {
public:
    /** 默认构造：空拼音、空文本、零权重。 */
    CandidateItem() : pinyin_parts_(), text_(""), weight_(0) {}

    /**
     * 完整构造函数。
     * @param pinyin_parts 拼音分段数组。
     * @param text 对应候选文本。
     * @param weight 候选权重（通常是使用次数）。
     */
    CandidateItem(const std::vector<std::string>& pinyin_parts,
                  const std::string& text,
                  int weight)
        : pinyin_parts_(pinyin_parts), text_(text), weight_(weight) {}

    /** 获取拼音分段数组。 */
    const std::vector<std::string>& getPinyinParts() const {
        return pinyin_parts_;
    }

    /** 设置拼音分段数组。 */
    void setPinyinParts(const std::vector<std::string>& pinyin_parts) {
        pinyin_parts_ = pinyin_parts;
    }

    /** 获取拼音分段长度。 */
    size_t getPinyinLength() const {
        return pinyin_parts_.size();
    }

    /** 获取候选文本。 */
    const std::string& getText() const {
        return text_;
    }

    /** 设置候选文本。 */
    void setText(const std::string& text) {
        text_ = text;
    }

    /** 获取候选权重。 */
    int getWeight() const {
        return weight_;
    }

    /** 设置候选权重。 */
    void setWeight(int weight) {
        weight_ = weight;
    }

    std::string toString() const;
    std::string getSourceFileName() const;
    int findSourceLineNumber() const;

    /**
     * 根据字库行号构建候选元素（预留接口，当前留空）。
     *
     * @param line_number 字库真实行号（1-based）。
     * @return 候选元素对象。
     */
    static CandidateItem fromCharDictLineNumber(int line_number);

    /**
     * 根据词库行号构建候选元素。
     * 支持两种行格式：
     * - pinyin_csv text weight
     * - pinyin_csv text
     * 若无 weight，默认使用 1。
     *
     * @param line_number 词库真实行号（1-based）。
     * @return 候选元素对象。
     */
    static CandidateItem fromWordDictLineNumber(int line_number);

    static CandidateItem mergeCandidateItems(const std::vector<CandidateItem>& items);

    /**
     * 按权重降序对候选数组执行快速排序（原地修改）。
     *
     * @param candidates 待排序的候选元素数组。
     */
    static void quickSortByWeightDesc(std::vector<CandidateItem>& candidates);

private:
    std::vector<std::string> pinyin_parts_;
    std::string text_;
    int weight_;
};

#endif // CANDIDATE_ITEM_H
