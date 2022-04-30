#pragma once

#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <utility>

#include "limonp/Md5.hpp"
#include "Unicode.hpp"
#include "darts-clone/include/darts.h"
#include "Error.hpp"

namespace cppjieba {

using std::pair;

struct DatElement {
    string word;
    string tag;
    double weight = 0;

    bool operator < (const DatElement & b) const {
        if (word == b.word) {
            return this->weight > b.weight;
        }

        return this->word < b.word;
    }
};

inline std::ostream & operator << (std::ostream& os, const DatElement & elem) {
    return os << "word=" << elem.word << "/tag=" << elem.tag << "/weight=" << elem.weight;
}

struct DatMemElem {
    double weight = 0.0;
    char tag[8] = {};

    void SetTag(const string & str) {
        memset(&tag[0], 0, sizeof(tag));
        strncpy(&tag[0], str.c_str(), std::min(str.size(), sizeof(tag) - 1));
    }

    string GetTag() const {
        return &tag[0];
    }
};

inline std::ostream & operator << (std::ostream& os, const DatMemElem & elem) {
    return os << "/tag=" << elem.GetTag() << "/weight=" << elem.weight;
}

struct DatDag {
    limonp::LocalVector<pair<size_t, const DatMemElem *> > nexts;
    double max_weight;
    int max_next;
};

typedef Darts::DoubleArray JiebaDAT;


struct CacheFileHeader {
    char md5_hex[32] = {};
    double min_weight = 0;
    uint32_t elements_num = 0;
    uint32_t dat_size = 0;
};

static_assert(sizeof(DatMemElem) == 16, "DatMemElem length invalid");
static_assert((sizeof(CacheFileHeader) % sizeof(DatMemElem)) == 0, "DatMemElem CacheFileHeader length equal");


class DatTrie {
public:
    DatTrie() {}
    ~DatTrie() {
        ::munmap(mmap_addr_, mmap_length_);
        mmap_addr_ = nullptr;
        mmap_length_ = 0;

        ::close(mmap_fd_);
        mmap_fd_ = -1;
    }

    DatTrie(const DatTrie &) = delete;
    DatTrie &operator=(const DatTrie &) = delete;

    const DatMemElem * Find(const string & key) const {
        JiebaDAT::result_pair_type find_result;
        dat_.exactMatchSearch(key.c_str(), find_result);

        if ((0 == find_result.length) || (find_result.value < 0) || (find_result.value >= elements_num_)) {
            return nullptr;
        }

        return &elements_ptr_[ find_result.value ];
    }

    void Find(RuneStrArray::const_iterator begin, RuneStrArray::const_iterator end,
              vector<struct DatDag>&res, size_t max_word_len) const {

        res.clear();
        res.resize(end - begin);
        const string text_str = EncodeRunesToString(begin, end);

        for (size_t i = 0, begin_pos = 0; i < size_t(end - begin); i++) {
            static const size_t max_num = 128;
            JiebaDAT::result_pair_type result_pairs[max_num] = {};
            std::size_t num_results = dat_.commonPrefixSearch(&text_str[begin_pos], &result_pairs[0], max_num);

            res[i].nexts.push_back(pair<size_t, const DatMemElem *>(i + 1, nullptr));

            for (std::size_t idx = 0; idx < num_results; ++idx) {
                auto & match = result_pairs[idx];

                if ((match.value < 0) || (match.value >= elements_num_)) {
                    continue;
                }

                auto const char_num = Utf8CharNum(&text_str[begin_pos], match.length);

                if (char_num > max_word_len) {
                    continue;
                }

                auto pValue = &elements_ptr_[match.value];

                if (1 == char_num) {
                    res[i].nexts[0].second = pValue;
                    continue;
                }

                res[i].nexts.push_back(pair<size_t, const DatMemElem *>(i + char_num, pValue));
            }

            begin_pos += limonp::UnicodeToUtf8Bytes((begin + i)->rune);
        }
    }

    double GetMinWeight() const {
        return min_weight_;
    }

    void SetMinWeight(double d) {
        min_weight_ = d ;
    }

    Error InitBuildDat(vector<DatElement>& elements, const string & dat_cache_file, const string & md5) {
        auto status = BuildDatCache(elements, dat_cache_file, md5);
        if (status != Error::Ok) {
            return status;
        }
        return InitAttachDat(dat_cache_file, md5);
    }

    Error InitAttachDat(const string & dat_cache_file, const string & md5) {
        mmap_fd_ = ::open(dat_cache_file.c_str(), O_RDONLY);

        if (mmap_fd_ < 0) {
            return Error::OpenFileFailed;
        }

        mmap_length_ = ::lseek(mmap_fd_, 0, SEEK_END);
        if (mmap_length_ < sizeof(CacheFileHeader)) {
            return Error::FileOperationError;
        }

        mmap_addr_ = reinterpret_cast<char *>(mmap(nullptr, mmap_length_, PROT_READ, MAP_SHARED, mmap_fd_, 0));
        if (MAP_FAILED == mmap_addr_) {
            return Error::MmapError;
        }

        // assert(mmap_length_ >= sizeof(CacheFileHeader));
        CacheFileHeader & header = *reinterpret_cast<CacheFileHeader*>(mmap_addr_);
        elements_num_ = header.elements_num;
        min_weight_ = header.min_weight;
        assert(sizeof(header.md5_hex) == md5.size());

        if (0 != memcmp(&header.md5_hex[0], md5.c_str(), md5.size())) {
            XLOG(ERROR) << "MD5 checksum failed for file: " << dat_cache_file;
            return Error::ValueError;
        }

        if (mmap_length_ != sizeof(header) + header.elements_num * sizeof(DatMemElem) + header.dat_size * dat_.unit_size()) {
            XLOG(ERROR) << "mmap length check failed. ";
            return Error::ValueError;
        }
        elements_ptr_ = (const DatMemElem *)(mmap_addr_ + sizeof(header));
        const char * dat_ptr = mmap_addr_ + sizeof(header) + sizeof(DatMemElem) * elements_num_;
        dat_.set_array(dat_ptr, header.dat_size);
        return Error::Ok;
    }

private:
    Error BuildDatCache(vector<DatElement>& elements, const string & dat_cache_file, const string & md5) {
        std::sort(elements.begin(), elements.end());

        vector<const char*> keys_ptr_vec;
        vector<int> values_vec;
        vector<DatMemElem> mem_elem_vec;

        keys_ptr_vec.reserve(elements.size());
        values_vec.reserve(elements.size());
        mem_elem_vec.reserve(elements.size());

        CacheFileHeader header;
        header.min_weight = min_weight_;
        assert(sizeof(header.md5_hex) == md5.size());
        memcpy(&header.md5_hex[0], md5.c_str(), md5.size());

        for (int i = 0; i < elements.size(); ++i) {
            keys_ptr_vec.push_back(elements[i].word.data());
            values_vec.push_back(i);
            mem_elem_vec.emplace_back();
            auto & mem_elem = mem_elem_vec.back();
            mem_elem.weight = elements[i].weight;
            mem_elem.SetTag(elements[i].tag);
        }

        auto const ret = dat_.build(keys_ptr_vec.size(), &keys_ptr_vec[0], nullptr, &values_vec[0]);
        if (0 != ret) {
            XLOG(ERROR) << "Build double array trie error";
            return Error::BuildTrieError;
        }
        header.elements_num = mem_elem_vec.size();
        header.dat_size = dat_.size();

        {
            string tmp_filepath = string(dat_cache_file) + "_XXXXXX";
            ::umask(S_IWGRP | S_IWOTH);
            const int fd = ::mkstemp(&tmp_filepath[0]);
            if (fd < 0) {
                XLOG(ERROR) << "make temporary file failed";
                return Error::OpenFileFailed;
            }
            auto st = ::fchmod(fd, 0644);
            if (st != 0) {
                XLOG(ERROR) << "change temporary file mode to 0644 failed. Please check permission";
                return Error::FileOperationError;
            }

            auto write_bytes = ::write(fd, (const char *)&header, sizeof(header));
            write_bytes += ::write(fd, (const char *)&mem_elem_vec[0], sizeof(mem_elem_vec[0]) * mem_elem_vec.size());
            write_bytes += ::write(fd, dat_.array(), dat_.total_size());

            // assert(write_bytes == sizeof(header) + mem_elem_vec.size() * sizeof(mem_elem_vec[0]) + dat_.total_size());
            if (write_bytes != sizeof(header) + mem_elem_vec.size() * sizeof(mem_elem_vec[0]) + dat_.total_size()) {
                XLOG(ERROR) << "check written data size failed. ";
                return Error::FileOperationError;
            }
            st = ::close(fd);
            if (st != 0) {
                XLOG(ERROR) << "Closing " << tmp_filepath << " failed";
                return Error::FileOperationError;
            }

            const auto rename_ret = ::rename(tmp_filepath.c_str(), dat_cache_file.c_str());
            if (0 != rename_ret) {
                XLOG(ERROR) << "rename " << tmp_filepath << " to " << dat_cache_file << " failed. ";
                return Error::FileOperationError;
            }
        }

        return Error::Ok;
    }


private:
    JiebaDAT dat_;
    const DatMemElem * elements_ptr_ = nullptr;
    size_t elements_num_ = 0;
    double min_weight_ = 0;

    int mmap_fd_ = -1;
    size_t mmap_length_ = 0;
    char * mmap_addr_ = nullptr;
};


inline Error CalcFileListMD5(const vector<string>& files, size_t& file_size_sum, string& md5sum) {
    limonp::MD5 md5;
    file_size_sum = 0;

    for (auto const & local_path : files) {
        const int fd = ::open(local_path.c_str(), O_RDONLY);
        if (fd < 0) {
            XLOG(ERROR) << "failed to open " << local_path;
            return Error::OpenFileFailed;
        }
        auto const len = ::lseek(fd, 0, SEEK_END);
        if (len > 0) {
            void * addr = ::mmap(nullptr, len, PROT_READ, MAP_SHARED, fd, 0);
            if (MAP_FAILED == addr) {
                XLOG(ERROR) << "mmap for " << local_path << " failed";
                return Error::MmapError;
            };

            md5.Update((unsigned char *) addr, len);
            file_size_sum += len;

            auto st = ::munmap(addr, len);
            if (st != 0) {
                XLOG(ERROR) << "munmap for " << local_path << " failed";
                return Error::MmapError;
            }
        }
        ::close(fd);
    }

    md5.Final();
    md5sum = md5.digestChars;
    return Error::Ok;
}

}
