#pragma once

#include <algorithm>
#include <set>
#include <cassert>
#include "limonp/Logging.hpp"
#include "DictTrie.hpp"
#include "SegmentTagged.hpp"
#include "PosTagger.hpp"
#include "Error.hpp"

namespace cppjieba {

class MPSegment: public SegmentTagged {
public:
    explicit MPSegment(const DictTrie* dictTrie)
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

    ~MPSegment() override = default;

    void Cut(RuneStrArray::const_iterator begin,
             RuneStrArray::const_iterator end,
             vector<WordRange>& words,
             bool, size_t max_word_len) const override {
        vector<DatDag> dags;
        dictTrie_->Find(begin, end, dags, max_word_len);
        CalcDP(dags);
        CutByDag(begin, end, dags, words);
    }

    const DictTrie* GetDictTrie() const override {
        return dictTrie_;
    }

    bool Tag(const string& src, vector<pair<string, string> >& res) const override {
        return tagger_.Tag(src, res, *this);
    }

    bool IsUserDictSingleChineseWord(const Rune& value) const {
        return dictTrie_->IsUserDictSingleChineseWord(value);
    }
private:
    void CalcDP(vector<DatDag>& dags) const {
        for (auto rit = dags.rbegin(); rit != dags.rend(); rit++) {
            rit->max_next = -1;
            rit->max_weight = MIN_DOUBLE;

            for (const auto & it : rit->nexts) {
                const auto nextPos = it.first;
                double val = dictTrie_->GetMinWeight();

                if (nullptr != it.second) {
                    val = it.second->weight;
                }

                if (nextPos  < dags.size()) {
                    val += dags[nextPos].max_weight;
                }

                if ((nextPos <= dags.size()) && (val > rit->max_weight)) {
                    rit->max_weight = val;
                    rit->max_next = nextPos;
                }
            }
        }
    }

    void CutByDag(RuneStrArray::const_iterator begin,
                  RuneStrArray::const_iterator,
                  const vector<DatDag>& dags,
                  vector<WordRange>& words) const {

        for (size_t i = 0; i < dags.size();) {
            const auto next = dags[i].max_next;
            assert(next > i);
            assert(next <= dags.size());
            WordRange wr(begin + i, begin + next - 1);
            words.push_back(wr);
            i = next;
        }
    }

    const DictTrie* dictTrie_;
    PosTagger tagger_;

}; // class MPSegment

} // namespace cppjieba

