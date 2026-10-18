// SPDX-License-Identifier: MPL-2.0
import OXRSysSimulator
import Foundation

#if os(macOS)
// No window is created. Metal decoding and the normal tracking transport stay live.
guard CommandLine.arguments.count == 2 else {
    fputs("""
    usage: oxrsys-sim-control /private/path/control.sock
    Parent directory must be owned by you and mode 0700; socket must not exist.
    Send one newline-terminated JSON object per connection (maximum 16 KiB).
    Commands: {"command":"connect"}, "status", "reset", "disconnect".
    connect discovers the localhost runtime asynchronously; inspect status.message.
    Use runtime transport=wifi or auto; stop other headset clients before connecting.
    set: {"command":"set","input":<complete snapshot from status.input>}
    Poses: position [x,y,z] meters, rotation [x,y,z,w] unit quaternion, OpenXR axes.
    Each hand has gripPose, aimPose, trigger/grip [0,1], thumbstick [-1,1].
    Buttons: A=1 B=2 X=4 Y=8 menu=16 left-stick=32 right-stick=64.
    Snapshots are applied on the next tracking tick; ok is not runtime acknowledgement.
    Resend set more often than every 2s to hold controls; expiry releases all controls
    while retaining poses. status reports effective input and released.
    SIGINT/SIGTERM disconnect and remove the socket. After SIGKILL, remove the stale
    socket yourself only after confirming its owning process is gone.

    """, stderr)
    exit(2)
}
#else
fputs("oxrsys-sim-control requires macOS\n", stderr)
exit(2)
#endif
do {
    try SimulatorAutomation.run(socketPath: CommandLine.arguments[1])
    dispatchMain()
} catch {
    fputs("Simulator automation: \(error)\n", stderr)
    exit(1)
}
