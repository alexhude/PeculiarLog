//
//  SettingsViewController.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 7/11/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Cocoa

public protocol SettingsDelegate {
    func settingsChanged()
}

class SettingsViewController: NSViewController {

    @IBOutlet weak var ignoreCaseButton: NSButton!
    @IBOutlet weak var scopeBeforeField: NSTextField!
    @IBOutlet weak var scopeAfterField: NSTextField!
    @IBOutlet weak var matchColorPicker: NSColorWell!
    @IBOutlet weak var scopeColorPicker: NSColorWell!
    @IBOutlet weak var showLinesButton: NSButton!
    @IBOutlet weak var hintsLabel: NSTextField!
    
    var userDefaults: UserDefaults?
    var delegate: SettingsDelegate?

    static var eventMonitor : Any? = nil
    
    private(set) var ignoreCase = false
    private(set) var showLines = true

    var matchColor: NSColor {
        get {
            guard matchColorPicker != nil else { return NSColor.red }
            return matchColorPicker.color;
        }
    }
    var scopeColor: NSColor {
        get {
            guard scopeColorPicker != nil else { return NSColor.textColor }
            return scopeColorPicker.color;
        }
    }
    var scopeBefore: UInt32 {
        get {
            guard scopeBeforeField != nil else { return 0 }
            return UInt32(scopeBeforeField.integerValue);
        }
    }
    var scopeAfter: UInt32 {
        get {
            guard scopeAfterField != nil else { return 0 }
            return UInt32(scopeAfterField.integerValue);
        }
    }

    func bindShortcuts() {
        if let em = SettingsViewController.eventMonitor {
            NSEvent.removeMonitor(em)
        }
        
        SettingsViewController.eventMonitor = NSEvent.addLocalMonitorForEvents(matching: NSEvent.EventTypeMask.keyDown, handler: handleKeyDownEvent)
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()

        // load settings
        userDefaults = UserDefaults.standard
        if let caseless = userDefaults?.bool(forKey: "ignoreCase") {
            ignoreCase = caseless
            ignoreCaseButton.state = (caseless == true) ? .on : .off
        }
        if let showlines = userDefaults?.bool(forKey: "showLines") {
            showLines = showlines
            showLinesButton.state = (showlines == true) ? .on : .off
        }
        if let scopeBefore = userDefaults?.integer(forKey: "scopeBefore") {
            scopeBeforeField.integerValue = scopeBefore
        }
        if let scopeAfter = userDefaults?.integer(forKey: "scopeAfter") {
            scopeAfterField.integerValue = scopeAfter
        }
        if let matchColor = userDefaults?.object(forKey: "matchColor") as? Data {
            matchColorPicker.color = NSKeyedUnarchiver.unarchiveObject(with: matchColor) as! NSColor
        }
        if let scopeColor = userDefaults?.object(forKey: "scopeColor") as? Data {
            scopeColorPicker.color = NSKeyedUnarchiver.unarchiveObject(with: scopeColor) as! NSColor
        }

        // Do any additional setup after loading the view.
        scopeBeforeField.nextKeyView = scopeAfterField
        scopeAfterField.nextKeyView = scopeBeforeField

        bindShortcuts()
        
        // generate shortcut hints
        let shorcuts = [
            [ "⇧⌃↑", "B+   " ],
            [ "⇧⌘↑", "A-   " ],
            [ "⇧⌃↓", "B-   " ],
            [ "⇧⌘↓", "A+   " ],
            [ "⇧⌘c", "case " ],
            [ "⇧⌘l", "lines" ],
        ]
        
        let cols = 2
        let hintString = NSMutableAttributedString(string:"")

        let font = hintsLabel.font
        #if swift(>=5.0)
            let attKeys : [NSAttributedString.Key: Any] = [
                NSAttributedString.Key.foregroundColor: NSColor.red,
                NSAttributedString.Key.font: NSFont(name: (font?.familyName)!, size: 15) as Any
            ]
            let attArrow : [NSAttributedString.Key: Any] = [
                NSAttributedString.Key.font: NSFont(name: (font?.familyName)!, size: 15) as Any
            ]
        #else
            let attKeys : [NSAttributedStringKey: Any] = [
                NSAttributedStringKey.foregroundColor: NSColor.red,
                NSAttributedStringKey.font: NSFont(name: (font?.familyName)!, size: 15) as Any
            ]
            let attArrow : [NSAttributedStringKey: Any] = [
                NSAttributedStringKey.font: NSFont(name: (font?.familyName)!, size: 15) as Any
            ]
        #endif
        let arrow = NSAttributedString(string:" ⇒ ", attributes:attArrow)

        for (index, entry) in shorcuts.enumerated() {
            if (index % cols == 0) {
                hintString.append(NSAttributedString(string:"\n"))
            }
            let keys = NSAttributedString(string:entry[0], attributes:attKeys)
            hintString.append(keys)
            hintString.append(arrow)
            hintString.append(NSMutableAttributedString(string:entry[1]))
        }
        
        // squeeze lines
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.lineSpacing = 6
        paragraphStyle.maximumLineHeight = 7
        hintString.addAttribute(.paragraphStyle, value: paragraphStyle, range: NSMakeRange(0, hintString.length))

        hintsLabel.attributedStringValue = hintString
    }

    func handleKeyDownEvent(event: NSEvent) -> NSEvent?
    {
        func intToString(x : Int) -> String {
            return String(UnicodeScalar(x)!)
        }

        if (event.modifierFlags.contains([.control, .shift])) {
            switch event.charactersIgnoringModifiers! {
                case intToString(x: NSUpArrowFunctionKey):
                    if (scopeBeforeField.integerValue < MAX_SCOPE_BEFORE) {
                        scopeBeforeField.integerValue += 1
                        userDefaults?.set(scopeBeforeField.integerValue, forKey: "scopeBefore")
                        self.delegate?.settingsChanged()
                    }
                    return nil
                case intToString(x: NSDownArrowFunctionKey):
                    if (scopeBeforeField.integerValue > 0) {
                        scopeBeforeField.integerValue -= 1
                        userDefaults?.set(scopeBeforeField.integerValue, forKey: "scopeBefore")
                        self.delegate?.settingsChanged()
                    }
                    return nil
                default:
                    break;
            }
        } else if (event.modifierFlags.contains([.command, .shift])) {
            switch event.charactersIgnoringModifiers! {
            case "C":
                ignoreCase = !ignoreCase
                if (ignoreCaseButton.state == .on) {
                    ignoreCaseButton.state = .off
                } else {
                    ignoreCaseButton.state = .on
                }
                userDefaults?.set(ignoreCase, forKey: "ignoreCase")
                self.delegate?.settingsChanged()
                return nil
            case "L":
                showLines = !showLines
                if (showLinesButton.state == .on) {
                    showLinesButton.state = .off
                } else {
                    showLinesButton.state = .on
                }
                userDefaults?.set(showLines, forKey: "showLines")
                self.delegate?.settingsChanged()
                return nil
            case intToString(x: NSDownArrowFunctionKey):
                if (scopeAfterField.integerValue < MAX_SCOPE_AFTER) {
                    scopeAfterField.integerValue += 1
                    userDefaults?.set(scopeAfterField.integerValue, forKey: "scopeAfter")
                    self.delegate?.settingsChanged()
                }
                return nil
            case intToString(x: NSUpArrowFunctionKey):
                if (scopeAfterField.integerValue > 0) {
                    scopeAfterField.integerValue -= 1
                    userDefaults?.set(scopeAfterField.integerValue, forKey: "scopeAfter")
                    self.delegate?.settingsChanged()
                }
                return nil
            default:
                break;
            }
        }
        return event
    }

    override var representedObject: Any? {
        didSet {
            // Update the view, if already loaded.
        #if swift(>=5.0)
            for child in children {
                child.representedObject = representedObject
            }
        #endif
        }
    }
    
    @IBAction func changeCase(_ sender: Any) {
        let button = sender as? NSButton
        ignoreCase = button?.state == .on
        userDefaults?.set(ignoreCase, forKey: "ignoreCase")
        delegate?.settingsChanged()
    }

    @IBAction func changeShowLines(_ sender: Any) {
        let button = sender as? NSButton
        showLines = button?.state == .on
        userDefaults?.set(showLines, forKey: "showLines")
        delegate?.settingsChanged()
    }
    
    @IBAction func matchColorChanged(_ sender: Any) {
        let encodedData = NSKeyedArchiver.archivedData(withRootObject: matchColorPicker.color)
        userDefaults?.set(encodedData, forKey: "matchColor")
        delegate?.settingsChanged()
    }
    @IBAction func scopeColorChanged(_ sender: Any) {
        let encodedData = NSKeyedArchiver.archivedData(withRootObject: scopeColorPicker.color)
        userDefaults?.set(encodedData, forKey: "scopeColor")
        delegate?.settingsChanged()
    }
}

// MARK: - TextField delegate

extension SettingsViewController: NSTextFieldDelegate {
    
    #if swift(>=5.0)
    func controlTextDidChange(_ obj: Notification) {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            self.delegate?.settingsChanged()
        }
    }
    #else
    override func controlTextDidChange(_ obj: Notification) {
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) {
            self.delegate?.settingsChanged()
        }
    }
    #endif
}

// MARK: - NumberFormatter customization

class ScopeFormatter: NumberFormatter {
    override func isPartialStringValid(_ partialString: String, newEditingString newString: AutoreleasingUnsafeMutablePointer<NSString?>?, errorDescription error: AutoreleasingUnsafeMutablePointer<NSString?>?) -> Bool {
        let characterSet = NSMutableCharacterSet()
        characterSet.formUnion(with: NSCharacterSet.decimalDigits)
        if (partialString.rangeOfCharacter(from: characterSet.inverted) != nil) {
            return false
        }
        return true
    }
}
