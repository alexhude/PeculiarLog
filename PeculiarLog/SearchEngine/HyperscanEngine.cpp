//
//  HyperscanEngine.cpp
//  PeculiarLog
//
//  Created by Alexander Hude on 14/11/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

#include <algorithm>
#include <functional>
#include "HyperscanEngine.hpp"

#define DEBUG_BLOCKS    0
#define DEBUG_GETLINE   0

static const unsigned int SE_HS_EOL_ID      = 0x5EE0;
static const unsigned int SE_HS_PATTERN_ID  = 0x5EAA;
unsigned int HyperscanEngine::s_filterIDs[2] = {
    SE_HS_EOL_ID,
    SE_HS_PATTERN_ID,
};

template<typename Func>
hs_error_t HS_CDECL hs_scan(const hs_database_t *db, const char *data, unsigned int length, unsigned int flags, hs_scratch_t *scratch, Func func)
{
    auto closure = [](unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int
    {
        return (*(Func*)ctx)(id, from, to, flags, ctx);
    };
    
    return hs_scan(db, data, length, flags, scratch, closure, &func);
}

HyperscanEngine::HyperscanEngine() {}
HyperscanEngine::~HyperscanEngine() {}

SearchEngineError HyperscanEngine::init(const char* file)
{
    SearchEngineError err = SearchEngine::init(file);

    hs_platform_info_t pi;
    hs_populate_platform(&pi);
    if (pi.cpu_features & HS_CPU_FEATURES_AVX512) {
        printf("[+] host supports AVX512 instruction set\n");
    }
    
    hs_compile_error_t *compile_err;
    m_eolDB = nullptr;
    if (hs_compile_lit(s_eolPattern, 0, strlen(s_eolPattern), HS_MODE_BLOCK, &pi, &m_eolDB, &compile_err) != HS_SUCCESS) {
        printf("[!] unable to compile EOL pattern: %s\n", compile_err->message);
        hs_free_compile_error(compile_err);
        return UnknownError;
    }
    
    m_baseScratch = nullptr;
    auto res = hs_alloc_scratch(m_eolDB, &m_baseScratch);
    if (res != HS_SUCCESS) {
        printf("[!] unable to allocate scratch space for EOL\n");
        hs_free_database(m_eolDB);
        m_eolDB = nullptr;
        return UnknownError;
    }
    
    m_patternDB = nullptr;
    m_filterScratch = nullptr;
    for (int block=0; block < MAX_BLOCK_COUNT; block++) {
        m_scratchPool[block] = nullptr;
    }
    
    m_predictedAbsLineNum = -1;
    m_predictedLinePos = -1;
    m_predictedLineNum = -1;
    m_recentBlock = -1;
    m_recentLineOffset = 0;
    m_recentAbsLineOffset = 0;
    
    return err;
}

SearchEngineError HyperscanEngine::fetch(uint32_t blockIdx, SEBlockInfo* info)
{
    if (!info)
        return BadArgument;
    
    if (blockIdx > MAX_BLOCK_COUNT - 1)
        return BadArgument;
    
    if (!m_blocks[blockIdx].active)
        return BadArgument;
    
    hs_clone_scratch(m_baseScratch, &m_scratchPool[blockIdx]);
    
    uint64_t pos = m_blocks[blockIdx].byteOffset;
    uint64_t size = m_blocks[blockIdx].size;

    uint64_t lastHit = 0;
    auto res = hs_scan(m_eolDB, m_mem + pos, (unsigned int)size, 0, m_scratchPool[blockIdx],
        [&, &lines = info->lines, &maxLength = info->maxLength]
        (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
            // for every EOL update max length and increment match line counter
            if ((to - lastHit - 1) > maxLength) {
                maxLength = uint32_t(to - lastHit - 1);
            }
            lastHit = to;
            lines++;
            return 0;
        }
    );
    
    if (res != HS_SUCCESS) {
        printf("[!] unable to fetch lines\n");
        return EngineOpFailed;
    }

    m_blocks[blockIdx].lines = info->lines;
    
#if DEBUG_BLOCKS
    printf("[#] fetch block %2d ready (%d lines, %d cols)\n", blockIdx, info->lines, info->maxLength);
#endif
    
    return NoError;
}

void HyperscanEngine::close()
{
    for (int i=0; i < MAX_BLOCK_COUNT; i++) {
        if (m_scratchPool[i])
            hs_free_scratch(m_scratchPool[i]);
    }
    
    if (m_filterScratch) {
        hs_free_scratch(m_filterScratch);
        m_filterScratch = nullptr;
    }
    
    if (m_baseScratch) {
        hs_free_scratch(m_baseScratch);
        m_baseScratch = nullptr;
    }

    if (m_patternDB) {
        hs_free_database(m_patternDB);
        m_patternDB = nullptr;
    }
    
    if (m_eolDB) {
        hs_free_database(m_eolDB);
        m_eolDB = nullptr;
    }
    
    SearchEngine::close();
}

SearchEngineError HyperscanEngine::getLine(uint32_t number, SELineInfo* lineInfo)
{
    if (!lineInfo)
        return BadArgument;
    
    if (! m_filtered) {
        
        int blockIdx = 0;
        uint32_t lineOffset = 0;
        if (number > m_recentLineOffset) {
            blockIdx = m_recentBlock;
            lineOffset = m_recentLineOffset;
        } else {
            lineOffset = m_blocks[blockIdx].lines;
        }
        
        uint32_t currentLine = 0;
        while (number >= lineOffset) {
            currentLine = lineOffset;
            lineOffset += m_blocks[++blockIdx].lines;
        }
        
        if (blockIdx != m_recentBlock) {
            m_predictedAbsLineNum = -1;
            m_predictedLineNum = -1;
            m_predictedLinePos = -1;
        }
        
        uint64_t basePos = m_blocks[blockIdx].byteOffset;
        uint64_t pos = basePos;
        if (number == m_predictedLineNum) {
            currentLine = m_predictedLineNum;
            pos = m_predictedLinePos;
        }

        uint64_t scanSize = m_blocks[blockIdx].size - (pos - basePos);

        uint64_t lastHit = 0;
        auto res = hs_scan(m_eolDB, m_mem + pos, (unsigned int)scanSize, 0, m_baseScratch,
            [&, &length = lineInfo->length]
            (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                // get line length and return when line counter matches target number
                if (currentLine == number) {
                    length = uint32_t(to - lastHit - 1);
                    return 1;
                }
                lastHit = uint32_t(to);
                currentLine++;
                return 0;
            }
        );
        
        if (!(res == HS_SUCCESS || res == HS_SCAN_TERMINATED)) {
            printf("[!] unable to find line %d\n", number);
            return UnknownError;
        }

        lineInfo->line = m_mem + pos + lastHit;
        lineInfo->number = number;
        lineInfo->scope = false;
        m_predictedLinePos = pos + lastHit + lineInfo->length + 1;
        m_predictedLineNum = number + 1;
        m_recentBlock = blockIdx;
        m_recentLineOffset = lineOffset;
        
        // skip \r at the end of the line if exists
        if (lineInfo->length && lineInfo->line[lineInfo->length-1] == '\r')
            lineInfo->length--;
    } else {
        int blockIdx = 0;

        lineInfo->number = 0;
        uint32_t lineOffset = 0;
        uint32_t absLineOffset = 0;
        if (number > m_recentLineOffset) {
            blockIdx = m_recentBlock;
            lineOffset = m_recentLineOffset;
            absLineOffset = m_recentAbsLineOffset;
        } else {
            lineOffset = m_blocks[blockIdx].filteredLines + m_blocks[blockIdx].borrowHeadLines + m_blocks[blockIdx].borrowTailLines;
            absLineOffset = m_blocks[blockIdx].lines;
        }
        
        uint32_t currentLine = 0;
        while (number >= lineOffset) {
            currentLine = lineOffset;
            lineInfo->number = absLineOffset;
            blockIdx++;
            lineOffset += m_blocks[blockIdx].filteredLines + m_blocks[blockIdx].borrowHeadLines + m_blocks[blockIdx].borrowTailLines;
            absLineOffset += m_blocks[blockIdx].lines;
        }
        
        uint32_t baseLine = currentLine;
        if (blockIdx != m_recentBlock) {
        #if DEBUG_GETLINE
            printf("[#] unexpected block %d (expected %d)\n", blockIdx, m_recentBlock);
        #endif
            m_beforeTracker[blockIdx].reset();
            m_afterTracker[blockIdx].reset();
            m_predictedAbsLineNum = -1;
            m_predictedLineNum = -1;
            m_predictedLinePos = -1;
        }
        
        uint64_t basePos = m_blocks[blockIdx].byteOffset;
        uint64_t searchPos = basePos;
        if (number != m_predictedLineNum) {
        #if DEBUG_GETLINE
            printf("[#] unexpected line %d (expected %d)\n", number, m_predictedLineNum);
        #endif
            m_beforeTracker[blockIdx].reset();
            m_afterTracker[blockIdx].reset();
            m_predictedAbsLineNum = -1;
            m_predictedLineNum = -1;
            m_predictedLinePos = -1;
        } else {
            lineInfo->number = m_predictedAbsLineNum;
            currentLine = m_predictedLineNum;
            searchPos = m_predictedLinePos;
        }
        uint64_t scanSize = m_blocks[blockIdx].size - (searchPos - basePos);
        
        uint64_t lastHit = 0;
        bool patternMatch = false;
        
    #if DEBUG_GETLINE
        printf("[#] get line %d for block %2d starting with line %d at offset %lld\n", number, blockIdx, currentLine, searchPos);
    #endif
        
        if (m_scopeBefore || m_scopeAfter) {
            // advanced search with scope support
            auto btracker = &m_beforeTracker[blockIdx];
            auto atracker = &m_afterTracker[blockIdx];

            if (m_blocks[blockIdx].filteredLines) {
                
                bool lineFound = false;
                uint32_t borrowTailLines = m_blocks[blockIdx].borrowTailLines;
                uint32_t borrowHeadLines = m_blocks[blockIdx].borrowHeadLines;
                if (btracker->hasBaseLine()) {
                    // return 'before' scope including base line
                    if (! btracker->isEmpty()) {
                        __unused auto topLine = btracker->getTopScopeLine();
                        assert(currentLine == topLine);
                        // search offset points to the right line, pop scope line
                        __unused auto curPos = btracker->popScope(lineInfo->length);
                        assert(curPos == searchPos);
                        lineInfo->scope = true;
                        lineFound = true;
                    } else {
                        __unused auto topLine = btracker->getTopScopeLine();
                        assert(currentLine == topLine);
                        // search offset points to the right line, pop base line
                        __unused auto curPos = btracker->popBaseLine(lineInfo->length);
                        assert(curPos == searchPos);
                        lineInfo->scope = false;
                        lineFound = true;

                        // reset 'before' tracker if we reached match line
                        btracker->reset();
                    }
                } else if (currentLine > lineOffset - borrowHeadLines) {
                    if (btracker->getCount()) {
                        uint64_t curPos = btracker->popScope(lineInfo->length);
                        assert(curPos != -1);
                        
                        lastHit = curPos - searchPos;
                        lineInfo->scope = true;
                        lineFound = true;
                    }
                }

                if (!lineFound) {
                    // search filtered line with scope
                    auto res = hs_scan(m_patternDB, m_mem + searchPos, (unsigned int)scanSize, 0, m_filterScratch,
                        [&, &btracker = btracker, &atracker = atracker, &absNumber = lineInfo->number, &length = lineInfo->length, &isScope = lineInfo->scope]
                        (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                            if (id == SE_HS_EOL_ID) {
                                // for every EOL match check if we have pattern match within this line
                                // in this case get line length and return when line counter matches target number
                                if (patternMatch) {
                                    uint32_t scopeBaseLine = currentLine + btracker->getCount();
                                    
                                    // setup scope trackers
                                    atracker->reset();
                                    atracker->pushBaseLine(scopeBaseLine, lastHit + searchPos, uint32_t(to - lastHit - 1));
                                    btracker->pushBaseLine(scopeBaseLine, lastHit + searchPos, uint32_t(to - lastHit - 1));

                                    // start returning 'before' scope
                                    if (btracker->getCount()) {
                                        absNumber -= btracker->getCount();
                                        uint64_t curPos = btracker->popScope(length);
                                        while(curPos != -1) {
                                            if (currentLine == number) {
                                                lastHit = curPos - searchPos;
                                                isScope = true;
                                                return 1;
                                            }
                                            curPos = btracker->popScope(length);
                                            currentLine++;
                                            absNumber++;
                                        }
                                    }
                                    
                                    // reset 'before' tracker if we reached match line
                                    btracker->reset();

                                    // check if we reached desired pattern match
                                    if (currentLine == number) {
                                        length = uint32_t(to - lastHit - 1);
                                        isScope = false;
                                        return 1;
                                    }
                                    currentLine++;
                                } else {
                                    uint32_t len = uint32_t(to - lastHit - 1);
                                    
                                    // keep tracking scope lines
                                    
                                    if (atracker->hasBaseLine()) {
                                        if (!atracker->isFull()) {
                                            // return 'after' scope
                                            if (!atracker->pushScope(lastHit + searchPos, len))
                                                atracker->reset();
                                            if (currentLine == number) {
                                                length = len;
                                                isScope = true;
                                                return 1;
                                            }
                                            currentLine++;
                                        } else {
                                            btracker->pushScope(lastHit + searchPos, len); // TODO: investigate
                                        }
                                    } else if (currentLine < baseLine + borrowTailLines) {
                                        if (currentLine == number) {
                                            length = uint32_t(to - lastHit - 1);
                                            isScope = true;
                                            return 1;
                                        }
                                        currentLine++;
                                    } else {
                                        btracker->pushScope(lastHit + searchPos, len); // TODO: investigate
                                    }
                                }
                                // save pointer to the next line, reset pattern match flag
                                lastHit = to;
                                absNumber++;
                                patternMatch = false;
                            } else if (id == SE_HS_PATTERN_ID) {
                                // for every pattern match within the line set flag
                                patternMatch = true;
                            }
                            return 0;
                        }
                    );
                    if (!(res == HS_SUCCESS || res == HS_SCAN_TERMINATED)) {
                        printf("[!] unable to find filtered line %d with scope\n", number);
                        return UnknownError;
                    }

                    // borrow head search should reach the end of block
                    if (borrowHeadLines && res == HS_SUCCESS) {
                        if (btracker->getCount()) {
                            uint64_t curPos = btracker->popScope(lineInfo->length);
                            assert(curPos != -1);
                            
                            lastHit = curPos - searchPos;
                            lineInfo->scope = true;
                        }
                    }
                }
            } else {
                // search borrowed line with scope in the block without matches
                bool lineFound = false;
                uint32_t borrowTailLines = m_blocks[blockIdx].borrowTailLines;
                uint32_t borrowHeadLines = m_blocks[blockIdx].borrowHeadLines;
                
                if (currentLine > lineOffset - borrowHeadLines) {
                    if (btracker->getCount()) {
                        uint64_t curPos = btracker->popScope(lineInfo->length);
                        assert(curPos != -1);
                        
                        lastHit = curPos - searchPos;
                        lineInfo->scope = true;
                        lineFound = true;
                    }
                }
                
                if (!lineFound) {
                    auto res = hs_scan(m_eolDB, m_mem + searchPos, (unsigned int)scanSize, 0, m_baseScratch,
                        [&, &absNumber = lineInfo->number, &length = lineInfo->length, &isScope = lineInfo->scope]
                        (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                            uint32_t len = uint32_t(to - lastHit - 1);
                            if (currentLine < baseLine + borrowTailLines) {
                                // return lines for the tail of previous block
                                if (currentLine == number) {
                                    length = len;
                                    isScope = true;
                                    return 1;
                                }
                                currentLine++;
                            }
                            if (borrowHeadLines) {
                                // borrow lines to the head of next block
                                btracker->pushScope(lastHit + searchPos, len);
                            }
                            lastHit = to;
                            absNumber++;
                            return 0;
                        }
                    );
                    if (!(res == HS_SUCCESS || res == HS_SCAN_TERMINATED)) {
                        printf("[!] unable to find borrowed line %d with scope\n", number);
                        return UnknownError;
                    }
                    
                    // borrow head search should reach the end of block
                    if (borrowHeadLines && res == HS_SUCCESS) {
                        if (btracker->getCount()) {
                            uint64_t curPos = btracker->popScope(lineInfo->length);
                            assert(curPos != -1);
                            
                            lastHit = curPos - searchPos;
                            lineInfo->scope = true;
                        }
                    }
                }
            }
        } else {
            // optimized search without scope support
            auto res = hs_scan(m_patternDB, m_mem + searchPos, (unsigned int)scanSize, 0, m_filterScratch,
                [&, &absNumber = lineInfo->number, &length = lineInfo->length]
                (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                    if (id == SE_HS_EOL_ID) {
                        // for every EOL match check if we have pattern match within this line
                        // in this case get line length and return when line counter matches target number
                        if (patternMatch) {
                            if (currentLine == number) {
                                length = uint32_t(to - lastHit - 1);
                                return 1;
                            }
                            currentLine++;
                        }
                        // save pointer to the next line, reset pattern match flag
                        lastHit = to;
                        absNumber++;
                        patternMatch = false;
                    } else if (id == SE_HS_PATTERN_ID) {
                        // for every pattern match within the line set flag
                        patternMatch = true;
                    }
                    return 0;
                }
            );
            if (!(res == HS_SUCCESS || res == HS_SCAN_TERMINATED)) {
                printf("[!] unable to find filtered line %d\n", number);
                return UnknownError;
            }
            lineInfo->scope = false;
        }

        lineInfo->line = m_mem + searchPos + lastHit;
        m_predictedLinePos = searchPos + lastHit + lineInfo->length + 1;
        m_predictedAbsLineNum = lineInfo->number + 1;
        m_predictedLineNum = number + 1;
        m_recentBlock = blockIdx;
        m_recentLineOffset = lineOffset;
        m_recentAbsLineOffset = absLineOffset;
        
        // skip \r at the end of the line if exists
        if (lineInfo->length && lineInfo->line[lineInfo->length-1] == '\r')
            lineInfo->length--;
    }
    
    return NoError;
}

SearchEngineError HyperscanEngine::setIgnoreCase(bool ignoreCase)
{
    m_ignoreCase = ignoreCase;
    return NoError;
}

SearchEngineError HyperscanEngine::setScope(uint32_t before, uint32_t after)
{
    m_scopeBefore = before;
    m_scopeAfter = after;
    printf("[+] set scope B%d A%d\n", m_scopeBefore, m_scopeAfter);

    for (int block=0; block < MAX_BLOCK_COUNT; block++) {
        m_beforeTracker[block].setSize(m_scopeBefore);
        m_afterTracker[block].setSize(m_scopeAfter);
    }

    return NoError;
}

SearchEngineError HyperscanEngine::setPattern(const char* pattern)
{
    const char* patterns[2] = {
        s_eolPattern,
        pattern,
    };
    
    unsigned int flags[2] = {
        HS_FLAG_DOTALL,
        0
    };
    
    flags[1] = (m_ignoreCase)? HS_FLAG_CASELESS : 0;

    printf("[+] set pattern = \"%s\"\n", pattern);
    
    if (pattern[0] == 0)
        m_filtered = false;
    else
        m_filtered = true;
    
    if (m_filtered) {
        if (m_patternDB) {
            hs_free_database(m_patternDB);
            m_patternDB = nullptr;
        }
        m_patternDB = nullptr;
        hs_compile_error_t *compile_err;
        if (hs_compile_multi(patterns, flags, s_filterIDs, 2, HS_MODE_BLOCK, nullptr, &m_patternDB, &compile_err) != HS_SUCCESS) {
            printf("[!] unable to compile filter pattern: %s\n", compile_err->message);
            hs_free_compile_error(compile_err);
            return UnknownError;
        }

        if (m_filterScratch) {
            hs_free_scratch(m_filterScratch);
            m_filterScratch = nullptr;
        }
        
        if (hs_alloc_scratch(m_patternDB, &m_filterScratch) != HS_SUCCESS) {
            printf("[!] unable to allocate scratch space for filter\n");
            hs_free_database(m_patternDB);
            m_patternDB = nullptr;
            return UnknownError;
        }
        
        for (int block=0; block < MAX_BLOCK_COUNT; block++) {
            m_blocks[block].filteredLines = 0;
            m_blocks[block].scopeLines = 0;
            m_blocks[block].headLines = 0;
            m_blocks[block].tailLines = 0;
            m_blocks[block].borrowHeadLines = 0;
            m_blocks[block].borrowTailLines = 0;
            m_beforeTracker[block].reset();
            m_afterTracker[block].reset();
        }
    }
    
    m_predictedLinePos = -1;
    m_predictedLineNum = -1;
    m_recentBlock = -1;
    m_recentLineOffset = 0;
    m_recentAbsLineOffset = 0;
    
    return NoError;
}

SearchEngineError HyperscanEngine::filter(uint32_t blockIdx, SEBlockInfo* info)
{
    if (!m_filtered)
        return NoError;
    
    if (!info)
        return BadArgument;
    
    if (blockIdx > MAX_BLOCK_COUNT - 1)
        return BadArgument;
    
    if (!m_blocks[blockIdx].active)
        return BadArgument;
    
    if (m_scratchPool[blockIdx]) {
        hs_free_scratch(m_scratchPool[blockIdx]);
        m_scratchPool[blockIdx] = nullptr;
    }
    hs_clone_scratch(m_filterScratch, &m_scratchPool[blockIdx]);
    
    auto block = &m_blocks[blockIdx];
    uint64_t pos = block->byteOffset;
    uint64_t size = block->size;
    
    uint32_t maxLength = 0;
    uint64_t lastHit = 0;
    bool patternMatch = false;
    if (m_scopeBefore || m_scopeAfter) {
        auto btracker = &m_beforeTracker[blockIdx];
        auto atracker = &m_afterTracker[blockIdx];
        auto res = hs_scan(m_patternDB, m_mem + pos, (unsigned int)size, 0, m_scratchPool[blockIdx],
            [&, &btracker = btracker, &atracker = atracker, &block = block, &lines = block->filteredLines]
            (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                if (id == SE_HS_EOL_ID) {
                    // for every EOL match check if we have pattern match within this line
                    // in this case increment match line counter and update max length
                    if (patternMatch) {
                        maxLength = std::max(uint32_t(to - lastHit - 1), maxLength);
                        // for the first pattern match take 'before' lines into account, for all other matches take both
                        if (lines) {
                            maxLength = std::max(atracker->getMaxLength(), std::max(btracker->getMaxLength(), maxLength));
                            block->scopeLines += btracker->getCount() + atracker->getCount();
                        } else {
                            maxLength = std::max(btracker->getMaxLength(), maxLength);
                            block->scopeLines += btracker->getCount();
                            block->headLines = block->tailLines;
                        }
                        
                        block->tailLines = 0;
                        lines++;
                        btracker->reset();
                        atracker->reset();
                    } else {
                        uint32_t len = uint32_t(to - lastHit - 1);
                        // for the first pattern match take 'before' lines into account, for all other matches take both
                        if (lines) {
                            if (!atracker->isFull())
                                atracker->pushScope(lastHit, len);
                            else
                                btracker->pushScope(lastHit, len);
                        } else {
                            btracker->pushScope(lastHit, len);
                        }
                        block->tailLines++;
                    }
                    // save pointer to the next line, reset pattern match flag
                    lastHit = to;
                    patternMatch = false;
                } else {
                    // for every pattern match within the line set flag
                    patternMatch = true;
                }
                
                return 0;
            }
        );
        if (res != HS_SUCCESS) {
            printf("[!] unable to filter lines with scope\n");
            return EngineOpFailed;
        }
        
        // take scope lines after the last match into account
        maxLength = std::max(atracker->getMaxLength(), maxLength);
        block->scopeLines += (block->filteredLines)? atracker->getCount() : 0;
        
        // update head/tail lines for the merge
        block->headLines = (block->filteredLines)? block->headLines - m_scopeBefore : block->tailLines;
        block->tailLines = (block->filteredLines)? block->tailLines - m_scopeAfter : 0;

        block->filteredLines += block->scopeLines;
    } else {
        auto res = hs_scan(m_patternDB, m_mem + pos, (unsigned int)size, 0, m_scratchPool[blockIdx],
            [&, &lines = block->filteredLines]
            (unsigned int id, unsigned long long from, unsigned long long to, unsigned int flags, void *ctx) -> int {
                if (id == SE_HS_EOL_ID) {
                    // for every EOL match check if we have pattern match within this line
                    // in this case increment match line counter and update max length
                    if (patternMatch) {
                        maxLength = std::max(uint32_t(to - lastHit - 1), maxLength);
                        lines++;
                    }
                    // save pointer to the next line, reset pattern match flag
                    lastHit = to;
                    patternMatch = false;
                } else {
                    // for every pattern match within the line set flag
                    patternMatch = true;
                }
                
                return 0;
            }
        );
        if (res != HS_SUCCESS) {
            printf("[!] unable to filter lines\n");
            return EngineOpFailed;
        }
    }
    
    info->lines = block->filteredLines;
    info->maxLength = maxLength;

#if DEBUG_BLOCKS
    printf("[#] filter block %2d ready (%d lines, %2d cols, +%d scope lines %+d|%+d)\n",
           blockIdx, info->lines - block->scopeLines, info->maxLength,
           block->scopeLines, block->headLines, block->tailLines);
#endif
    
    return NoError;
}
