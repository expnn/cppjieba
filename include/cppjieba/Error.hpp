//
// Created by cyc on 22-4-30.
//

#ifndef CPPJIEBA_ERROR_HPP
#define CPPJIEBA_ERROR_HPP

enum Error {
    Ok,
    OpenFileFailed,
    FileOperationError,
    MmapError,
    ValueError,
    BuildTrieError,
};

#endif //CPPJIEBA_ERROR_HPP
