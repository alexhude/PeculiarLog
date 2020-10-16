//
//  SearchEngine.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Foundation

class SearchEngine {

    struct ScopeBlocks {
        var head: Int32 = 0
        var tail: Int32 = 0
    }
    
    private var context = SEContext()
    private var scopeBlock : [ScopeBlocks]!
    
    private         var blockLines : [Int]!
    private(set)    var totalLines : UInt32 = 0
    private(set)    var maxEntryLength : Int = 0
    private(set)    var filteredLines : UInt32 = 0
    private(set)    var maxFilteredLength : Int = 0
    private(set)    var ignoreCase : Bool = false

    var lineCount: Int {
        get {
            if (se_is_filtered(&context)) {
                return Int(filteredLines)
            } else {
                return Int(totalLines)
            }
        }
    }
    
    var maxLength: Int {
        get {
            if (se_is_filtered(&context)) {
                return maxFilteredLength
            } else {
                return maxEntryLength
            }
        }
    }
    
    var isFiltered: Bool {
        get {
            return se_is_filtered(&context)
        }
    }
    
    var totalBytesString: String {
        get {
            let formatter:ByteCountFormatter = ByteCountFormatter()
            formatter.countStyle = .binary
            return formatter.string(fromByteCount: Int64(context.bytes))
        }
    }
    
    init(_ file: String) {
        context.back = .Hyperscan
        guard se_init(file, &context) == .NoError else {
            print("[+] unable to init SearchEngine")
            return
        }

        let stime = DispatchTime.now()
        let group = DispatchGroup()
        let queue = DispatchQueue.global()
        scopeBlock = [ScopeBlocks](repeating: ScopeBlocks(), count: Int(context.blocks))
        for i in 0..<context.blocks {
            queue.async(group: group) {
                var blockInfo = SEBlockInfo()
                guard se_fetch(&self.context, i, &blockInfo) == .NoError else {
                    print("[!] unable to load block \(i)")
                    return
                }

                queue.sync() {
                    self.totalLines += blockInfo.lines
                    if (self.maxEntryLength < blockInfo.maxLength) {
                        self.maxEntryLength = Int(blockInfo.maxLength)
                    }
                }
            }
        }
        
        group.wait()
        let etime = DispatchTime.now()
        let microTime = etime.uptimeNanoseconds - stime.uptimeNanoseconds
        let dtime = Double(microTime) / 1_000_000
        print("[+] engine ready (\(totalLines) lines, \(maxEntryLength) cols) in \(dtime)ms")
    }
    
    deinit {
        se_destroy(&context)
    }
    
    func getLine(_ number: Int) -> (line: String, number:Int, scope: Bool, newWidth: Bool) {
        var lineInfo = SELineInfo()
        guard se_get_line(&context, UInt32(number), &lineInfo) == .NoError else {
            return ("[!] unable to get line \(number)", 0, false, false)
        }
        
        // while max length is calculated borrowed lines between blocks are not taken into account,
        // therefore we need to inform table if bigger log entry is detected
        var newWidth = false
        if (maxFilteredLength != 0) {
            if (lineInfo.length > maxFilteredLength) {
                print("[+] new filtered width \(lineInfo.length) cols")
                maxFilteredLength = Int(lineInfo.length)
                newWidth = true
            }
        }
        
        return (String(bytesNoCopy: UnsafeMutableRawPointer(mutating:lineInfo.line), length:Int(lineInfo.length), encoding:.ascii, freeWhenDone: false)!, Int(lineInfo.number), lineInfo.scope, newWidth)
    }
    
    func getRowForAbsLine(_ absLine: Int) -> Int {
        var row : UInt32 = 0
        guard se_get_row_for_abs_line(&context, UInt32(absLine), &row) == .NoError else {
            print("[!] unable to get row for line number \(absLine)")
            return -1
        }
        return Int(row)
    }
    
    func setIgnoreCase(_ ignoreCase: Bool) -> Bool {
        guard se_set_ignore_case(&context, ignoreCase) == .NoError else {
            print("[!] unable to set ignore case")
            return false
        }
        
        self.ignoreCase = ignoreCase
        return true;
    }

    func setScope(_ before: UInt32, _ after: UInt32) -> Bool {
        guard se_set_scope(&context, before, after) == .NoError else {
            print("[!] unable to set scope")
            return false
        }
        
        return true;
    }
    
    func setPattern(_ pattern: String) -> (Bool, String) {
        let cError = UnsafeMutablePointer<Int8>.allocate(capacity: Int(MAX_ERROR_LENGTH) + 1)
        guard se_set_pattern(&context, pattern, cError) == .NoError else {
            print("[!] unable to set pattern")
            let error = String(cString: cError)
            return (false, error)
        }
        
        return (true, "")
    }
    
    func filter() -> Bool {
        
        filteredLines = 0
        maxFilteredLength = 0
        
        let group = DispatchGroup()
        let queue = DispatchQueue.global()
        let stime = DispatchTime.now()
        for i in 0..<context.blocks {
            queue.async(group: group) {
                var blockInfo = SEBlockInfo()
                guard se_filter(&self.context, i, &blockInfo) == .NoError else {
                    print("[!] unable to filter block \(i)")
                    return
                }
                
                queue.sync() {
                    self.filteredLines += blockInfo.lines
                    if (self.maxFilteredLength < blockInfo.maxLength) {
                        self.maxFilteredLength = Int(blockInfo.maxLength)
                    }
                }
            }
        }
        
        group.wait()
        let etime = DispatchTime.now()
        let microTime = etime.uptimeNanoseconds - stime.uptimeNanoseconds
        let dtime = Double(microTime) / 1_000_000

        se_merge_scope(&context, &filteredLines)
        
        print("[+] filter ready (\(filteredLines) lines, \(maxFilteredLength) cols) in \(dtime)ms")
        return true;
    }
    
}
