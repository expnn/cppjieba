#pragma once

#include <algorithm>
#include <set>
#include <cassert>
#include "limonp/Logging.hpp"
#include "DictTrie.hpp"
#include "SegmentBase.hpp"
#include "FullSegment.hpp"
#include "MixSegment.hpp"
#include "Unicode.hpp"
#include "DictTrie.hpp"

namespace cppjieba {
class QuerySegment: public SegmentBase {
public:
    QuerySegment(const DictTrie* dictTrie, const HMMModel* model)
        : mixSeg_(dictTrie, model), trie_(dictTrie) {
    }

    Error Create(const DictTrie* dictTrie, const HMMModel* model) {
        auto status = this->mixSeg_.Create(dictTrie, model);
        if (status != Error::Ok) {
            XLOG(ERROR) << "Failed to create submodule: mixSeg";
            return status;
        }
        this->trie_ = dictTrie;
        return Error::Ok;
    }

    ~QuerySegment() override = default;

    void Cut(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end, vector<WordRange>& res, bool hmm,
                     size_t) const override {
        //use mix Cut first
        vector<WordRange> mixRes;
        mixSeg_.CutRuneArray(begin, end, mixRes, hmm);

        vector<WordRange> fullRes;

        for (auto mixRe : mixRes) {
            if (mixRe.Length() > 2) {
                for (size_t i = 0; i + 1 < mixRe.Length(); i++) {
                    string text = EncodeRunesToString(mixRe.left + i, mixRe.left + i + 2);

                    if (trie_->Find(text) != nullptr) {
                        WordRange wr(mixRe.left + i, mixRe.left + i + 1);
                        res.push_back(wr);
                    }
                }
            }

            if (mixRe.Length() > 3) {
                for (size_t i = 0; i + 2 < mixRe.Length(); i++) {
                    string text = EncodeRunesToString(mixRe.left + i, mixRe.left + i + 3);

                    if (trie_->Find(text) != nullptr) {
                        WordRange wr(mixRe.left + i, mixRe.left + i + 2);
                        res.push_back(wr);
                    }
                }
            }

            res.push_back(mixRe);
        }
    }
private:
    static bool IsAllAscii(const RuneArray& s) {
        for (unsigned int i : s) {
            if (i >= 0x80) {
                return false;
            }
        }

        return true;
    }
    MixSegment mixSeg_;
    const DictTrie* trie_;
}; // QuerySegment

} // namespace cppjieba

