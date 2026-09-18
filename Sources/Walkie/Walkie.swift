import Foundation
import WalkieCore

guard CommandLine.arguments.count == 4 else {
    print("usage: Walkie <listenPort> <remoteHost> <remotePort>")
    print("  same machine:  Walkie 5001 localhost 5002")
    print("                 Walkie 5002 localhost 5001")
    print("  two devices:   Walkie 5000 <other-ip> 5000")
    exit(1)
}

let listenPort = CommandLine.arguments[1]
let remoteHost = CommandLine.arguments[2]
let remotePort = CommandLine.arguments[3]

let session = WalkieSession(
    listenPort: listenPort, remoteHost: remoteHost, remotePort: remotePort)
do {
    try session.start()
} catch {
    print("failed to start: \(error)")
    exit(1)
}

print("listening on \(listenPort), sending to \(remoteHost):\(remotePort)")
print("Ctrl-C to quit")

RunLoop.current.run()
