//
//  LogViewController.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 31/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Cocoa

class LogViewController: NSViewController {

    @IBOutlet var tableView: NSTableView!
    
    private var currentPattern: String = ""
    private var currentMatchColor: NSColor = NSColor.red
    private var currentScopeColor: NSColor = NSColor.textColor
    
    private var rowInfoCache: (line: String, number: Int, scope: Bool, newWidth: Bool)?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
    }

    override var representedObject: Any? {
        didSet {
            // Update the view, if already loaded.
        #if swift(>=5.0)
            for child in children {
                child.representedObject = representedObject
            }
        #endif

            guard let engine = representedObject as? SearchEngine else { return }

            adjustColumnWidth(for:"LogLineColumn", length: String(engine.totalLines + 1).count) // lines start with 1
            adjustColumnWidth(for:"LogDataColumn", length: engine.maxLength)
            tableView.reloadData()
        }
    }

    func adjustColumnWidth(for column:String, length:Int) {

        // adjust column size to maximum length
        let cid = NSUserInterfaceItemIdentifier(rawValue: column)
        let cindex = tableView.column(withIdentifier: cid)
        guard let cell = tableView.makeView(withIdentifier: cid, owner: self) as? NSTableCellView else { return }
        guard let font = cell.textField?.font else { return }
        
        // it is safe to use dummy string here because of monospace font
        let sz = String(repeating: " ", count: length + 1).sizeOfString(font: font) // add extra character because selection makes line a bit wider so that last word disappears
        tableView.tableColumns[cindex].width = sz.width
    }
    
    func showLineNumbers(_ show: Bool) {
        let cid = NSUserInterfaceItemIdentifier(rawValue: "LogLineColumn")
        let cindex = tableView.column(withIdentifier: cid)
        tableView.tableColumns[cindex].isHidden = !show
    }
    
    func filterLog(with pattern: String, matchColor: NSColor, scopeColor: NSColor) {
        guard let engine = self.representedObject as? SearchEngine else { return }
        currentPattern = pattern
        currentMatchColor = matchColor
        currentScopeColor = scopeColor
        
        guard engine.setPattern(pattern) else { return }
        if (pattern.count != 0) {
            guard engine.filter() else { return }
        }
        
        if (engine.lineCount != 0) {
            adjustColumnWidth(for:"LogDataColumn", length: engine.maxLength)
        }
        
        tableView.reloadData()
    }
    
}

extension String {
    func sizeOfString(font: NSFont) -> CGSize {
    #if swift(>=5.0)
        let tmp = NSMutableAttributedString(string: self, attributes:[NSAttributedString.Key.font:font])
    #else
        let tmp = NSMutableAttributedString(string: self, attributes:[NSAttributedStringKey.font:font])
    #endif
        let limitSize = CGSize(width: CGFloat(MAXFLOAT), height: CGFloat(MAXFLOAT))
        let contentSize = tmp.boundingRect(with: limitSize, options: .usesLineFragmentOrigin, context: nil)
        return contentSize.size
    }
}

extension LogViewController: NSTableViewDataSource, NSTableViewDelegate {
    
    func numberOfRows(in tableView: NSTableView) -> Int {
        guard let engine = self.representedObject as? SearchEngine else { return 0 }
        return engine.lineCount
    }
    
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard let engine = self.representedObject as? SearchEngine else { return nil; }
        guard let cell = tableView.makeView(withIdentifier: tableColumn!.identifier, owner: self) as? NSTableCellView else { return nil }

        // Line Number Column
        if (tableColumn!.identifier.rawValue == "LogLineColumn") {
            
            // line column always goes first, therefore fetch line info here
            rowInfoCache = engine.getLine(row)
            guard let lineInfo = rowInfoCache else { return nil; }
            
            cell.textField?.stringValue = String(lineInfo.number + 1)
            cell.wantsLayer = true
            cell.layer?.backgroundColor = NSColor(named: NSColor.Name("LineNumberBackgroundColor"))?.cgColor
            return cell
        }
        
        // if line number column in hidden, fetch line info
        if (rowInfoCache == nil) {
            rowInfoCache = engine.getLine(row)
        }
        
        guard let lineInfo = rowInfoCache else { return nil; }

        // Line Data Column
        if (lineInfo.newWidth) {
            adjustColumnWidth(for:"LogDataColumn", length: engine.maxLength)
        }
        
        if (engine.isFiltered) {
            let attEntry = NSMutableAttributedString(string:lineInfo.line)
            
            guard let font = cell.textField?.font else { return nil }
            attEntry.addAttribute(.font, value: font, range: NSMakeRange(0, lineInfo.line.count))
            attEntry.addAttribute(.foregroundColor, value: cell.textField?.textColor as Any, range: NSMakeRange(0, lineInfo.line.count))
            
            if (!lineInfo.scope) {
                // color match line
                do {
                    var options: NSRegularExpression.Options = []
                    if (engine.ignoreCase) {
                        options = [.caseInsensitive]
                    }
                    let regex = try NSRegularExpression(pattern:currentPattern, options: options )
                    regex.enumerateMatches(in: lineInfo.line, options: [], range: NSMakeRange(0, lineInfo.line.count)) { result, flags, stop in
                        if let r = result?.range(at: 0) {
                            attEntry.addAttribute(.foregroundColor, value: currentMatchColor, range: r)
                        }
                    }
                } catch {
                    print("[!] unable to highlight pattern: \(error)")
                }
            } else {
                // color scope line
                attEntry.addAttribute(.foregroundColor, value: currentScopeColor, range: NSMakeRange(0, lineInfo.line.count))
            }
            cell.textField?.attributedStringValue = attEntry
        } else {
            cell.textField?.stringValue = lineInfo.line
        }
        
        rowInfoCache = nil // invalidate row cache for the next row
        return cell
    }
}
