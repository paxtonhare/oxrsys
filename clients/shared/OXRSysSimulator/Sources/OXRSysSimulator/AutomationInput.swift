// SPDX-License-Identifier: MPL-2.0
import Foundation
import OXRSysStreaming
import simd

// Full snapshots avoid a lost release leaving a button pressed. Poses are meters,
// OpenXR coordinates, with normalized x/y/z/w quaternions.
struct AutomationInput: Codable {
    struct Pose: Codable {
        var position: [Float]
        var rotation: [Float]

        func validate() throws {
            guard position.count == 3, rotation.count == 4,
                  position.allSatisfy({ $0.isFinite && abs($0) <= 1000 }),
                  rotation.allSatisfy(\.isFinite),
                  abs(rotation.reduce(0) { $0 + $1 * $1 } - 1) < 0.01 else {
                throw InputError.invalid
            }
        }
        var xyz: (Float, Float, Float) { (position[0], position[1], position[2]) }
        var xyzw: (Float, Float, Float, Float) { (rotation[0], rotation[1], rotation[2], rotation[3]) }
    }
    struct Hand: Codable {
        var gripPose: Pose
        var aimPose: Pose
        var trigger: Float = 0
        var grip: Float = 0
        var thumbstick: [Float] = [0, 0]
        func validate() throws {
            try gripPose.validate()
            try aimPose.validate()
            guard trigger.isFinite, (0...1).contains(trigger),
                  grip.isFinite, (0...1).contains(grip), thumbstick.count == 2,
                  thumbstick.allSatisfy({ $0.isFinite && (-1...1).contains($0) }) else {
                throw InputError.invalid
            }
        }
    }
    enum InputError: Error { case invalid }
    var head: Pose
    var left: Hand
    var right: Hand
    var buttons: UInt32 = 0

    static var neutral: Self {
        let head = Pose(position: [0, 1.6, 0], rotation: [0, 0, 0, 1])
        let left = Pose(position: [-0.2, 1.3, -0.4], rotation: [0, 0, 0, 1])
        let right = Pose(position: [0.2, 1.3, -0.4], rotation: [0, 0, 0, 1])
        return Self(head: head, left: Hand(gripPose: left, aimPose: left),
                    right: Hand(gripPose: right, aimPose: right))
    }
    func validate() throws {
        try head.validate()
        try left.validate()
        try right.validate()
        guard buttons & ~UInt32(0x7ff) == 0 else { throw InputError.invalid }
    }
    func apply(to packet: inout TrackingPacket) {
        packet.headPosition = head.xyz
        packet.headOrientation = head.xyzw
        packet.leftControllerPos = left.gripPose.xyz
        packet.leftControllerRot = left.gripPose.xyzw
        packet.leftControllerAimPos = left.aimPose.xyz
        packet.leftControllerAimRot = left.aimPose.xyzw
        packet.rightControllerPos = right.gripPose.xyz
        packet.rightControllerRot = right.gripPose.xyzw
        packet.rightControllerAimPos = right.aimPose.xyz
        packet.rightControllerAimRot = right.aimPose.xyzw
        packet.trackingFlags = TrackingFlagsValues.leftControllerActive |
            TrackingFlagsValues.rightControllerActive |
            TrackingFlagsValues.leftControllerAimActive | TrackingFlagsValues.rightControllerAimActive
        packet.leftTrigger = left.trigger
        packet.rightTrigger = right.trigger
        packet.leftGrip = left.grip
        packet.rightGrip = right.grip
        packet.leftThumbstick = (left.thumbstick[0], left.thumbstick[1])
        packet.rightThumbstick = (right.thumbstick[0], right.thumbstick[1])
        packet.buttonState = buttons
    }
}
