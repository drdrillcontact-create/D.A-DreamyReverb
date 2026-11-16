import AppKit
import Darwin
import Foundation

private let artWidth: CGFloat = 1737
private let artHeight: CGFloat = 664
private let fm = FileManager.default

private struct Plugin {
    let name: String
    let folder: String
}
private let plugins = [
    Plugin(name: "D.A AURA.vst3", folder: "VST3"),
    Plugin(name: "D.A AURA.component", folder: "Components")
]

private func payloadDirectory() throws -> URL {
    guard let resources = Bundle.main.resourceURL else {
        throw NSError(domain: "DAAURA", code: 1, userInfo: [NSLocalizedDescriptionKey: "Installer resources are missing."])
    }
    return resources.appendingPathComponent("payload", isDirectory: true)
}

private func verifyPayload() throws {
    let payload = try payloadDirectory()
    for plugin in plugins {
        let bundle = payload.appendingPathComponent(plugin.name, isDirectory: true)
        let binary = bundle.appendingPathComponent("Contents/MacOS/D.A AURA")
        var isDirectory: ObjCBool = false
        guard fm.fileExists(atPath: bundle.path, isDirectory: &isDirectory), isDirectory.boolValue,
              fm.isExecutableFile(atPath: binary.path) else {
            throw NSError(domain: "DAAURA", code: 2, userInfo: [NSLocalizedDescriptionKey: "The embedded \(plugin.name) is missing or invalid."])
        }
    }
}

private func stripQuarantine(at root: URL) {
    _ = removexattr(root.path, "com.apple.quarantine", XATTR_NOFOLLOW)
    guard let enumerator = fm.enumerator(at: root, includingPropertiesForKeys: nil) else { return }
    for case let item as URL in enumerator {
        _ = removexattr(item.path, "com.apple.quarantine", XATTR_NOFOLLOW)
    }
}

// Prepare both bundles first. If either replacement fails, restore the previous
// versions and do not leave a half-installed pair of formats behind.
private func installPlugins(in root: URL) throws {
    try verifyPayload()
    let payload = try payloadDirectory()
    struct Change {
        let target: URL
        let stage: URL
        let backup: URL
        var hadBackup = false
        var installed = false
    }
    var changes: [Change] = []
    do {
        for plugin in plugins {
            let directory = root.appendingPathComponent(plugin.folder, isDirectory: true)
            try fm.createDirectory(at: directory, withIntermediateDirectories: true)
            let suffix = UUID().uuidString
            let stage = directory.appendingPathComponent(".DA-AURA-stage-\(suffix)")
            let backup = directory.appendingPathComponent(".DA-AURA-backup-\(suffix)")
            let target = directory.appendingPathComponent(plugin.name)
            changes.append(Change(target: target, stage: stage, backup: backup))
            try fm.copyItem(at: payload.appendingPathComponent(plugin.name), to: stage)
            stripQuarantine(at: stage)
        }
        for index in changes.indices {
            if fm.fileExists(atPath: changes[index].target.path) {
                try fm.moveItem(at: changes[index].target, to: changes[index].backup)
                changes[index].hadBackup = true
            }
            try fm.moveItem(at: changes[index].stage, to: changes[index].target)
            changes[index].installed = true
        }
    } catch {
        var restoreFailed = false
        for change in changes.reversed() {
            if change.installed { try? fm.removeItem(at: change.target) }
            if change.hadBackup {
                do { try fm.moveItem(at: change.backup, to: change.target) }
                catch { restoreFailed = true }
            }
            try? fm.removeItem(at: change.stage)
        }
        if restoreFailed {
            throw NSError(domain: "DAAURA", code: 3, userInfo: [NSLocalizedDescriptionKey: "Installation failed and an earlier version could not be restored. It is preserved in a .DA-AURA-backup folder in the plug-in directory."])
        }
        throw error
    }
    for change in changes where change.hadBackup { try? fm.removeItem(at: change.backup) }
}

private func fittedSize(_ available: CGSize) -> CGSize {
    let scale = min(1, max(0.001, min((available.width - 48) / artWidth, (available.height - 48) / artHeight)))
    return CGSize(width: round(artWidth * scale), height: round(artHeight * scale))
}

private final class InstallerWindow: NSWindow {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

private final class InstallerView: NSView {
    private let image: NSImage
    private var pressed = 0
    private var trackingAreaRef: NSTrackingArea?
    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    init(image: NSImage, frame: NSRect) {
        self.image = image
        super.init(frame: frame)
    }
    required init?(coder: NSCoder) { fatalError("Not supported") }

    override func updateTrackingAreas() {
        super.updateTrackingAreas()
        if let existing = trackingAreaRef { removeTrackingArea(existing) }
        let area = NSTrackingArea(
            rect: bounds,
            options: [.mouseMoved, .mouseEnteredAndExited, .activeAlways, .inVisibleRect],
            owner: self,
            userInfo: nil
        )
        addTrackingArea(area)
        trackingAreaRef = area
    }

    override func draw(_ dirtyRect: NSRect) {
        NSColor.clear.setFill()
        dirtyRect.fill(using: .copy)
        NSGraphicsContext.current?.imageInterpolation = .high
        image.draw(in: bounds, from: NSRect(origin: .zero, size: image.size),
                   operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
    }

    func hit(_ point: NSPoint) -> Int {
        guard bounds.width > 0 && bounds.height > 0 else { return 0 }
        let x = point.x * artWidth / bounds.width
        let y = point.y * artHeight / bounds.height
        if x >= 848 && x <= 1243 && y >= 536 && y <= 607 { return 1 } // Next ->
        if x >= 1280 && x <= 1645 && y >= 536 && y <= 607 { return 2 } // Cancel
        if x >= 1656 && x <= 1710 && y >= 14 && y <= 62 { return 3 }   // Close (X)
        if x >= 1592 && x <= 1650 && y >= 14 && y <= 62 { return 4 }   // Minimize (-)
        return 0
    }

    override func mouseMoved(with event: NSEvent) {
        if hit(convert(event.locationInWindow, from: nil)) != 0 {
            NSCursor.pointingHand.set()
        } else {
            NSCursor.arrow.set()
        }
    }

    override func mouseExited(with event: NSEvent) {
        NSCursor.arrow.set()
    }

    override func mouseDown(with event: NSEvent) {
        pressed = hit(convert(event.locationInWindow, from: nil))
        if pressed == 0 { window?.performDrag(with: event) }
    }

    override func mouseUp(with event: NSEvent) {
        let action = hit(convert(event.locationInWindow, from: nil))
        let wasPressed = pressed
        pressed = 0
        if action != 0 && action == wasPressed { activate(action) }
    }

    override func keyDown(with event: NSEvent) {
        if event.keyCode == 53 { activate(2) } // Escape
        else if event.keyCode == 36 || event.keyCode == 76 { activate(1) } // Return / Enter
        else { super.keyDown(with: event) }
    }

    private func activate(_ action: Int) {
        if action == 2 || action == 3 { NSApp.terminate(nil); return }
        if action == 4 { window?.miniaturize(nil); return }
        guard action == 1 else { return }
        do {
            let root = fm.homeDirectoryForCurrentUser.appendingPathComponent("Library/Audio/Plug-Ins", isDirectory: true)
            try installPlugins(in: root)
            let done = NSAlert()
            done.messageText = "D.A AURA Installer"
            done.informativeText = "D.A AURA (AU and VST3) has been installed in ~/Library/Audio/Plug-Ins. Restart or rescan your DAW."
            done.runModal()
            NSApp.terminate(nil)
        } catch {
            let alert = NSAlert(error: error)
            alert.messageText = "Installation failed"
            alert.runModal()
        }
    }
}

private final class AppDelegate: NSObject, NSApplicationDelegate {
    private var window: InstallerWindow?
    func applicationDidFinishLaunching(_ notification: Notification) {
        guard let url = Bundle.main.url(forResource: "welcome", withExtension: "png"),
              let data = try? Data(contentsOf: url),
              let rep = NSBitmapImageRep(data: data),
              let screen = NSScreen.main else {
            let alert = NSAlert(); alert.messageText = "Installer artwork could not be loaded."; alert.runModal()
            NSApp.terminate(nil); return
        }
        // Set explicit 1:1 point size matching the 1737x664 pixel design so Retina/non-Retina
        // screens interpret the artwork dimensions identically.
        let image = NSImage(size: NSSize(width: artWidth, height: artHeight))
        image.addRepresentation(rep)

        let available = screen.visibleFrame
        let size = fittedSize(available.size)
        let frame = NSRect(x: available.midX - size.width / 2, y: available.midY - size.height / 2,
                           width: size.width, height: size.height)
        let win = InstallerWindow(contentRect: frame, styleMask: [.borderless, .miniaturizable],
                                  backing: .buffered, defer: false)
        win.title = "D.A AURA Installer"
        win.isOpaque = false
        win.backgroundColor = .clear
        win.hasShadow = true
        win.isMovableByWindowBackground = false
        win.contentView = InstallerView(image: image, frame: NSRect(origin: .zero, size: size))
        win.center()
        win.makeKeyAndOrderFront(nil)
        win.makeFirstResponder(win.contentView)
        NSApp.activate(ignoringOtherApps: true)
        window = win
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

// Exercise the real borderless view, its octagonal corner transparency, and button regions.
private func testWindow() -> Bool {
    _ = NSApplication.shared
    guard let url = Bundle.main.url(forResource: "welcome", withExtension: "png"),
          let data = try? Data(contentsOf: url),
          let reference = NSBitmapImageRep(data: data) else { return false }
    guard reference.pixelsWide == Int(artWidth),
          reference.pixelsHigh == Int(artHeight),
          let c00 = reference.colorAt(x: 0, y: 0), c00.alphaComponent == 0,
          let c10 = reference.colorAt(x: Int(artWidth) - 1, y: 0), c10.alphaComponent == 0,
          let c01 = reference.colorAt(x: 0, y: Int(artHeight) - 1), c01.alphaComponent == 0,
          let c11 = reference.colorAt(x: Int(artWidth) - 1, y: Int(artHeight) - 1), c11.alphaComponent == 0 else {
        fputs("Installer artwork dimensions or octagonal corner transparency invalid\n", stderr)
        return false
    }
    let image = NSImage(size: NSSize(width: artWidth, height: artHeight))
    image.addRepresentation(reference)

    for width: CGFloat in [artWidth, floor(artWidth / 2)] {
        let height = round(width * artHeight / artWidth)
        let frame = NSRect(x: 0, y: 0, width: width, height: height)
        let window = InstallerWindow(contentRect: frame, styleMask: .borderless, backing: .buffered, defer: false)
        window.isOpaque = false
        window.backgroundColor = .clear
        let view = InstallerView(image: image, frame: frame)
        window.contentView = view
        for (x, y, action) in [(1045.0, 571.0, 1), (1462.0, 571.0, 2), (1682.0, 38.0, 3), (1620.0, 38.0, 4), (400.0, 300.0, 0)] {
            if view.hit(NSPoint(x: x * width / artWidth, y: y * height / artHeight)) != action { return false }
        }
        guard let bitmap = view.bitmapImageRepForCachingDisplay(in: view.bounds) else { return false }
        view.cacheDisplay(in: view.bounds, to: bitmap)
        if width == artWidth {
            for (x, y) in [(200, 40), (500, 250), (500, 420), (1045, 571), (1462, 571), (1650, 420)] {
                guard let a = reference.colorAt(x: x, y: y)?.usingColorSpace(.deviceRGB),
                      let b = bitmap.colorAt(x: x * bitmap.pixelsWide / reference.pixelsWide,
                                             y: y * bitmap.pixelsHigh / reference.pixelsHigh)?.usingColorSpace(.deviceRGB) else { return false }
                if abs(a.redComponent - b.redComponent) > 0.25 ||
                   abs(a.greenComponent - b.greenComponent) > 0.25 ||
                   abs(a.blueComponent - b.blueComponent) > 0.25 {
                    fputs("Rendered pixel mismatch at \(x),\(y): \(a) vs \(b)\n", stderr)
                    return false
                }
            }
        }
        window.close()
    }
    return true
}

// Non-interactive checks used on the actual packaged, relocated application.
let args = CommandLine.arguments
if args.count > 1 && args[1] == "--verify-payload" {
    do { try verifyPayload(); exit(0) } catch { fputs("\(error)\n", stderr); exit(1) }
}
if args.count == 3 && args[1] == "--test-install-root" {
    do { try installPlugins(in: URL(fileURLWithPath: args[2], isDirectory: true)); exit(0) }
    catch { fputs("\(error)\n", stderr); exit(1) }
}
if args.count > 1 && args[1] == "--test-window" { exit(testWindow() ? 0 : 1) }
if args.count > 1 && args[1] == "--test-layout" {
    for available in [CGSize(width: 1920, height: 1080), CGSize(width: 1440, height: 900), CGSize(width: 1024, height: 768), CGSize(width: 800, height: 500)] {
        let size = fittedSize(available)
        if size.width > available.width - 32 || size.height > available.height - 32 ||
            abs(size.width / size.height - artWidth / artHeight) > 0.015 { exit(1) }
    }
    let fullHD = fittedSize(CGSize(width: 1920, height: 1080))
    if Int(fullHD.width) != Int(artWidth) || Int(fullHD.height) != Int(artHeight) { exit(1) }
    exit(0)
}
let app = NSApplication.shared
private let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
