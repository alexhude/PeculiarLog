//
//  Document.swift
//  PeculiarLog
//
//  Created by Alexander Hude on 12/10/16.
//  Copyright © 2016 Alexander Hude. All rights reserved.
//

import Cocoa

class Document: NSDocument {

    var searchEngine : SearchEngine?
    
    override init() {
        super.init()
        // Add your subclass-specific initialization here.
    }

    override class var autosavesInPlace: Bool {
        return true
    }

    override func makeWindowControllers() {
        // Returns the Storyboard that contains your Document window.
        let storyboard = NSStoryboard(name: NSStoryboard.Name("Main"), bundle: nil)
        let windowController = storyboard.instantiateController(withIdentifier: NSStoryboard.SceneIdentifier("Document Window Controller")) as! NSWindowController
        self.addWindowController(windowController)
        
        // Set the view controller's represented object as your document.
        if let contentVC = windowController.contentViewController as? ViewController {
            contentVC.representedObject = searchEngine
        }
    }

    override func read(from url: URL, ofType typeName: String) throws {
        searchEngine = SearchEngine(url.path)
    }
}

