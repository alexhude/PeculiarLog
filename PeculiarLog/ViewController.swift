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
    @IBOutlet var centerStatus: NSTextField!
    @IBOutlet var settingsButton: NSButton!
    @IBOutlet var beforeAfter: NSTextField!
    @IBOutlet var ignoreCase: NSTextField!
    
    var splitViewController: SplitViewController!
    var logViewController: LogViewController!
    var settingsViewController: SettingsViewController!
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // Do any additional setup after loading the view.
        ignoreCase.isHidden = true
        beforeAfter.isHidden = true
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
            centerStatus.stringValue = "\(engine.lineCount) of \(engine.totalLines) lines displayed, \(engine.totalBytesString) total"
        }
    }

    override func prepare(for segue: NSStoryboardSegue, sender: Any?) {
        if let embeddedVC = segue.destinationController as? SplitViewController {
            splitViewController = embeddedVC
            settingsViewController = splitViewController.splitViewItems[0].viewController as? SettingsViewController
            settingsViewController.delegate = self
            logViewController = splitViewController.splitViewItems[1].viewController as? LogViewController
        }
    }
    
    @IBAction func toggleSettings(_ sender: Any) {
        splitViewController.splitViewItems[0].animator().isCollapsed = !splitViewController.splitViewItems[0].isCollapsed
    }
    
    func updateStatus() {
        // set center status
        guard let engine = representedObject as? SearchEngine else { return }
        
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
        
        centerStatus.stringValue = "\(engine.lineCount) of \(engine.totalLines) lines displayed, \(engine.totalBytesString) total"
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

#if swift(>=5.0)
    func controlTextDidChange(_ obj: Notification) {
        logViewController.filterLog(with:patternField.stringValue, matchColor: settingsViewController.matchColor, scopeColor: settingsViewController.scopeColor)
        updateStatus()
    }
#else
    override func controlTextDidChange(_ obj: Notification) {
        logViewController.filterLog(with:patternField.stringValue, matchColor: settingsViewController.matchColor, scopeColor: settingsViewController.scopeColor)
        updateStatus()
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
