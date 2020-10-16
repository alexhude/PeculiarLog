//
//  ViewController.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Cocoa

// MARK: - ViewController

class ViewController: NSViewController {

    @IBOutlet var patternField: NSTextField!
    @IBOutlet var navigateField: NSTextField!
    @IBOutlet var errorInfo: NSTextField!
    @IBOutlet var centerStatus: NSTextField!
    @IBOutlet var settingsButton: NSButton!
    @IBOutlet var beforeAfter: NSTextField!
    @IBOutlet var ignoreCase: NSTextField!
    @IBOutlet var mainPanel: NSStackView!
    
    var splitViewController: SplitViewController!
    var logViewController: LogViewController!
    var settingsViewController: SettingsViewController!
    
    var errorTimer: Timer?
    
    override func viewDidLoad() {
        super.viewDidLoad()

        errorInfo.textColor = NSColor.red
        
        // Do any additional setup after loading the view.
        errorInfo.isHidden = true
        ignoreCase.isHidden = true
        beforeAfter.isHidden = true
        
        mainPanel.arrangedSubviews[1].isHidden = true
    }
    
    override var representedObject: Any? {
        didSet {
            // Update the view, if already loaded.
        #if swift(>=5.0)
            for child in children {
                child.representedObject = representedObject
            }
        #endif

            // force loading Settings view
            settingsViewController.loadView()
            
            // pass engine to embedded controllers
            settingsViewController.representedObject = representedObject
            logViewController.representedObject = representedObject
            
            // set window delegate
            self.view.window?.delegate = self
            
            guard let engine = representedObject as? SearchEngine else { return }

            // setup engine from settings
            ignoreCase.isHidden = !settingsViewController.ignoreCase
            guard engine.setIgnoreCase(settingsViewController.ignoreCase) else { return }
            guard engine.setScope(settingsViewController.scopeBefore, settingsViewController.scopeAfter) else { return }
            logViewController.showLineNumbers(settingsViewController.showLines)
            updateStatus()
            
            // set center status
            centerStatus.stringValue = "\(engine.lineCount) of \(engine.totalLines) lines displayed"
            let selected = logViewController.tableView.numberOfSelectedRows
            if selected > 1 {
                centerStatus.stringValue += " (\(selected) selected)"
            }
            centerStatus.stringValue += ", \(engine.totalBytesString) total"

            let mainMenu = NSApplication.shared.mainMenu!
            let subMenu = mainMenu.item(withTitle: "File")?.submenu
            
            // handle menu
            if engine.isFiltered {
                subMenu?.item(withTitle: "Export Filtered")?.isEnabled = true
            } else {
                subMenu?.item(withTitle: "Export Filtered")?.isEnabled = false
            }
            if selected > 1 {
                subMenu?.item(withTitle: "Export Selected")?.isEnabled = true
            } else {
                subMenu?.item(withTitle: "Export Selected")?.isEnabled = false
            }
        }
    }

    override func prepare(for segue: NSStoryboardSegue, sender: Any?) {
        if let embeddedVC = segue.destinationController as? SplitViewController {
            splitViewController = embeddedVC
            settingsViewController = splitViewController.splitViewItems[0].viewController as? SettingsViewController
            settingsViewController.delegate = self
            logViewController = splitViewController.splitViewItems[1].viewController as? LogViewController
            logViewController.delegate = self
        }
    }
    
    @IBAction func toggleSettings(_ sender: Any) {
        splitViewController.splitViewItems[0].animator().isCollapsed = !splitViewController.splitViewItems[0].isCollapsed
    }
    
    @IBAction func gotoAbsoluteLineMenu(_ sender: Any) {
        self.navigateField.window?.makeFirstResponder(self.navigateField)
        
        NSAnimationContext.runAnimationGroup({context in
            context.duration = 0.25
            context.allowsImplicitAnimation = true
            self.mainPanel.arrangedSubviews[1].animator().isHidden = false
        }, completionHandler: {
        })
    }
    
    @IBAction func copyAbsoluteLineNumber(_ sender: Any) {
        guard let engine = representedObject as? SearchEngine else { return }
        guard logViewController.tableView.numberOfSelectedRows > 0 else { return }

        let indexSet = logViewController.tableView.selectedRowIndexes
        let rowIndex = indexSet.first!
        let lineInfo = engine.getLine(rowIndex)

        let pasteBoard = NSPasteboard.general
        pasteBoard.clearContents()
        pasteBoard.setString(String(lineInfo.number), forType:NSPasteboard.PasteboardType.string)
    }
    
    func updateStatus() {
        // set center status
        guard let engine = representedObject as? SearchEngine else { return }
        
        let mainMenu = NSApplication.shared.mainMenu!
        let subMenu = mainMenu.item(withTitle: "File")?.submenu
        
        let before = settingsViewController.scopeBefore
        let after = settingsViewController.scopeAfter
        var status = ""
        if (before != 0 || after != 0) {
            if (before != 0) {
                status += "B\(before) "
            }
            if (after != 0) {
                status += "A\(after) "
            }
            beforeAfter.stringValue = status
            beforeAfter.isHidden = false
        } else {
            beforeAfter.stringValue = ""
            beforeAfter.isHidden = true
        }
        
        // set center status
        centerStatus.stringValue = "\(engine.lineCount) of \(engine.totalLines) lines displayed"
        let selected = logViewController.tableView.numberOfSelectedRows
        if selected > 1 {
            centerStatus.stringValue += " (\(selected) selected)"
        }
        centerStatus.stringValue += ", \(engine.totalBytesString) total"

        // handle menu
        if engine.isFiltered {
            subMenu?.item(withTitle: "Export Filtered")?.isEnabled = true
        } else {
            subMenu?.item(withTitle: "Export Filtered")?.isEnabled = false
        }
        if selected > 1 {
            subMenu?.item(withTitle: "Export Selected")?.isEnabled = true
        } else {
            subMenu?.item(withTitle: "Export Selected")?.isEnabled = false
        }
    }
    
    func setError(_ string: String) {
        // set regexp error
        errorInfo.stringValue = string
        errorInfo.isHidden = false
        
        if (errorTimer != nil) {
            errorTimer?.invalidate()
        }
        
        errorTimer = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: false) { _ in
            self.errorInfo.stringValue = ""
            self.errorInfo.isHidden = true
        }
    }
    
    func filteredContent(_ fileHandle: FileHandle) {
        guard let engine = representedObject as? SearchEngine else { return }
        
        let filteredRows = logViewController.tableView.numberOfRows
        for rowIndex in 0..<filteredRows {
            let lineInfo = engine.getLine(rowIndex)
            let selectedLine = lineInfo.line + "\n"
            fileHandle.write(selectedLine.data(using: String.Encoding.utf8)!)
        }
    }
    
    func selectedContent(_ fileHandle: FileHandle) {
        guard let engine = representedObject as? SearchEngine else { return }
        
        let indexSet = logViewController.tableView.selectedRowIndexes
        for (_, rowIndex) in indexSet.enumerated() {
            let lineInfo = engine.getLine(rowIndex)
            let selectedLine = lineInfo.line + "\n"
            fileHandle.write(selectedLine.data(using: String.Encoding.utf8)!)
        }
    }
}

// MARK: - NSWindow delegate

extension ViewController: NSWindowDelegate {
    
    func windowDidBecomeMain(_ notification: Notification) {
        settingsViewController.bindShortcuts()
    }
}

// MARK: - TextField delegate

extension ViewController: NSTextFieldDelegate {
    
    func controlTextDidChangeCommon(_ obj: Notification) {
        guard
            let textField = obj.object as? NSTextField,
            textField == patternField
        else {
            return
        }
        
        logViewController.filterLog(with:patternField.stringValue, matchColor: settingsViewController.matchColor, scopeColor: settingsViewController.scopeColor)
        updateStatus()
    }
    
    func controlTextDidEndEditingCommon(_ obj: Notification) {
        guard
            let textMovement = obj.userInfo?["NSTextMovement"] as? Int,
            textMovement == NSReturnTextMovement,
            let textField = obj.object as? NSTextField,
            (textField == navigateField || textField == patternField)
        else {
            return
        }
        
        if (textField == navigateField) {
            if (logViewController.gotoAbsLine(textField.integerValue)) {
                textField.stringValue = ""

                NSAnimationContext.runAnimationGroup({context in
                    context.duration = 0.25
                    context.allowsImplicitAnimation = true
                    self.mainPanel.arrangedSubviews[1].animator().isHidden = true
                }, completionHandler: {
                    self.patternField.becomeFirstResponder()
                })
            } else {
                setError("Line not found")
            }
        } else {
            logViewController.filterLog(with:textField.stringValue, matchColor: settingsViewController.matchColor, scopeColor: settingsViewController.scopeColor, reportError: true)
            updateStatus()
        }
    }
    
#if swift(>=5.0)
    func controlTextDidChange(_ obj: Notification) {
        controlTextDidChangeCommon(obj)
    }
    
    func controlTextDidEndEditing(_ obj: Notification) {
        controlTextDidEndEditingCommon(obj)
    }
#else
    override func controlTextDidChange(_ obj: Notification) {
        controlTextDidChangeCommon(obj)
    }
    
    override func controlTextDidEndEditing(_ obj: Notification) {
        controlTextDidEndEditingCommon(obj)
    }
#endif
}

// MARK: - Settings delagate

extension ViewController: SettingsDelegate {
    func settingsChanged() {
        guard let engine = self.representedObject as? SearchEngine else { return }
        
        ignoreCase.isHidden = !settingsViewController.ignoreCase
        guard engine.setIgnoreCase(settingsViewController.ignoreCase) else { return }
        guard engine.setScope(settingsViewController.scopeBefore, settingsViewController.scopeAfter) else { return }

        logViewController.showLineNumbers(settingsViewController.showLines)
        logViewController.filterLog(with:patternField.stringValue, matchColor: settingsViewController.matchColor, scopeColor: settingsViewController.scopeColor)
        updateStatus()
    }
}

// MARK: - LogView delegate

extension ViewController: LogViewDelegate {
    func patternCompilationError(_ error: String) {
        setError(error)
    }
    
    func selectionChanged() {
        updateStatus()
    }
}


// MARK: - SplitView customization

class SplitViewController: NSSplitViewController {
    override func splitView(_ splitView: NSSplitView, effectiveRect proposedEffectiveRect: NSRect, forDrawnRect drawnRect: NSRect, ofDividerAt dividerIndex: Int) -> NSRect {
        return NSZeroRect
    }
}

class CustomSplitView: NSSplitView {
    override var dividerThickness:CGFloat {
        get { return 0.0 }
    }
}

// MARK: - NumberFormatter customization

class CustomNumberFormatter: NumberFormatter {
    override func isPartialStringValid(_ partialString: String, newEditingString newString: AutoreleasingUnsafeMutablePointer<NSString?>?, errorDescription error: AutoreleasingUnsafeMutablePointer<NSString?>?) -> Bool {
        let characterSet = NSMutableCharacterSet()
        characterSet.formUnion(with: NSCharacterSet.decimalDigits)
        if (partialString.rangeOfCharacter(from: characterSet.inverted) != nil) {
            return false
        }
        return true
    }
}
