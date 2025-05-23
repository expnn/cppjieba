#pragma once

#include <algorithm>
#include <set>
#include <cassert>
#include "limonp/Logging.hpp"
#include "DictTrie.hpp"
#include "SegmentBase.hpp"
#include "Unicode.hpp"

namespace cppjieba {
class FullSegment: public SegmentBase {
public:
    explicit FullSegment(const DictTrie* dictTrie)
        : dictTrie_(dictTrie) {
        assert(dictTrie_);
    }

    Error Create(const DictTrie* dictTrie) {
        if (nullptr == dictTrie) {
            XLOG(ERROR) << "Got NULL DictTrie pointer ";
            return Error::ValueError;
        }
        this->dictTrie_ = dictTrie;
        return Error::Ok;
    }

    ~FullSegment() override = default;

    void Cut(RuneStrArray::const_iterator begin,
             RuneStrArray::const_iterator end,
             vector<WordRange>& res, bool, size_t) const override {
        assert(dictTrie_);
        vector<struct DatDag> dags;
        dictTrie_->Find(begin, end, dags);
        size_t max_word_end_pos = 0;

        for (size_t i = 0; i < dags.size(); i++) {
            for (const auto & kv : dags[i].nexts) {
                const size_t nextoffset = kv.first - 1;
                assert(nextoffset < dags.size());
                const auto wordLen = nextoffset - i + 1;
                const bool is_not_covered_single_word = ((dags[i].nexts.size() == 1) && (max_word_end_pos <= i));
                const bool is_oov = (nullptr == kv.second); //Out-of-Vocabulary

                if ((is_not_covered_single_word) || ((not is_oov) && (wordLen >= 2))) {
                    WordRange wr(begin + i, begin + nextoffset);
                    res.push_back(wr);
                }

                max_word_end_pos = max(max_word_end_pos, nextoffset + 1);
            }
        }
    }
private:
    const DictTrie* dictTrie_;
};
}

