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
   is closed (host_theater.m sets it and clears it) */
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
                .accessibilityHint("Plays the game around you in the headset")
            }
        ]
    }
}

/* Exit VR: a small button low in front of the player, out of the line of
   sight. The system places the panel's window centered in front of the
   player (the utility panel placement), so the window is tall and clear
   with the button at its bottom edge: the button sits about half a meter
   below eye level, at chest height, and nothing covers the game */
@available(visionOS 26.0, *)
struct VRControls: View {
    var body: some View {
        VStack {
            Spacer(minLength: 0)
            Button {
                host_theater_close()
            } label: {
                Label("Exit VR", systemImage: "rectangle.portrait.and.arrow.right")
            }
            .controlSize(.small)
            .padding(10)
            .glassBackgroundEffect(in: .capsule)
            .accessibilityHint("Returns the game to its window")
        }
        .frame(width: 260, height: 1100)
        // no window bar or close control: only the button shows
        .persistentSystemOverlays(.hidden)
    }
}

@available(visionOS 26.0, *)
final class VRControlsSceneDelegate: UIResponder, UIHostingSceneDelegate {
    static var rootScene: some Scene {
        WindowGroup(id: "vr-controls") {
            VRControls()
        }
        .windowStyle(.plain)
        .windowResizability(.contentSize)
        .persistentSystemOverlays(.hidden)
        .defaultWindowPlacement { _, _ in
            WindowPlacement(.utilityPanel)
        }
    }
}

/* host_theater.m: opens the Exit VR panel as the space opens (and closes it
   as the space closes) */
@_cdecl("host_theater_controls_open")
public func host_theater_controls_open() {
    guard #available(visionOS 26.0, *) else { return }
    guard let request = UISceneSessionActivationRequest(hostingDelegateClass: VRControlsSceneDelegate.self,
                                                        id: "vr-controls") else {
        host_theater_log("theater: the Exit VR panel can't be requested")
        return
    }
    UIApplication.shared.activateSceneSession(for: request) { error in
        host_theater_log("theater: the Exit VR panel didn't open: \(error.localizedDescription)")
    }
}

private func host_theater_log(_ message: String) {
    message.withCString { host_theater_log_c($0) }
}

/* The game's window keeps its window bar while VR is open, though UIKit has
   hidden its window, and UIKit has no way to hide the bar. A zero-size
   SwiftUI child of the window's root controller hides the window's system
   overlays while VR is open, and shows them again when it closes, so the
   player can still move the window */
@Observable
final class GameWindowOverlays {
    static let shared = GameWindowOverlays()
    var visibility: Visibility = .automatic
}

struct GameWindowOverlaysView: View {
    var body: some View {
        Color.clear
            .frame(width: 0, height: 0)
            .persistentSystemOverlays(GameWindowOverlays.shared.visibility)
            .allowsHitTesting(false)
    }
}

@_cdecl("host_theater_window_overlays")
public func host_theater_window_overlays(_ controller: UnsafeMutableRawPointer?, _ hidden: Int32) {
    guard let controller else { return }
    MainActor.assumeIsolated {
        let window = Unmanaged<UIViewController>.fromOpaque(controller).takeUnretainedValue()
        if !window.children.contains(where: { $0 is UIHostingController<GameWindowOverlaysView> }) {
            let child = UIHostingController(rootView: GameWindowOverlaysView())
            child.view.backgroundColor = .clear
            child.view.isUserInteractionEnabled = false
            child.view.frame = .zero
            window.addChild(child)
            window.view.addSubview(child.view)
            child.didMove(toParent: window)
        }
        GameWindowOverlays.shared.visibility = hidden != 0 ? .hidden : .automatic
    }
}
