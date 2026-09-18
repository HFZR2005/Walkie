import Darwin
import SwiftUI
import WalkieCore

struct ContentView: View {
    @State private var listenPort = "5000"
    @State private var remoteHost = ""
    @State private var remotePort = "5000"
    @State private var session: WalkieSession?
    @State private var errorText = ""
    @State private var running = false

    var body: some View {
        NavigationStack {
            Form {
                Section("This phone") {
                    TextField("Listen port", text: $listenPort)
                        .keyboardType(.numberPad)
                    if let ip = localIPv4() {
                        Text("LAN IP \(ip)")
                            .foregroundStyle(.secondary)
                    }
                }
                Section("Other phone") {
                    TextField("IP address", text: $remoteHost)
                        .textInputAutocapitalization(.never)
                        .keyboardType(.decimalPad)
                    TextField("Port", text: $remotePort)
                        .keyboardType(.numberPad)
                }
                Section {
                    Button(running ? "Stop" : "Start") {
                        if running {
                            session?.stop()
                            session = nil
                            running = false
                            errorText = ""
                        } else {
                            start()
                        }
                    }
                    .disabled(!running && (listenPort.isEmpty || remoteHost.isEmpty || remotePort.isEmpty))
                }
                if !errorText.isEmpty {
                    Section {
                        Text(errorText)
                            .foregroundStyle(.red)
                    }
                }
                Section {
                    Text(
                        "Same Wi‑Fi, no VPN. Type this phone’s LAN IP on the other device. Both can use port 5000."
                    )
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Walkie")
        }
    }

    private func start() {
        let next = WalkieSession(
            listenPort: listenPort, remoteHost: remoteHost, remotePort: remotePort)
        do {
            try next.start()
            session = next
            running = true
            errorText = ""
        } catch {
            errorText = String(describing: error)
            next.stop()
        }
    }
}

private func localIPv4() -> String? {
    var address: String?
    var ifaddr: UnsafeMutablePointer<ifaddrs>?
    guard getifaddrs(&ifaddr) == 0, let first = ifaddr else { return nil }
    defer { freeifaddrs(ifaddr) }

    var ptr: UnsafeMutablePointer<ifaddrs>? = first
    while let iface = ptr {
        let flags = Int32(iface.pointee.ifa_flags)
        let family = iface.pointee.ifa_addr.pointee.sa_family
        if family == UInt8(AF_INET),
            flags & (IFF_UP | IFF_RUNNING | IFF_LOOPBACK) == (IFF_UP | IFF_RUNNING)
        {
            var hostname = [CChar](repeating: 0, count: Int(NI_MAXHOST))
            if getnameinfo(
                iface.pointee.ifa_addr,
                socklen_t(iface.pointee.ifa_addr.pointee.sa_len),
                &hostname,
                socklen_t(hostname.count),
                nil,
                0,
                NI_NUMERICHOST
            ) == 0 {
                let name = String(cString: hostname)
                if name.hasPrefix("192.168.") || name.hasPrefix("10.")
                    || name.hasPrefix("172.")
                {
                    address = name
                    break
                }
            }
        }
        ptr = iface.pointee.ifa_next
    }
    return address
}
