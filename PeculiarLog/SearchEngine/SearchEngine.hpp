//
//  SearchEngine.hpp
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

#pragma once

#define NDEBUG

#include <stdio.h>
#include <stdint.h>

// for CF_ENUM
#include "CoreFoundation/CoreFoundation.h"

// MARK: - Config

#define SE_SUPPORT_HYPERSCAN    1

// MARK: - C header

#ifdef __cplusplus
    class SearchEngine;
    #define SEARCH_ENGINE_TYPE   SearchEngine*
#else
    #define SEARCH_ENGINE_TYPE   void*
#endif

#ifdef __cplusplus
extern "C" {
#endif

    static const uint32_t MAX_BLOCK_COUNT   = 40;
    static const uint32_t MAX_SCOPE_BEFORE  = 10;
    static const uint32_t MAX_SCOPE_AFTER   = 10;
    static const uint32_t MAX_ERROR_LENGTH  = 64;

    typedef CF_ENUM(int, SearchEngineError) {
        NoError,
        BadArgument,
        NotSupported,
        InvalidContext,
        FileOpenFailed,
        FileStatFailed,
        FileMapFailed,
        InitFailed,
        EngineOpFailed,
        
        UnknownError
    };

    typedef CF_ENUM(int, SearchEngineBack) {
        Hyperscan
    };

    struct SEContext {
        SearchEngineBack        back;
        SEARCH_ENGINE_TYPE      engine;
        uint32_t                blocks;
        uint64_t                bytes;
    };

    struct SEBlockInfo {
        uint32_t    lines;
        uint32_t    maxLength;
    };
    
    struct SELineInfo {
        const char* line;
        uint32_t    length;
        uint32_t    number;
        bool        scope;
    };
    
    SearchEngineError   se_init(const char* file, struct SEContext* context);
    SearchEngineError   se_fetch(struct SEContext* context, uint32_t blockIdx, struct SEBlockInfo* info);
    SearchEngineError   se_merge_scope(struct SEContext* context, uint32_t* filteredLines);
    SearchEngineError   se_get_line(struct SEContext* context, uint32_t lineNumber, struct SELineInfo* lineInfo);
    SearchEngineError   se_get_row_for_abs_line(struct SEContext* context, uint32_t absLine, uint32_t* row);
    bool                se_is_filtered(struct SEContext* context);
    SearchEngineError   se_set_literal(struct SEContext* context, const char* literal);
    SearchEngineError   se_set_pattern(struct SEContext* context, const char* pattern, char* error);
    SearchEngineError   se_set_ignore_case(struct SEContext* context, bool ignoreCase);
    SearchEngineError   se_set_scope(struct SEContext* context, uint32_t before, uint32_t after);
    SearchEngineError   se_filter(struct SEContext* context, uint32_t blockIdx, struct SEBlockInfo* info);
    void                se_destroy(struct SEContext* context);

#ifdef __cplusplus
}
#endif

// MARK: - C++ header

#ifdef __cplusplus

#include "ScopeTracker.hpp"

class SearchEngine {
    
public:
    SearchEngine();
    virtual ~SearchEngine();
    
    virtual SearchEngineError   init(const char* file);
            uint64_t            totalBytes();
            uint32_t            formatBlocks();
    virtual SearchEngineError   fetch(uint32_t blockIdx, SEBlockInfo* info) = 0;
            SearchEngineError   mergeScope(uint32_t* filteredLines);
    virtual void                close();

    virtual SearchEngineError   getLine(uint32_t number, SELineInfo* lineInfo) = 0;
    virtual SearchEngineError   getRowForAbsLine(uint32_t absLine, uint32_t* row) = 0;
    
            bool                isFiltered();
    virtual SearchEngineError   setPattern(const char* pattern, char* error) = 0;
    virtual SearchEngineError   setIgnoreCase(bool ignoreCase) = 0;
    virtual SearchEngineError   setScope(uint32_t before, uint32_t after) = 0;
    virtual SearchEngineError   filter(uint32_t blockIdx, SEBlockInfo* info) = 0;
    
protected:
    
    static const char*  s_eolPattern;
    
protected:

    const char*     m_mem   = nullptr;
    size_t          m_size  = 0;
    
    // optimizations
    struct SEBlock {
        bool        active;             // block is in use
        uint64_t    byteOffset;         // block start address within the file
        uint32_t    lines;              // total number of lines
        uint32_t    filteredLines;      // total number of pattern matching lines
        uint32_t    scopeLines;         // total number of scope lines
        int32_t     headLines;          // number of spare lines at the beginning of a block
        int32_t     tailLines;          // number of spare lines at the end of a block
        int32_t     lendedHeadLines;    // number of lines to lend to the head of next block
        int32_t     lendedTailLines;    // number of lines to lend to the tail of previous block
        uint64_t    size;               // block size
    };
    
    SEBlock         m_blocks[MAX_BLOCK_COUNT] = {0};
    uint32_t        m_blockCount = 0;

    // getting lines
    uint32_t        m_recentBlock = 0;
    uint32_t        m_recentLineOffset = 0;
    uint32_t        m_recentAbsLineOffset = 0;
    uint32_t        m_predictedAbsLineNum = 0;
    uint32_t        m_predictedLineNum = 0;
    uint64_t        m_predictedLinePos = 0;
    
    // filter
    bool            m_filtered = false;
    bool            m_ignoreCase = false;
    uint32_t        m_scopeBefore = 0;
    uint32_t        m_scopeAfter = 0;

    ScopeTracker<MAX_SCOPE_BEFORE, TrackingPolicy::Ring>  m_beforeTracker[MAX_BLOCK_COUNT];
    ScopeTracker<MAX_SCOPE_AFTER, TrackingPolicy::Fixed>  m_afterTracker[MAX_BLOCK_COUNT];
    
private:
    
    int             m_fd    = -1;
    
};
#endif
