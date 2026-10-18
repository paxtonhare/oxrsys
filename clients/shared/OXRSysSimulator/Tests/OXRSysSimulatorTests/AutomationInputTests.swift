// SPDX-License-Identifier: MPL-2.0
import XCTest
import OXRSysStreaming
@testable import OXRSysSimulator

final class AutomationInputTests: XCTestCase {
    func testExpiredHoldReleasesControlsButKeepsPoses() throws {
        let manager = SimulatorInputManager()
        var input = AutomationInput.neutral
        input.buttons = 0x7ff
        input.left.trigger = 1
        input.head.position = [1, 2, 3]
        try manager.setAutomation(input)
        XCTAssertFalse(manager.automationSnapshot()!.released)
        let expired = manager.automationSnapshot(now: .greatestFiniteMagnitude)!
        XCTAssertTrue(expired.released)
        XCTAssertEqual(expired.input.buttons, 0)
        XCTAssertEqual(expired.input.left.trigger, 0)
        XCTAssertEqual(expired.input.head.position, [1, 2, 3])
    }
    func testIndependentPosesAndRelease() throws {
        let manager = SimulatorInputManager()
        var input = AutomationInput.neutral
        input.left.aimPose.rotation = [0, 1, 0, 0]
        input.right.gripPose.rotation = [1, 0, 0, 0]
        input.left.trigger = 0.7
        input.right.thumbstick = [0.5, -0.5]
        input.buttons = 0x7f
        try manager.setAutomation(input)
        let packet = manager.buildTrackingPacket()
        XCTAssertEqual(packet.headOrientation.3, 1)
        XCTAssertEqual(packet.leftControllerAimRot.1, 1)
        XCTAssertEqual(packet.rightControllerRot.0, 1)
        XCTAssertEqual(packet.leftControllerRot.3, 1)
        XCTAssertEqual(packet.leftTrigger, 0.7)
        XCTAssertEqual(packet.rightThumbstick.1, -0.5)
        XCTAssertEqual(packet.buttonState, 0x7f)
        XCTAssertNotEqual(packet.trackingFlags & TrackingFlagsValues.leftControllerAimActive, 0)
        try manager.setAutomation(.neutral)
        XCTAssertEqual(manager.buildTrackingPacket().buttonState, 0)
        XCTAssertEqual(manager.buildTrackingPacket().leftTrigger, 0)
    }
    func testInvalidSnapshotDoesNotReplaceInput() throws {
        let manager = SimulatorInputManager()
        try manager.setAutomation(.neutral)
        var bad = AutomationInput.neutral
        bad.head.rotation = [0, 0, 0, 0]
        XCTAssertThrowsError(try manager.setAutomation(bad))
        bad = .neutral
        bad.left.thumbstick = [2, 0]
        XCTAssertThrowsError(try manager.setAutomation(bad))
        bad = .neutral
        bad.right.gripPose.position = [.nan, 0, 0]
        XCTAssertThrowsError(try manager.setAutomation(bad))
        XCTAssertEqual(manager.buildTrackingPacket().headOrientation.3, 1)
    }
}
