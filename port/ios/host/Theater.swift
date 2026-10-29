/* Theater mode (display.immersive, visionOS 26 and later): an immersive space
whose Compositor Services layer shows the game's picture on a screen placed in
the room (host_theater.m draws it). SDL's window scene stays as it is: the
space is a second scene, opened through a SwiftUI hosting delegate, and its
frames are drawn from the game's thread in gpu_present, as the Phase 3
prototype showed works with SDL's run-loop slices. */
import CompositorServices
import SwiftUI
import UIKit

@available(visionOS 26.0, *)
@Observable
final class TheaterHands {
    static let shared = TheaterHands()
    var visibility: Visibility = .automatic
}

/* hands a spatial event to the menus' pointer (host_sdl.c's host_menu_gaze_spatial_event), which takes only a
   pinch that looks at a menu while one is up: in play, every pinch stays the game's */
@available(visionOS 26.0, *)
enum TheaterPinch {
    static func forward(_ event: SpatialEventCollection.Event) {
        // the menus' pointer, the presenters and the game share state unlocked on the main thread
        dispatchPrecondition(condition: .onQueue(.main))
        switch event.kind {
        case .indirectPinch, .directPinch, .pointer: break
        default: return
        }
        let phase: Int32
        switch event.phase {
        case .active: phase = 0
        case .ended: phase = 1
        default: phase = 2
        }
        var ray = [Float](repeating: 0, count: 6)
        var hasRay: Int32 = 0
        if let selection = event.selectionRay {
            ray = [Float(selection.origin.x), Float(selection.origin.y), Float(selection.origin.z),
                   Float(selection.direction.x), Float(selection.direction.y), Float(selection.direction.z)]
            hasRay = 1
        }
        var hand = [Float](repeating: 0, count: 3)
        var hasHand: Int32 = 0
        if let pose = event.inputDevicePose {
            hand = [Float(pose.pose3D.position.x), Float(pose.pose3D.position.y), Float(pose.pose3D.position.z)]
            hasHand = 1
        }
        host_menu_gaze_spatial_event(event.id.hashValue, phase, hasRay, ray, hasHand, hand)
    }
}

@available(visionOS 26.0, *)
struct TheaterLayerConfiguration: CompositorLayerConfiguration {
    func makeConfiguration(capabilities: LayerRenderer.Capabilities,
                           configuration: inout LayerRenderer.Configuration) {
        // Foveation (display.foveation, HEAD mode only): the screen's picture
        // in SCREEN and mono theater mode is flat, so foveation would only
        // blur its edges
        configuration.isFoveationEnabled = false
        if host_theater_foveation() != 0 {
            let offered = capabilities.supportedLayouts(options: .foveationEnabled)
            let names = offered.map { layout -> String in
                switch layout {
                case .dedicated: return "dedicated"
                case .shared: return "shared"
                case .layered: return "layered"
                @unknown default: return "unknown"
                }
            }.joined(separator: ", ")
            let quality = host_theater_render_quality()
            // dedicated, one single-layer map per view, as this port renders
            // one pass per view; not Apple's sample's layered (one amplified
            // pass), whose map's layer is picked by
            // [[render_target_layer_index]], 0 in every pass here. Layered
            // only if dedicated isn't offered; neither leaves foveation off
            let layout: LayerRenderer.Layout? = offered.contains(.dedicated) ? .dedicated :
                offered.contains(.layered) ? .layered : nil
            if capabilities.supportsFoveation, let layout {
                configuration.isFoveationEnabled = true
                configuration.maxRenderQuality = LayerRenderer.RenderQuality(rawValue: quality)
                configuration.layout = layout
            }
            host_theater_set_foveation(configuration.isFoveationEnabled ? 1 : 0,
                                       capabilities.supportsFoveation ? 1 : 0,
                                       Int32(configuration.layout.rawValue), quality,
                                       capabilities.defaultRenderQuality.rawValue, names.isEmpty ? "none" : names)
        }
        // head-tracked stereo's depth range can't start nearer than this (host_stereo.m)
        host_theater_set_minimum_near(capabilities.supportedMinimumNearPlaneDistance)
    }
}

@available(visionOS 26.0, *)
final class TheaterSceneDelegate: UIResponder, UIHostingSceneDelegate {
    static let environment: any ImmersionStyle = host_theater_dark() != 0 ? .full : .mixed

    static var rootScene: some Scene {
        ImmersiveSpace(id: "theater") {
            CompositorLayer(configuration: TheaterLayerConfiguration()) { renderer in
                host_theater_attach(Unmanaged.passUnretained(renderer).toOpaque())
                // a look and pinch on the menus (host_menu_gaze.h): each pinch's ray and hand, in the space's frame
                renderer.onSpatialEvent = { events in
                    for event in events {
                        TheaterPinch.forward(event)
                    }
                }
            }
            .onWorldRecenter { phase in
                switch phase {
                case .began: host_stereo_world_recenter(1)
                case .ended: host_stereo_world_recenter(0)
                @unknown default: break
                }
            }
        }
        // display.theater_environment: the room around the screen, or the dark
        .immersionStyle(selection: .constant(Self.environment), in: .mixed, .full)
        .upperLimbVisibility(TheaterHands.shared.visibility)
    }

    /* a join link opened while the space is the active scene: the game's, as
       SDL's window scene passes them on (host_join_link.c) */
    func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
        for context in URLContexts {
            context.url.absoluteString.withCString { host_join_link_open($0) }
        }
    }
}

@_cdecl("host_theater_set_upper_limbs")
public func host_theater_set_upper_limbs(_ visible: Int32) {
    DispatchQueue.main.async {
        if #available(visionOS 26.0, *) {
            TheaterHands.shared.visibility = visible != 0 ? .automatic : .hidden
        }
    }
}

/* opens the space: at launch with display.immersive, and for Enter VR (host_theater.m) */
@_cdecl("host_theater_open")
public func host_theater_open() {
    guard #available(visionOS 26.0, *) else {
        host_theater_log("theater: display.immersive needs visionOS 26 or later; the game stays in its window")
        return
    }
    guard let request = UISceneSessionActivationRequest(hostingDelegateClass: TheaterSceneDelegate.self, id: "theater") else {
        host_theater_log("theater: the immersive space can't be requested")
        return
    }
    UIApplication.shared.activateSceneSession(for: request) { error in
        host_theater_log("theater: the immersive space didn't open: \(error.localizedDescription)")
        host_theater_open_failed()
    }
}

/* Enter VR: the ornament below the game's window, while the immersive space
   is closed (host_theater.m sets it and clears it), faint until looked at */
@_cdecl("host_theater_ornament")
public func host_theater_ornament(_ controller: UnsafeMutableRawPointer?, _ shown: Int32) {
    guard let controller else { return }
    MainActor.assumeIsolated {
        let window = Unmanaged<UIViewController>.fromOpaque(controller).takeUnretainedValue()
        window.ornaments = shown == 0 ? [] : [
            // below the window, where visionOS puts a window's toolbar
            UIHostingOrnament(sceneAnchor: .bottom, contentAlignment: .top) {
                Button {
                    host_theater_enter()
                } label: {
                    Label("Enter VR", systemImage: "visionpro")
                }
                .controlSize(.large)
                .padding(12)
                .glassBackgroundEffect()
                // faint until the player looks at it: the system's look-to-reveal
                // (a hover effect it renders itself; the app never sees where
                // the player looks), so it stays out of the way during play
                .hoverEffect { effect, isActive, _ in
                    effect.opacity(isActive ? 1.0 : 0.12)
                }
                .accessibilityHint("Plays the game around you in the headset")
                // a clear gap below the window's edge, as a toolbar ornament has
                .padding(.top, 24)
            }
        ]
    }
}

private func host_theater_log(_ message: String) {
    message.withCString { host_theater_log_c($0) }
}
