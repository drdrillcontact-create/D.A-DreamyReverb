import AppKit
import Foundation

private let artWidth: CGFloat = 1466
private let artHeight: CGFloat = 560
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
    let scale = min(1, max(0.001, min((available.width - 32) / artWidth, (available.height - 32) / artHeight)))
    return CGSize(width: floor(artWidth * scale), height: floor(artHeight * scale))
}

private final class InstallerWindow: NSWindow {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

private final class InstallerView: NSView {
    private let image: NSImage
    private var pressed = 0
    override var isFlipped: Bool { true }
    override var acceptsFirstResponder: Bool { true }

    init(image: NSImage, frame: NSRect) {
        self.image = image
        super.init(frame: frame)
    }
    required init?(coder: NSCoder) { fatalError("Not supported") }

    override func draw(_ dirtyRect: NSRect) {
        super.draw(dirtyRect)
        NSGraphicsContext.current?.imageInterpolation = .high
        image.draw(in: bounds, from: NSRect(origin: .zero, size: image.size),
                   operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
    }
    private func hit(_ point: NSPoint) -> Int {
        guard bounds.width > 0 && bounds.height > 0 else { return 0 }
        let x = point.x * artWidth / bounds.width, y = point.y * artHeight / bounds.height
        if x >= 716 && x <= 1049 && y >= 453 && y <= 514 { return 1 }
        if x >= 1081 && x <= 1389 && y >= 453 && y <= 514 { return 2 }
        if x >= 1396 && x <= 1440 && y >= 5 && y <= 60 { return 3 }
        if x >= 1344 && x < 1396 && y >= 5 && y <= 60 { return 4 }
        return 0
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
        else if event.keyCode == 36 || event.keyCode == 76 { activate(1) } // Return
        else { super.keyDown(with: event) }
    }
    private func activate(_ action: Int) {
        if action == 2 || action == 3 { NSApp.terminate(nil); return }
        if action == 4 { window?.miniaturize(nil); return }
        guard action == 1 else { return }
        let confirm = NSAlert()
        confirm.messageText = "Install D.A AURA?"
        confirm.informativeText = "The included AU and VST3 plug-ins will be installed for the current user."
        confirm.addButton(withTitle: "Install")
        confirm.addButton(withTitle: "Cancel")
        guard confirm.runModal() == .alertFirstButtonReturn else { return }
        do {
            let root = fm.homeDirectoryForCurrentUser.appendingPathComponent("Library/Audio/Plug-Ins", isDirectory: true)
            try installPlugins(in: root)
            let done = NSAlert()
            done.messageText = "D.A AURA installed"
            done.informativeText = "AU and VST3 were installed in ~/Library/Audio/Plug-Ins. Restart or rescan your DAW."
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
              let image = NSImage(contentsOf: url), let screen = NSScreen.main else {
            let alert = NSAlert(); alert.messageText = "Installer artwork could not be loaded."; alert.runModal()
            NSApp.terminate(nil); return
        }
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
        win.contentView = InstallerView(image: image, frame: NSRect(origin: .zero, size: size))
        win.center()
        win.makeKeyAndOrderFront(nil)
        win.makeFirstResponder(win.contentView)
        NSApp.activate(ignoringOtherApps: true)
        window = win
    }
    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
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
if args.count > 1 && args[1] == "--test-layout" {
    for available in [CGSize(width: 1920, height: 1080), CGSize(width: 1024, height: 768), CGSize(width: 800, height: 500)] {
        let size = fittedSize(available)
        if size.width > available.width - 32 || size.height > available.height - 32 ||
            abs(size.width / size.height - artWidth / artHeight) > 0.01 { exit(1) }
    }
    exit(0)
}
let app = NSApplication.shared
private let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
