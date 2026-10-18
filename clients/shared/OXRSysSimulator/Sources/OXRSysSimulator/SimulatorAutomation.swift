// SPDX-License-Identifier: MPL-2.0
#if os(macOS)
import Foundation
import Darwin

/// Opt-in, windowless software headset. One bounded JSON request per connection.
public enum SimulatorAutomation {
    private static var signals: [DispatchSourceSignal] = []
    public static func run(socketPath: String) throws {
        let parent = (socketPath as NSString).deletingLastPathComponent
        let attributes = try FileManager.default.attributesOfItem(atPath: parent)
        guard (attributes[.ownerAccountID] as? NSNumber)?.uint32Value == getuid(),
              ((attributes[.posixPermissions] as? NSNumber)?.intValue ?? 0) & 0o077 == 0,
              !FileManager.default.fileExists(atPath: socketPath),
              socketPath.utf8.count < 104 else {
            throw AutomationError.invalidPath
        }
        var address = sockaddr_un()
        address.sun_family = sa_family_t(AF_UNIX)
        address.sun_len = UInt8(MemoryLayout<sockaddr_un>.size)
        withUnsafeMutableBytes(of: &address.sun_path) { bytes in
            for (index, byte) in socketPath.utf8.enumerated() { bytes[index] = byte }
        }
        let fd = socket(AF_UNIX, SOCK_STREAM, 0)
        guard fd >= 0 else { throw AutomationError.socket(errno) }
        let result = withUnsafePointer(to: &address) {
            $0.withMemoryRebound(to: sockaddr.self, capacity: 1) {
                bind(fd, $0, socklen_t(MemoryLayout<sockaddr_un>.size))
            }
        }
        guard result == 0 else { close(fd); throw AutomationError.socket(errno) }
        guard chmod(socketPath, 0o600) == 0, listen(fd, 4) == 0 else {
            close(fd); unlink(socketPath); throw AutomationError.socket(errno)
        }
        let model = SimulatorModel()
        try model.inputManager.setAutomation(.neutral)
        for number in [SIGTERM, SIGINT] {
            signal(number, SIG_IGN)
            let source = DispatchSource.makeSignalSource(signal: number, queue: .main)
            source.setEventHandler {
                model.disconnect()
                unlink(socketPath)
                exit(0)
            }
            source.resume()
            signals.append(source)
        }
        let thread = Thread {
            defer { close(fd); unlink(socketPath) }
            while true {
                let client = accept(fd, nil, nil)
                if client < 0 {
                    if errno == EINTR { continue }
                    break
                }
                var timeout = timeval(tv_sec: 2, tv_usec: 0)
                var yes: Int32 = 1
                setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, socklen_t(MemoryLayout.size(ofValue: timeout)))
                setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, socklen_t(MemoryLayout.size(ofValue: timeout)))
                setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &yes, socklen_t(MemoryLayout.size(ofValue: yes)))
                var data = Data()
                var byte: UInt8 = 0
                let deadline = ProcessInfo.processInfo.systemUptime + 2
                while data.count < 16384, ProcessInfo.processInfo.systemUptime < deadline,
                      recv(client, &byte, 1, 0) == 1, byte != 10 { data.append(byte) }
                var response: [String: Any] = ["ok": false, "error": "Invalid or oversized newline JSON request"]
                if byte == 10, data.count < 16384 {
                    DispatchQueue.main.sync {
                        do {
                            let request = try JSONDecoder().decode(Request.self, from: data)
                            switch request.command {
                            case "connect":
                                model.connectLocalAutomation()
                            case "set":
                                guard let input = request.input else { throw AutomationError.missingInput }
                                try model.inputManager.setAutomation(input)
                            case "reset":
                                try model.inputManager.setAutomation(.neutral)
                            case "disconnect":
                                // Direct discovery owns a bounded worker; don't recycle its
                                // socket while that worker is still receiving.
                                guard model.state != .discovering else { throw AutomationError.discoveryInProgress }
                                model.disconnect()
                                try model.inputManager.setAutomation(.neutral)
                            case "status": break
                            default: throw AutomationError.unknownCommand
                            }
                            let snapshot = model.inputManager.automationSnapshot()!
                            response = ["ok": true, "state": String(describing: model.state), "message": model.statusText,
                                        "framesDecoded": model.framesDecoded,
                                        "released": snapshot.released, "holdTimeoutSeconds": 2,
                                        "input": try JSONSerialization.jsonObject(with: JSONEncoder().encode(snapshot.input))]
                        } catch { response = ["ok": false, "error": String(describing: error)] }
                    }
                }
                if var output = try? JSONSerialization.data(withJSONObject: response) {
                    output.append(10)
                    output.withUnsafeBytes { bytes in
                        var offset = 0
                        while offset < bytes.count {
                            let written = send(client, bytes.baseAddress!.advanced(by: offset), bytes.count - offset, 0)
                            if written <= 0 { break }
                            offset += written
                        }
                    }
                }
                close(client)
            }
        }
        thread.name = "oxrsys.simulator.automation"
        thread.start()
    }
    private struct Request: Decodable {
        var command: String
        var input: AutomationInput?
    }
    private enum AutomationError: Error {
        case invalidPath, socket(Int32), missingInput, unknownCommand, discoveryInProgress
    }
}
#endif
