#pragma once

#include <cmath>
#include <set>
#include "MixSegment.hpp"

namespace cppjieba {

using namespace limonp;
using namespace std;

/*utf8*/
class KeywordExtractor {
public:
    struct Word {
        string word;
        vector<size_t> offsets;
        double weight;
    }; // struct Word

    KeywordExtractor(const DictTrie* dictTrie,
                     const HMMModel* model,
                     const string& idfPath,
                     const string& stopWordPath)
        : segment_(dictTrie, model) {
        LoadIdfDict(idfPath);
        LoadStopWordDict(stopWordPath);
    }

    Error Create(const DictTrie* dictTrie,
                 const HMMModel* model,
                 const string& idfPath,
                 const string& stopWordPath) {
        this->segment_.Create(dictTrie, model);
        auto status = LoadIdfDict(idfPath);
        if (Error::Ok != status) {
            XLOG(ERROR) << "failed to load IDF dict";
            return status;
        }
        status = LoadStopWordDict(stopWordPath);
        if (Error::Ok != status) {
            XLOG(ERROR) << "failed to load stop words dict";
            return status;
        }
    }

    ~KeywordExtractor() = default;

    void Extract(const string& sentence, vector<string>& keywords, size_t topN) const {
        vector<Word> topWords;
        Extract(sentence, topWords, topN);

        for (auto & topWord : topWords) {
            keywords.push_back(topWord.word);
        }
    }

    void Extract(const string& sentence, vector<pair<string, double> >& keywords, size_t topN) const {
        vector<Word> topWords;
        Extract(sentence, topWords, topN);

        for (auto & topWord : topWords) {
            keywords.emplace_back(topWord.word, topWord.weight);
        }
    }

    void Extract(const string& sentence, vector<Word>& keywords, size_t topN) const {
        vector<string> words;
        segment_.CutToStr(sentence, words);

        map<string, Word> wordmap;
        size_t offset = 0;

        for (auto & word : words) {
            size_t t = offset;
            offset += word.size();

            if (IsSingleWord(word) || stopWords_.find(word) != stopWords_.end()) {
                continue;
            }

            wordmap[word].offsets.push_back(t);
            wordmap[word].weight += 1.0;
        }

        if (offset != sentence.size()) {
            XLOG(ERROR) << "words illegal";
            return;
        }

        keywords.clear();
        keywords.reserve(wordmap.size());

        for (auto & itr : wordmap) {
            auto cit = idfMap_.find(itr.first);

            if (cit != idfMap_.end()) {
                itr.second.weight *= cit->second;
            } else {
                itr.second.weight *= idfAverage_;
            }

            itr.second.word = itr.first;
            keywords.push_back(itr.second);
        }

        topN = min(topN, keywords.size());
        partial_sort(keywords.begin(), keywords.begin() + topN, keywords.end(), Compare);
        keywords.resize(topN);
    }
private:
    Error LoadIdfDict(const string& idfPath) {
        ifstream ifs(idfPath.c_str());

        if (!ifs.is_open()){
            XLOG(ERROR) << "open " << idfPath << " failed";
            return Error::OpenFileFailed;
        }

        string line ;
        vector<string> buf;
        double idf = 0.0;
        double idfSum = 0.0;
        size_t lineno = 0;

        for (; getline(ifs, line); lineno++) {
            buf.clear();

            if (line.empty()) {
                XLOG(WARNING) << "lineno: " << lineno << " empty. skipped.";
                continue;
            }

            Split(line, buf, " ");

            if (buf.size() != 2) {
                XLOG(ERROR) << "line: " << line << ", lineno: " << lineno << " bad format. skipped.";
                continue;
            }

            idf = stod(buf[1], nullptr);
            if (errno == ERANGE) {
                XLOG(WARNING) << "failed to parse idf value from: " << lineno << ": " << buf[1];
                return Error::ValueError;
            }
            idfMap_[buf[0]] = idf;
            idfSum += idf;
        }

        if (lineno == 0) {
            XLOG(ERROR) << "empty file.";
            return Error::ValueError;
        }
        idfAverage_ = idfSum / (double)lineno;
        return Error::Ok;
    }

    Error LoadStopWordDict(const string& filePath) {
        ifstream ifs(filePath.c_str());
        if(not ifs.is_open()){
            XLOG(ERROR) << "open " << filePath << " failed";
            return Error::OpenFileFailed;
        }

        string line ;
        while (getline(ifs, line)) {
            stopWords_.insert(line);
        }

        if (stopWords_.empty()) {
            XLOG(ERROR) << "empty file";
            return Error::ValueError;
        }
        return Error::Ok;
    }

    static bool Compare(const Word& lhs, const Word& rhs) {
        return lhs.weight > rhs.weight;
    }

    MixSegment segment_;
    unordered_map<string, double> idfMap_;
    double idfAverage_;

    unordered_set<string> stopWords_;
}; // class KeywordExtractor

inline ostream& operator << (ostream& os, const KeywordExtractor::Word& word) {
    return os << R"({"word": ")" << word.word << R"(", "offset": )"
              << word.offsets << ", \"weight\": " << word.weight << "}";
}

} // namespace cppjieba



