// Extract evenly-spaced frames from a reference video so a broadcast camera and player motion can
// actually be looked at. macOS has no ffmpeg here, but AVFoundation is always present.
// Usage: swift Tools/ExtractVideoFrames.swift <video> <outdir> <count>
import Foundation
import AVFoundation
import AppKit

let args = CommandLine.arguments
guard args.count >= 4 else {
    print("usage: ExtractVideoFrames.swift <video> <outdir> <count>")
    exit(2)
}
let path = args[1]
let outDir = args[2]
let count = Int(args[3]) ?? 12

try? FileManager.default.createDirectory(atPath: outDir, withIntermediateDirectories: true)

let asset = AVURLAsset(url: URL(fileURLWithPath: path))
let duration = CMTimeGetSeconds(asset.duration)
print("C26_VID duration=\(String(format: "%.2f", duration))s")
if let track = asset.tracks(withMediaType: .video).first {
    let s = track.naturalSize
    print("C26_VID size=\(Int(s.width))x\(Int(s.height)) fps=\(String(format: "%.2f", track.nominalFrameRate))")
}

let gen = AVAssetImageGenerator(asset: asset)
gen.appliesPreferredTrackTransform = true
gen.requestedTimeToleranceBefore = .zero
gen.requestedTimeToleranceAfter = .zero

for i in 0..<count {
    let t = duration * Double(i) / Double(max(1, count - 1))
    let clamped = min(max(0.0, t), max(0.0, duration - 0.05))
    let time = CMTime(seconds: clamped, preferredTimescale: 600)
    do {
        let cg = try gen.copyCGImage(at: time, actualTime: nil)
        let rep = NSBitmapImageRep(cgImage: cg)
        guard let data = rep.representation(using: .jpeg, properties: [.compressionFactor: 0.9]) else {
            print("C26_VID encode failed at \(i)")
            continue
        }
        let name = String(format: "ref_%02d.jpg", i)
        try data.write(to: URL(fileURLWithPath: outDir).appendingPathComponent(name))
        print("C26_VID wrote \(name) t=\(String(format: "%.2f", clamped))")
    } catch {
        print("C26_VID fail i=\(i) t=\(String(format: "%.2f", clamped)) err=\(error.localizedDescription)")
    }
}
print("C26_VID_END")
