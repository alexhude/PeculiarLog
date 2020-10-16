//
//  SearchEngine.cpp
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

#include <sys/stat.h>
#include <sys/mman.h>

#include <thread>   // std::thread::hardware_concurrency()

#include "SearchEngine.hpp"
#include "HyperscanEngine.hpp"

#define DEBUG_BLOCKS 1

const char*  SearchEngine::s_eolPattern = "\n";

SearchEngine::SearchEngine()
{
    m_size = 0;
}

SearchEngine::~SearchEngine()
{
    
}

SearchEngineError SearchEngine::init(const char* file)
{
    m_fd = ::open(file, O_RDONLY);
    if(m_fd < 0) {
        printf("[!] failed to open %s\n", file);
        return FileOpenFailed;
    }
    
    struct stat stat_buf;
    if(fstat(m_fd, &stat_buf) != 0) {
        printf("[!] failed to stat %s\n", file);
        return FileStatFailed;
    }
    
    m_size = stat_buf.st_size;
    
    m_mem = (const char*)mmap(nullptr, m_size, PROT_READ, MAP_FILE | MAP_SHARED, m_fd, 0);
    if (m_mem == MAP_FAILED) {
        printf("[!] mmap failed: %d.\n", errno);
        return FileMapFailed;
    }
    
    return NoError;
}

uint64_t SearchEngine::totalBytes()
{
    return m_size;
}

uint32_t SearchEngine::formatBlocks()
{
    uint32_t cores = std::thread::hardware_concurrency();
    uint32_t blocks = (m_size > 1024*1024)? cores : 1;
    printf("[+] use %d block (available %d logical cores)\n", blocks, cores);
    
    size_t psize = strlen(s_eolPattern);
    uint64_t blockSize = m_size / blocks;
    uint64_t offset = blockSize;
    
    m_blocks[0].byteOffset = 0;
    m_blocks[0].size = 0;
    m_blocks[0].active = true;
    int i = 1;
    for (; i < blocks; i++) {
        m_blocks[i].active = true;
        m_blocks[i].byteOffset = strstr(m_mem + offset, s_eolPattern) - m_mem + psize; // FIXME: !!! if file doesn't end with \n
        m_blocks[i-1].size = m_blocks[i].byteOffset - m_blocks[i-1].byteOffset;
        offset += blockSize;
    }
    m_blocks[i-1].size = m_size - m_blocks[i-1].byteOffset;
    m_blockCount = blocks;
    
    return blocks;
}

SearchEngineError SearchEngine::mergeScope(uint32_t *filteredLines)
{
    if (!filteredLines)
        return BadArgument;
    
    uint32_t extraLines = 0;
    int32_t headLines = 0;
    int32_t tailLines = 0;
    int32_t linesLeft = 0;
    int32_t carry = 0;
    for (int i = 1; i < m_blockCount; i++) {
        headLines = m_blocks[i].headLines;
        if (m_blocks[i-1].filteredLines)
            tailLines = m_blocks[i-1].tailLines;
        else
            tailLines = m_blocks[i-1].headLines - carry;
        linesLeft = headLines + tailLines;
        carry = 0;
        if(tailLines < 0) {
            carry = (linesLeft > 0)? headLines - linesLeft : (headLines > 0)? headLines : 0;
            m_blocks[i].lendedTailLines = carry;
            if (m_beforeTracker[i].getSize() < carry)
                m_beforeTracker[i].setSize(carry);
        #if DEBUG_BLOCKS
            printf("[#] merge %d line(s) <= into block %d tail <%+d> from block %d head <%+d>\n", carry, i-1, tailLines, i, headLines);
        #endif
        } else if (headLines < 0) {
            carry = (linesLeft > 0)? tailLines - linesLeft : (tailLines > 0)? tailLines : 0;
            m_blocks[i-1].lendedHeadLines = carry;
            if (m_afterTracker[i-1].getSize() < carry)
                m_afterTracker[i-1].setSize(carry);
        #if DEBUG_BLOCKS
            printf("[#] merge %d line(s) => from block %d tail <%+d> into block %d head <%+d>\n", carry, i-1, tailLines, i, headLines);
        #endif
        } else {
            carry = 0;
        }
        extraLines += carry;
    }

    *filteredLines += extraLines;
    
    return NoError;
}

void SearchEngine::close()
{
    if (m_fd >= 0) {
        munmap((void*)m_mem, m_size);
        ::close(m_fd);
    }
}

bool SearchEngine::isFiltered()
{
    return m_filtered;
}

// MARK: - C export

#ifdef __cplusplus
extern "C" {
#endif
    
    SearchEngineError se_init(const char* file, struct SEContext* context) {
        if (!context)
            return InvalidContext;
        
    #if SE_SUPPORT_HYPERSCAN
        if (context->back == Hyperscan) {
            context->engine = new HyperscanEngine();
        } else
    #endif
        {
            printf("[!] unknown engine back\n");
            return BadArgument;
        }
        
        if (context->engine->init(file) != NoError) {
            printf("[!] unable to init engine\n");
            return InitFailed;
        }
        
        context->blocks = context->engine->formatBlocks();
        context->bytes = context->engine->totalBytes();
        
        return NoError;
    }
    
    SearchEngineError se_fetch(struct SEContext* context, uint32_t blockIdx, SEBlockInfo* info) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->fetch(blockIdx, info);
    }

    SearchEngineError se_merge_scope(struct SEContext* context, uint32_t* filteredLines) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->mergeScope(filteredLines);
    }
    
    SearchEngineError se_get_line(struct SEContext* context, uint32_t lineNumber, struct SELineInfo* lineInfo) {
        if (! (context && context->engine))
            return InvalidContext;

        return context->engine->getLine(lineNumber, lineInfo);
    }

    SearchEngineError   se_get_row_for_abs_line(struct SEContext* context, uint32_t absLine, uint32_t* row) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->getRowForAbsLine(absLine, row);
    }

    bool se_is_filtered(struct SEContext* context) {
        if (! (context && context->engine))
            return 0;
        
        return context->engine->isFiltered();
    }
    
    SearchEngineError se_set_literal(struct SEContext* context, const char* literal) {
        return NotSupported;
    }
    
    SearchEngineError se_set_ignore_case(struct SEContext* context, bool ignoreCase) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->setIgnoreCase(ignoreCase);
    }
    
    SearchEngineError se_set_scope(struct SEContext* context, uint32_t before, uint32_t after) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->setScope(before, after);
    }

    SearchEngineError se_set_pattern(struct SEContext* context, const char* pattern, char* error) {
        if (! (context && context->engine))
            return InvalidContext;
        
        if (error) {
            memset(error, 0, MAX_ERROR_LENGTH + 1);
        }
        
        return context->engine->setPattern(pattern, error);
    }
    
    SearchEngineError se_filter(struct SEContext* context, uint32_t blockIdx, struct SEBlockInfo* info) {
        if (! (context && context->engine))
            return InvalidContext;
        
        return context->engine->filter(blockIdx, info);
    }
    
    void se_destroy(struct SEContext* context) {
        if (context->engine) {
            context->engine->close();
            delete context->engine;
            context->engine = nullptr;
        }
    }
    
#ifdef __cplusplus
}
#endif
