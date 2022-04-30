#pragma once

#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <cerrno>
#include <limits>
#include "limonp/StringUtil.hpp"
#include "limonp/Logging.hpp"
#include "Unicode.hpp"
#include "DatTrie.hpp"
#include "Error.hpp"


namespace cppjieba {

using namespace limonp;

const double MIN_DOUBLE = -3.14e+100;
const double MAX_DOUBLE = 3.14e+100;
const size_t DICT_COLUMN_NUM = 3;
const char* const UNKNOWN_TAG = "";

class DictTrie {
public:
    enum UserWordWeightOption {
        WordWeightMin,
        WordWeightMedian,
        WordWeightMax,
    }; // enum UserWordWeightOption

    DictTrie(const string& dict_path, const string& user_dict_paths = "", const string & dat_cache_path = "",
             UserWordWeightOption user_word_weight_opt = WordWeightMedian) {
        Create(dict_path, user_dict_paths, dat_cache_path, user_word_weight_opt);
    }

    ~DictTrie() = default;

    const DatMemElem* Find(const string & word) const {
        return dat_.Find(word);
    }

    void Find(RuneStrArray::const_iterator begin,
              RuneStrArray::const_iterator end,
              vector<struct DatDag>&res,
              size_t max_word_len = MAX_WORD_LENGTH) const {
        dat_.Find(begin, end, res, max_word_len);
    }

    bool IsUserDictSingleChineseWord(const Rune& word) const {
        return IsIn(user_dict_single_chinese_word_, word);
    }

    double GetMinWeight() const {
        return dat_.GetMinWeight();
    }

    size_t GetTotalDictSize() const {
        return total_dict_size_;
    }

    void InsertUserDictNode(const string& line, bool saveNodeInfo = true) {
        vector<string> buf;

        Split(line, buf, " ");

        if (buf.empty()) {
            return;
        }

        DatElement node_info;
        node_info.word = buf[0];
        node_info.weight = user_word_default_weight_;
        node_info.tag = UNKNOWN_TAG;

        if (buf.size() == 2) {
            node_info.tag = buf[1];
        } else if (buf.size() == 3) {
            if (freq_sum_ > 0.0) {
                double freq = stod(buf[1], nullptr);
                node_info.weight = log(freq / freq_sum_);
                node_info.tag = buf[2];
            }
        }

        if (saveNodeInfo) {
            static_node_infos_.push_back(node_info);
        }

        if (Utf8CharNum(node_info.word) == 1) {
            RuneArray word;

            if (DecodeRunesInString(node_info.word, word)) {
                user_dict_single_chinese_word_.insert(word[0]);
            } else {
                XLOG(ERROR) << "Decode " << node_info.word << " failed. Ignored. Please Check the user dict";
            }
        }
    }

    Error LoadUserDict(const vector<string>& files, bool saveNodeInfo = true) {
        for (auto & file : files) {
            ifstream ifs(file.c_str());
            if (!ifs.is_open()) {
                XLOG(ERROR) << "open " << file << " failed";
                return Error::OpenFileFailed;
            }

            string line;
            while (getline(ifs, line)) {
                if (line.empty()) {
                    continue;
                }
                InsertUserDictNode(line, saveNodeInfo);
            }
        }
        return Error::Ok;
    }


public:
    Error Create(const string& dict_path, const string& user_dict_paths, string dat_cache_path,
                 UserWordWeightOption user_word_weight_opt) {
        size_t file_size_sum = 0;
        string md5;
        Error status = CalcFileListMD5({dict_path, user_dict_paths}, file_size_sum, md5);
        if (status != Error::Ok) {
            return status;
        }
        total_dict_size_ = file_size_sum;

        if (dat_cache_path.empty()) {
            dat_cache_path = dict_path + "." + md5 + "." + to_string(user_word_weight_opt) +  ".dat_cache";
        }

        if (dat_.InitAttachDat(dat_cache_path, md5)) {
            status = LoadUserDict({user_dict_paths}, false); // for load user_dict_single_chinese_word_;
            if (status != Error::Ok) {
                return status;
            }
            return Error::Ok;
        }

        status = LoadDefaultDict(dict_path);
        if (status != Error::Ok) {
            return status;
        }

        double min_weight, max_weight;

        std::tie(freq_sum_, min_weight, max_weight) = CalculateWeight(static_node_infos_);

        status = SetUserWordWeights(user_word_weight_opt);
        if (status != Error::Ok) {
            return status;
        }

        dat_.SetMinWeight(min_weight);

        status = LoadUserDict({user_dict_paths});
        if (status != Error::Ok) {
            return status;
        }

        status = dat_.InitBuildDat(static_node_infos_, dat_cache_path, md5);
        if (Error::Ok != status) {
            return status;
        }

        vector<DatElement>().swap(static_node_infos_);
        return Error::Ok;
    }

private:
    Error LoadDefaultDict(const string& filePath) {
        ifstream ifs(filePath.c_str());
        if (!ifs.is_open()) {
            XLOG(ERROR) << "open " << filePath << " failed.";;
            return Error::OpenFileFailed;
        }

        string line;
        vector<string> buf;

        for (; getline(ifs, line);) {
            Split(line, buf, " ");
            if (buf.size() != DICT_COLUMN_NUM) {
                XLOG(ERROR) << "split result illegal, line:" << line;
                return Error::ValueError;
            }

            DatElement node_info;
            node_info.word = buf[0];
            node_info.weight = strtod(buf[1].c_str(), nullptr);
            if (node_info.weight <= 0.0) {
                XLOG(ERROR) << "bad weight: " << buf[1];
                return Error::ValueError;
            }
            node_info.tag = buf[2];
            static_node_infos_.push_back(node_info);
        }

        if (static_node_infos_.empty()) {
            XLOG(ERROR) << "empty dict";
            return Error::ValueError;
        }
        return Error::Ok;
    }

    static bool WeightCompare(const DatElement& lhs, const DatElement& rhs) {
        return lhs.weight < rhs.weight;
    }

    Error SetUserWordWeights(UserWordWeightOption option) {
        if(static_node_infos_.empty()){
            XLOG(ERROR) << "Got empty dict";
            return Error::ValueError;
        }

        vector<DatElement> x = static_node_infos_;
        sort(x.begin(), x.end(), WeightCompare);

        double min_weight = x[0].weight;
        const double max_weight_ = x[x.size() - 1].weight;
        const double median_weight_ = x[x.size() / 2].weight;

        switch (option) {
            case WordWeightMin:
                user_word_default_weight_ = min_weight;
                break;

            case WordWeightMedian:
                user_word_default_weight_ = median_weight_;
                break;

            default:
                user_word_default_weight_ = max_weight_;
                break;
        }
        return Error::Ok;
    }

    static double CalcFreqSum(const vector<DatElement>& node_infos) {
        double sum = 0.0;

        for (const auto & node_info : node_infos) {
            sum += node_info.weight;
        }

        return sum;
    }

    static std::tuple<double, double, double> CalculateWeight(vector<DatElement>& node_infos) {
        double min_weight = node_infos[0].weight;
        double max_weight = node_infos[0].weight;
        double sum = CalcFreqSum(node_infos);

        for (auto & node_info : node_infos) {
            if (node_info.weight < min_weight) {
                min_weight = node_info.weight;
            } else if (node_info.weight > max_weight) {
                max_weight = node_info.weight;
            }

            assert(node_info.weight > 0.0);
            node_info.weight = log(double(node_info.weight) / sum);
        }
        return {sum, min_weight, max_weight};
    }

private:
    vector<DatElement> static_node_infos_;
    size_t total_dict_size_ = 0;
    DatTrie dat_;

    double freq_sum_;
    double user_word_default_weight_;
    unordered_set<Rune> user_dict_single_chinese_word_;
};
}

