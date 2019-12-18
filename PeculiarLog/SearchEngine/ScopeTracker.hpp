//
//  ScopeTracker.h
//  PeculiarLog
//
//  Created by Alexander Hude on 14/11/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

#pragma once

enum class TrackingPolicy {
    Fixed,
    Ring
};

template <uint32_t MAX_SIZE, TrackingPolicy POLICY>
class ScopeTracker {
    
public:
    
    constexpr uint32_t wrap(uint32_t idx) {
        return idx % m_size;
    }
    
    bool isEmpty() {
        return m_count == 0;
    }
    
    bool isFull() {
        return m_size == m_count;
    }
    
    uint32_t getSize() {
        return m_size;
    }
    
    void setSize(uint32_t size) {
        assert(size < MAX_SIZE);
        m_size = size;
    }
    
    uint32_t getCount() {
        return m_count;
    }
    
    uint32_t getMaxLength() {
        uint32_t length = 0;
        uint32_t cnt = m_count;
        uint32_t index = m_startIndex;
        while (cnt) {
            if (m_scopeLines[index].length > length)
                length = m_scopeLines[index].length;
            index = wrap(index + 1);
            cnt--;
        }
        return length;
    }

    bool hasBaseLine() {
        return m_baseLine != -1;
    }
    
    uint32_t getTopScopeLine() {
        return m_baseLine - m_count;
    }
    
    void pushBaseLine(uint32_t line, uint64_t pos, uint32_t length) {
        if (m_size == 0)
            return;
            
        m_baseLine = line;
        m_baseLinePos = pos;
        m_baseLineLength = length;
    }

    uint64_t popBaseLine(uint32_t& length) {
        if (m_size == 0)
            return -1;

        length = m_baseLineLength;
        m_baseLine = -1;
        return m_baseLinePos;
    }
    
    bool pushScope(uint64_t pos, uint32_t length = 0) {
        if (m_size == 0)
            return false;
        
        if (m_policy == TrackingPolicy::Fixed && m_count == m_size) {
            return false;
        }
        
        m_scopeLines[m_endIndex].pos = pos;
        m_scopeLines[m_endIndex].length = length;
        if (m_count == m_size) {
            m_startIndex = wrap(m_startIndex + 1);
            m_endIndex = m_startIndex;
        } else {
            m_endIndex = wrap(m_endIndex + 1);
            m_count++;
        }
        return true;
    }
    
    uint64_t popScope(uint32_t& length) {
        if (m_size == 0)
            return -1;

        if (m_count == 0)
            return -1;

        m_count--;
        uint32_t index = m_startIndex;
        m_startIndex = wrap(m_startIndex + 1);
        length = m_scopeLines[index].length;
        return m_scopeLines[index].pos;
    }
    
    void reset() {
        m_count = 0;
        
        m_baseLine = -1;
        m_baseLinePos = -1;
        m_baseLineLength = 0;
        
        m_startIndex = 0;
        m_endIndex = 0;
    }
    
private:
    
    struct ScopeEntry {
        uint64_t pos;
        uint32_t length;
    };
    
    uint32_t        m_size = MAX_SIZE;
    TrackingPolicy  m_policy = POLICY;
    uint32_t        m_count = 0;
    
    ScopeEntry  m_scopeLines[MAX_SIZE] = {0};
    uint32_t    m_baseLine = -1;
    uint64_t    m_baseLinePos = -1;
    uint32_t    m_baseLineLength = 0;

    uint32_t    m_startIndex = 0;
    uint32_t    m_endIndex = 0;
};
