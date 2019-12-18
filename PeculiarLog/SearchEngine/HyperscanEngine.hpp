//
//  HyperscanEngine.hpp
//  PeculiarLog
//
//  Created by Alexander Hude on 14/11/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

#pragma once

#include "hs.h"

#include "SearchEngine.hpp"

class HyperscanEngine : public SearchEngine {
    
public:
    HyperscanEngine();
    ~HyperscanEngine();
    
    SearchEngineError   init(const char* file) override;
    SearchEngineError   fetch(uint32_t blockIdx, SEBlockInfo* info) override;
    void                close() override;
    
    SearchEngineError   getLine(uint32_t number, SELineInfo* lineInfo) override;
    
    SearchEngineError   setIgnoreCase(bool ignoreCase) override;
    SearchEngineError   setScope(uint32_t before, uint32_t after) override;
    SearchEngineError   setPattern(const char* pattern) override;
    SearchEngineError   filter(uint32_t blockIdx, SEBlockInfo* info) override;
    
private:
    
    static unsigned int s_filterIDs[2];
    
private:
    
    hs_database_t*      m_eolDB;
    hs_database_t*      m_patternDB;
    hs_scratch_t*       m_baseScratch;
    hs_scratch_t*       m_filterScratch;
    hs_scratch_t*       m_scratchPool[MAX_BLOCK_COUNT] = {0};

};

