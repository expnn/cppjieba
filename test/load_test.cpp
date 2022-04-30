#include <iostream>
#include <ctime>
#include <fstream>
#include "cppjieba/MPSegment.hpp"
#include "cppjieba/HMMSegment.hpp"
#include "cppjieba/MixSegment.hpp"
#include "cppjieba/KeywordExtractor.hpp"
#include "limonp/Colors.hpp"

using namespace cppjieba;

const char* const DICT_PATH = "../dict/jieba.dict.utf8";
const char* const HMM_PATH = "../dict/hmm_model.utf8";
const char* const USER_DICT_PATH = "../dict/user.dict.utf8";
const char* const IDF_PATH = "../dict/idf.utf8";
const char* const STOP_WORD_PATH = "../dict/stop_words.utf8";

void Cut(size_t times = 50) {
    DictTrie trie(DICT_PATH, USER_DICT_PATH);
    HMMModel model(HMM_PATH);
    MixSegment seg(&trie, &model);
    vector<string> res;
    string doc;
    ifstream ifs("../test/testdata/weicheng.utf8");
    assert(ifs);
    doc << ifs;
    long beginTime = clock();
    for (size_t i = 0; i < times; i++) {
        printf("process [%3.0lf %%]\r", 100.0 * (i + 1) / times);
        fflush(stdout);
        res.clear();
        seg.CutToStr(doc, res, true);
    }
    printf("\n");
    long endTime = clock();
    ColorPrintln(GREEN, "Cut: [%.3lf seconds]time consumed.", double(endTime - beginTime) / CLOCKS_PER_SEC);
}

void Extract(size_t times = 400) {
    DictTrie trie(DICT_PATH, USER_DICT_PATH);
    HMMModel model(HMM_PATH);

    KeywordExtractor Extractor(&trie, &model, IDF_PATH, STOP_WORD_PATH);
    vector<string> words;
    string doc;
    ifstream ifs("../test/testdata/review.100");
    assert(ifs);
    doc << ifs;
    long beginTime = clock();
    for (size_t i = 0; i < times; i++) {
        printf("process [%3.0lf %%]\r", 100.0 * (i + 1) / times);
        fflush(stdout);
        words.clear();
        Extractor.Extract(doc, words, 5);
    }
    printf("\n");
    long endTime = clock();
    ColorPrintln(GREEN, "Extract: [%.3lf seconds]time consumed.", double(endTime - beginTime) / CLOCKS_PER_SEC);
}

int main(int argc, char **argv) {
    Cut();
    Extract();
    return EXIT_SUCCESS;
}
