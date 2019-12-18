//
//  AppDelegate.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {

    func applicationDidFinishLaunching(_ aNotification: Notification) {
        // Insert code here to initialize your application
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationShouldOpenUntitledFile(_ sender: NSApplication) -> Bool {
        return false
    }
    
    func applicationShouldHandleReopen(_ sender: NSApplication, hasVisibleWindows flag: Bool) -> Bool {
        return false
    }

    func application(_ sender: NSApplication, openFile filename: String) -> Bool {
        print("[+] openning file \(filename)")
        let documentController = NSDocumentController.shared
        documentController.openDocument(withContentsOf: URL(fileURLWithPath: filename), display: true) {
            (document, documentWasAlreadyOpen, error) in
            if error != nil {
                print("[+] unable to open document!")
            } else {
                if documentWasAlreadyOpen {
                    print("[+] document is already opened")
                }
            }
        }
        return true;
    }
    
    func application(_ sender: NSApplication, openFiles filenames: [String]) {
        for file in filenames {
            _ = application(sender, openFile: file)
        }
    }
    
}

