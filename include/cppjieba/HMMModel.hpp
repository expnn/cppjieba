#pragma once

#include "limonp/StringUtil.hpp"
#include "Error.hpp"
#include <cerrno>

namespace cppjieba {

using namespace limonp;
typedef unordered_map<Rune, double> EmitProbMap;

struct HMMModel {
    /*
     * STATUS:
     * 0: HMMModel::B, 1: HMMModel::E, 2: HMMModel::M, 3:HMMModel::S
     * */
    enum {B = 0, E = 1, M = 2, S = 3, STATUS_SUM = 4};

    HMMModel(const string& modelPath) {
        Create(modelPath);
    }

    Error Create(const string& modelPath) {
        memset(startProb, 0, sizeof(startProb));
        memset(transProb, 0, sizeof(transProb));
        statMap[0] = 'B';
        statMap[1] = 'E';
        statMap[2] = 'M';
        statMap[3] = 'S';
        emitProbVec.push_back(&emitProbB);
        emitProbVec.push_back(&emitProbE);
        emitProbVec.push_back(&emitProbM);
        emitProbVec.push_back(&emitProbS);
        auto status = LoadModel(modelPath);
        if (status != Error::Ok) {
            XLOG(ERROR)  << "create HMM model failed. Model path: " << modelPath;
            return status;
        }
        return Error::Ok;
    }

    ~HMMModel() = default;

    Error LoadModel(const string& filePath) {
        ifstream ifile(filePath.c_str());
        if (!ifile.is_open()) {
            XLOG(ERROR)  << "open " << filePath << " failed";
            return Error::OpenFileFailed;
        }

        string line;
        vector<string> tmp;
        vector<string> tmp2;
        //Load startProb
        if (!GetLine(ifile, line)) {
            XLOG(ERROR) << "read a line from " << filePath << " FAILED";
            return Error::FileOperationError;
        }

        Split(line, tmp, " ");
        if (tmp.size() != STATUS_SUM) {
            XLOG(ERROR) << "parse line failed: expecting " << STATUS_SUM << " columns, got "<< tmp.size();
            return Error::ValueError;
        }

        for (size_t j = 0; j < STATUS_SUM; j++) {
            // startProb[j] = atof(tmp[j].c_str());
            // 检查解析是否正确？
            startProb[j] = stod(tmp[j], nullptr);
            if (errno == ERANGE) {
                XLOG(ERROR) << "failed to parse: " << tmp[j] << " to double: out of range";
                return Error::ValueError;
            }
        }

        //Load transProb
        for (auto & i : transProb) {
            if(!GetLine(ifile, line)) {
                XLOG(ERROR) << "read a line from " << filePath << " FAILED";
                return Error::FileOperationError;
            }

            Split(line, tmp, " ");
            if (tmp.size() != STATUS_SUM) {
                XLOG(ERROR) << "parse line failed: expecting " << STATUS_SUM << " columns, got "<< tmp.size();
                return Error::ValueError;
            }

            for (int j = 0; j < STATUS_SUM; j++) {
                // i[j] = atof(tmp[j].c_str());
                i[j] = stod(tmp[j], nullptr);
                if (errno == ERANGE) {
                    XLOG(ERROR) << "failed to parse: " << tmp[j] << " to double: out of range";
                    return Error::ValueError;
                }
            }
        }

        //Load emitProbB
        if (!GetLine(ifile, line)) {
            XLOG(ERROR) << "read a line from " << filePath << " FAILED";
            return Error::FileOperationError;
        }
        if (!LoadEmitProb(line, emitProbB)) {
            XLOG(ERROR) << "load emitProbB from line '" << line << "' FAILED";
            return Error::ValueError;
        }

        //Load emitProbE
        if (!GetLine(ifile, line)) {
            XLOG(ERROR) << "read a line from " << filePath << " FAILED";
            return Error::FileOperationError;
        }
        if (!LoadEmitProb(line, emitProbE)) {
            XLOG(ERROR) << "load emitProbE from line '" << line << "' FAILED";
            return Error::ValueError;
        }

        //Load emitProbM
        if (!GetLine(ifile, line)) {
            XLOG(ERROR) << "read a line from " << filePath << " FAILED";
            return Error::FileOperationError;
        }
        if (!LoadEmitProb(line, emitProbM)){
            XLOG(ERROR) << "load emitProbM from line '" << line << "' FAILED";
            return Error::ValueError;
        }

        //Load emitProbS
        if (!GetLine(ifile, line)) {
            XLOG(ERROR) << "read a line from " << filePath << " FAILED";
            return Error::FileOperationError;
        }
        if (!LoadEmitProb(line, emitProbS)) {
            XLOG(ERROR) << "load emitProbS from line '" << line << "' FAILED";
            return Error::ValueError;
        }

        return Error::Ok;
    }

    static double GetEmitProb(const EmitProbMap* ptMp, Rune key,
                       double defVal) {
        auto cit = ptMp->find(key);

        if (cit == ptMp->end()) {
            return defVal;
        }

        return cit->second;
    }

    static bool GetLine(ifstream& ifile, string& line) {
        while (getline(ifile, line)) {
            Trim(line);

            if (line.empty()) {
                continue;
            }

            if (StartsWith(line, "#")) {
                continue;
            }

            return true;
        }

        return false;
    }

    static bool LoadEmitProb(const string& line, EmitProbMap& mp) {
        if (line.empty()) {
            return false;
        }

        vector<string> tmp, tmp2;
        RuneArray unicode;
        Split(line, tmp, ",");

        for (auto & i : tmp) {
            Split(i, tmp2, ":");

            if (2 != tmp2.size()) {
                XLOG(ERROR) << "emitProb illegal.";
                return false;
            }

            if (!DecodeRunesInString(tmp2[0], unicode) || unicode.size() != 1) {
                XLOG(ERROR) << "TransCode failed.";
                return false;
            }

            mp[unicode[0]] = stod(tmp2[1], nullptr);
            if (errno == ERANGE) {
                XLOG(ERROR) << "parse double from " << tmp2[1] << "failed. ";
                return false;
            }
        }

        return true;
    }

    char statMap[STATUS_SUM];
    double startProb[STATUS_SUM];
    double transProb[STATUS_SUM][STATUS_SUM];
    EmitProbMap emitProbB;
    EmitProbMap emitProbE;
    EmitProbMap emitProbM;
    EmitProbMap emitProbS;
    vector<EmitProbMap* > emitProbVec;
}; // struct HMMModel

} // namespace cppjieba

