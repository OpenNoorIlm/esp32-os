"""
╔══════════════════════════════════════════════════════════════════╗
║        ESP32-CAM  ·  Cinematic Object Detector                  ║
║   YOLOv8 · GPU inference · CPU rendering · Live MJPEG stream    ║
╚══════════════════════════════════════════════════════════════════╝

Install deps (run once):
    pip install ultralytics opencv-python numpy torch torchvision
    # For CUDA GPU support use:
    pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121

Usage:
    python esp32_detector.py --ip 192.168.1.42 --port 8082
    python esp32_detector.py --ip 192.168.1.42 --port 8082 --model yolov8s  # larger/more accurate
"""

import cv2
import numpy as np
import threading
import time
import queue
import argparse
import sys
import urllib.request
from collections import deque, defaultdict
import math

# ─── GPU / CPU device selection ──────────────────────────────────────────────
try:
    import torch
    TORCH_AVAILABLE = True
    if torch.cuda.is_available():
        DEVICE = "cuda"
        GPU_NAME = torch.cuda.get_device_name(0)
        VRAM_GB  = torch.cuda.get_device_properties(0).total_memory / 1e9
    else:
        DEVICE = "cpu"
        GPU_NAME = "None"
        VRAM_GB  = 0
except ImportError:
    TORCH_AVAILABLE = False
    DEVICE = "cpu"
    GPU_NAME = "None"
    VRAM_GB  = 0

# ─── YOLO import ─────────────────────────────────────────────────────────────
try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    print("ultralytics not found. Install: pip install ultralytics")
    sys.exit(1)


# ══════════════════════════════════════════════════════════════════════════════
#  COLOUR PALETTE  — one vivid colour per class, HSV-spread for max contrast
# ══════════════════════════════════════════════════════════════════════════════
def make_palette(n=80):
    palette = []
    for i in range(n):
        hue = int(i * 180 / n)
        s   = np.array([[[hue, 220, 255]]], dtype=np.uint8)
        bgr = cv2.cvtColor(s, cv2.COLOR_HSV2BGR)[0][0]
        palette.append((int(bgr[0]), int(bgr[1]), int(bgr[2])))
    return palette

PALETTE = make_palette(80)


# ══════════════════════════════════════════════════════════════════════════════
#  SMOOTH TRACKER  — exponential smoothing per object id so boxes glide
# ══════════════════════════════════════════════════════════════════════════════
class SmoothTracker:
    def __init__(self, alpha=0.4):
        self.alpha  = alpha          # 0=very smooth/laggy, 1=instant
        self.boxes  = {}             # id → smoothed (x1,y1,x2,y2)
        self.conf   = {}
        self.labels = {}
        self.age    = {}
        self.trails = defaultdict(lambda: deque(maxlen=30))

    def update(self, detections):
        """detections: list of (track_id, x1,y1,x2,y2, conf, cls_name)"""
        seen = set()
        for tid, x1, y1, x2, y2, conf, name in detections:
            seen.add(tid)
            new_box = np.array([x1, y1, x2, y2], dtype=float)
            if tid in self.boxes:
                self.boxes[tid] = self.alpha * new_box + (1 - self.alpha) * self.boxes[tid]
            else:
                self.boxes[tid] = new_box.copy()
            self.conf[tid]   = conf
            self.labels[tid] = name
            self.age[tid]    = time.time()
            cx = int((self.boxes[tid][0] + self.boxes[tid][2]) / 2)
            cy = int((self.boxes[tid][1] + self.boxes[tid][3]) / 2)
            self.trails[tid].append((cx, cy))

        # expire stale tracks after 1.5 s
        stale = [k for k, t in self.age.items() if time.time() - t > 1.5]
        for k in stale:
            del self.boxes[k], self.conf[k], self.labels[k], self.age[k]
            if k in self.trails: del self.trails[k]

    def get(self):
        out = []
        for tid in self.boxes:
            b = self.boxes[tid].astype(int)
            out.append((tid, b[0], b[1], b[2], b[3],
                        self.conf[tid], self.labels[tid],
                        list(self.trails[tid])))
        return out


# ══════════════════════════════════════════════════════════════════════════════
#  CINEMATIC RENDERER  (runs on CPU thread)
# ══════════════════════════════════════════════════════════════════════════════
class CinematicRenderer:
    SCAN_LINES   = True
    CORNER_LEN   = 18      # px of each corner bracket arm
    CORNER_W     = 2
    BOX_W        = 1
    TRAIL_FADE   = True
    PULSE_SPEED  = 3.0     # Hz of corner pulse
    FONT         = cv2.FONT_HERSHEY_SIMPLEX
    FONT_SCALE   = 0.48
    FONT_W       = 1

    def __init__(self):
        self.t0      = time.time()
        self.scan_y  = 0
        self.active  = False          # detection overlay on/off
        self.flash   = 0.0            # activation flash effect 0→1

    def _elapsed(self):
        return time.time() - self.t0

    def draw(self, frame, tracks, fps_det, fps_rend, device_str):
        h, w = frame.shape[:2]
        out  = frame.copy()

        # ── scan-line / vignette overlay ──────────────────────────────────
        if self.active:
            # soft vignette
            vig = np.zeros((h, w), dtype=np.float32)
            cx, cy = w // 2, h // 2
            Y, X = np.ogrid[:h, :w]
            dist = np.sqrt(((X - cx) / cx) ** 2 + ((Y - cy) / cy) ** 2)
            vig  = np.clip(1.0 - dist * 0.45, 0, 1).astype(np.float32)
            out  = (out.astype(np.float32) * vig[:, :, None]).astype(np.uint8)

            # scan line
            self.scan_y = (self.scan_y + 2) % h
            cv2.line(out, (0, self.scan_y), (w, self.scan_y),
                     (0, 255, 160), 1, cv2.LINE_AA)

            # activation flash
            if self.flash > 0:
                flash_layer = np.full_like(out, (0, 255, 160))
                out = cv2.addWeighted(out, 1.0, flash_layer,
                                      self.flash * 0.35, 0)
                self.flash = max(0.0, self.flash - 0.08)

        # ── draw each track ───────────────────────────────────────────────
        if self.active:
            t   = self._elapsed()
            pulse = 0.5 + 0.5 * math.sin(t * self.PULSE_SPEED * 2 * math.pi)

            for tid, x1, y1, x2, y2, conf, name, trail in tracks:
                col_bgr = PALETTE[hash(name) % len(PALETTE)]

                # ghost trail
                if self.TRAIL_FADE and len(trail) > 1:
                    for i in range(1, len(trail)):
                        alpha = i / len(trail)
                        cv2.line(out, trail[i-1], trail[i],
                                 tuple(int(c * alpha * 0.6) for c in col_bgr),
                                 1, cv2.LINE_AA)

                # thin bounding rect
                cv2.rectangle(out, (x1, y1), (x2, y2),
                              tuple(int(c * 0.4) for c in col_bgr),
                              self.BOX_W, cv2.LINE_AA)

                # animated corner brackets
                brt = tuple(min(255, int(c * (0.7 + 0.3 * pulse)))
                            for c in col_bgr)
                cl  = self.CORNER_LEN
                cw  = self.CORNER_W
                # TL
                cv2.line(out,(x1,y1),(x1+cl,y1),brt,cw,cv2.LINE_AA)
                cv2.line(out,(x1,y1),(x1,y1+cl),brt,cw,cv2.LINE_AA)
                # TR
                cv2.line(out,(x2,y1),(x2-cl,y1),brt,cw,cv2.LINE_AA)
                cv2.line(out,(x2,y1),(x2,y1+cl),brt,cw,cv2.LINE_AA)
                # BL
                cv2.line(out,(x1,y2),(x1+cl,y2),brt,cw,cv2.LINE_AA)
                cv2.line(out,(x1,y2),(x1,y2-cl),brt,cw,cv2.LINE_AA)
                # BR
                cv2.line(out,(x2,y2),(x2-cl,y2),brt,cw,cv2.LINE_AA)
                cv2.line(out,(x2,y2),(x2,y2-cl),brt,cw,cv2.LINE_AA)

                # label pill background
                label  = f"{name}  {conf:.0%}"
                (tw,th),_ = cv2.getTextSize(label,self.FONT,
                                            self.FONT_SCALE, self.FONT_W)
                lx, ly = x1, max(y1 - 6, th + 4)
                cv2.rectangle(out, (lx, ly - th - 4),
                              (lx + tw + 8, ly + 2),
                              col_bgr, -1)
                cv2.putText(out, label, (lx + 4, ly - 1),
                            self.FONT, self.FONT_SCALE,
                            (0, 0, 0), self.FONT_W, cv2.LINE_AA)

                # centre crosshair dot
                cx_ = (x1 + x2) // 2
                cy_ = (y1 + y2) // 2
                cv2.circle(out, (cx_, cy_), 3, brt, -1, cv2.LINE_AA)
                cv2.circle(out, (cx_, cy_), 6, brt, 1,  cv2.LINE_AA)

        # ── HUD ───────────────────────────────────────────────────────────
        self._draw_hud(out, fps_det, fps_rend, device_str,
                       len(tracks), w, h)
        return out

    def _draw_hud(self, img, fps_det, fps_rend, device_str, n_obj, w, h):
        lines = [
            f"DET  {fps_det:4.1f} fps",
            f"REND {fps_rend:4.1f} fps",
            f"DEV  {device_str}",
            f"OBJ  {n_obj}",
        ]
        pad, lh = 8, 18
        bh = len(lines) * lh + pad * 2
        cv2.rectangle(img, (0, 0), (170, bh), (0, 0, 0), -1)
        cv2.rectangle(img, (0, 0), (170, bh), (0, 200, 80), 1)
        for i, l in enumerate(lines):
            cv2.putText(img, l, (pad, pad + (i+1)*lh - 3),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4,
                        (0, 255, 120), 1, cv2.LINE_AA)

        # activation status
        status = "[ SCAN ON ]" if self.active else "[ PRESS  D ]"
        col    = (0, 255, 80) if self.active else (80, 80, 80)
        cv2.putText(img, status, (w - 130, h - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.45, col, 1, cv2.LINE_AA)


# ══════════════════════════════════════════════════════════════════════════════
#  STREAM READER  (dedicated thread — never blocks inference)
# ══════════════════════════════════════════════════════════════════════════════
class StreamReader(threading.Thread):
    def __init__(self, url):
        super().__init__(daemon=True)
        self.url   = url
        self.frame = None
        self.lock  = threading.Lock()
        self.ok    = False

    def run(self):
        cap = cv2.VideoCapture(self.url)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        self.ok = cap.isOpened()
        while True:
            ret, frame = cap.read()
            if not ret:
                self.ok = False
                time.sleep(0.1)
                cap.open(self.url)
                continue
            self.ok = True
            with self.lock:
                self.frame = frame

    def latest(self):
        with self.lock:
            return self.frame.copy() if self.frame is not None else None


# ══════════════════════════════════════════════════════════════════════════════
#  INFERENCE WORKER  (GPU thread)
# ══════════════════════════════════════════════════════════════════════════════
class InferenceWorker(threading.Thread):
    def __init__(self, model_name, device, result_queue):
        super().__init__(daemon=True)
        self.model_name   = model_name
        self.device       = device
        self.result_queue = result_queue
        self.input_queue  = queue.Queue(maxsize=1)
        self.fps          = 0.0
        self._stop        = False

    def submit(self, frame):
        try:
            self.input_queue.put_nowait(frame)
        except queue.Full:
            pass  # drop frame — we always want the latest

    def stop(self): self._stop = True

    def run(self):
        print(f"[YOLO] Loading {self.model_name} on {self.device} …")
        # Load model — accept .onnx, .pt, or bare name
        import os
        mn = self.model_name
        if os.path.isfile(mn):
            # user supplied a direct file path (.onnx or .pt)
            model_path = mn
        elif mn.endswith(".pt") or mn.endswith(".onnx"):
            model_path = mn
        else:
            model_path = f"{mn}.pt"   # bare name like yolov8n
        print(f"[YOLO] Model file: {model_path}")
        model = YOLO(model_path)

        # ONNX provider selection: AMD Vega via DirectML, CPU fallback
        is_onnx = model_path.endswith(".onnx")
        if is_onnx:
            import onnxruntime as ort
            providers = ort.get_available_providers()
            print(f"[ONNX] Available providers: {providers}")

            if "DmlExecutionProvider" in providers:
                use_providers = ["DmlExecutionProvider", "CPUExecutionProvider"]
                print("[ONNX] ✓ Using AMD GPU via DirectML + CPU fallback")
            elif "CUDAExecutionProvider" in providers:
                use_providers = ["CUDAExecutionProvider", "CPUExecutionProvider"]
                print("[ONNX] Using NVIDIA CUDA + CPU fallback")
            else:
                use_providers = ["CPUExecutionProvider"]
                print("[ONNX] Using CPU only")

            # Apply providers directly to the onnxruntime session inside YOLO
            import onnxruntime as ort_rt
            sess_opts = ort_rt.SessionOptions()
            sess_opts.graph_optimization_level = ort_rt.GraphOptimizationLevel.ORT_ENABLE_ALL
            sess_opts.execution_mode = ort_rt.ExecutionMode.ORT_PARALLEL
            # Patch ultralytics to use our session options
            model.predictor = None  # reset so it rebuilds with new session
            model.overrides["providers"] = use_providers
        else:
            model.to(self.device)
        # warm-up
        dummy = np.zeros((480, 640, 3), dtype=np.uint8)
        model(dummy, verbose=False)
        print(f"[YOLO] Ready on {self.device.upper()}")

        t_prev = time.time()
        while not self._stop:
            try:
                frame = self.input_queue.get(timeout=0.1)
            except queue.Empty:
                continue

            track_kwargs = dict(persist=True, verbose=False,
                                   conf=0.35, iou=0.45,
                                   tracker="bytetrack.yaml")
            if is_onnx:
                # ONNX runs via onnxruntime — no device= arg needed
                results = model.track(frame, **track_kwargs)
            else:
                results = model.track(frame, device=self.device, **track_kwargs)

            dets = []
            if results and results[0].boxes is not None:
                boxes = results[0].boxes
                ids   = boxes.id
                for i, box in enumerate(boxes):
                    tid  = int(ids[i].item()) if ids is not None else i
                    x1,y1,x2,y2 = map(int, box.xyxy[0].tolist())
                    conf = float(box.conf[0])
                    cls  = int(box.cls[0])
                    name = model.names[cls]
                    dets.append((tid, x1, y1, x2, y2, conf, name))

            try:
                self.result_queue.put_nowait(dets)
            except queue.Full:
                self.result_queue.get_nowait()
                self.result_queue.put_nowait(dets)

            now = time.time()
            self.fps = 0.9 * self.fps + 0.1 * (1.0 / max(now - t_prev, 1e-6))
            t_prev = now


# ══════════════════════════════════════════════════════════════════════════════
#  MAIN
# ══════════════════════════════════════════════════════════════════════════════
def main():
    ap = argparse.ArgumentParser(description="ESP32-CAM Cinematic Detector")
    ap.add_argument("--ip",    default="192.168.1.42",
                    help="ESP32-CAM IP address")
    ap.add_argument("--port",  default=8082, type=int,
                    help="Stream port (default 8082)")
    ap.add_argument("--model", default="yolov8n",
                    help="YOLO model: name (yolov8n/s/m/l/x) OR path to .onnx/.pt file")
    ap.add_argument("--alpha", default=0.45, type=float,
                    help="Box smoothing 0.1(slow)–1.0(instant)")
    ap.add_argument("--width", default=1280, type=int)
    ap.add_argument("--height",default=720,  type=int)
    args = ap.parse_args()

    stream_url = f"http://{args.ip}:{args.port}/stream"
    # Detect best available provider for display
    try:
        import onnxruntime as ort
        _providers = ort.get_available_providers()
        if "DmlExecutionProvider" in _providers:
            device_str = "AMD GPU (DirectML)"
        elif "CUDAExecutionProvider" in _providers:
            device_str = f"NVIDIA CUDA"
        else:
            device_str = "CPU"
    except:
        device_str = "CPU"

    print(f"""
╔══════════════════════════════════════════╗
║     ESP32-CAM Cinematic Detector         ║
╠══════════════════════════════════════════╣
║  Stream : {stream_url:<31}║
║  Model  : {args.model:<31}║
║  Device : {device_str:<31}║
╠══════════════════════════════════════════╣
║  KEYS                                    ║
║   D  — toggle detection overlay         ║
║   +  — increase smoothing               ║
║   -  — decrease smoothing               ║
║   S  — save snapshot                    ║
║   Q  — quit                             ║
╚══════════════════════════════════════════╝
""")

    # ── threads / queues ──────────────────────────────────────────────────
    result_q = queue.Queue(maxsize=2)
    reader   = StreamReader(stream_url)
    worker   = InferenceWorker(args.model, DEVICE, result_q)
    tracker  = SmoothTracker(alpha=args.alpha)
    renderer = CinematicRenderer()

    reader.start()
    worker.start()

    # wait for first frame
    print("[STREAM] Connecting …")
    for _ in range(50):
        if reader.ok: break
        time.sleep(0.1)
    if not reader.ok:
        print(f"[ERROR] Cannot reach {stream_url}")
        sys.exit(1)
    print("[STREAM] Connected!")

    cv2.namedWindow("ESP32-CAM · Cinematic Detector", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("ESP32-CAM · Cinematic Detector", args.width, args.height)

    t_prev_rend = time.time()
    fps_rend    = 0.0
    snap_n      = 0
    current_tracks = []

    while True:
        frame = reader.latest()
        if frame is None:
            time.sleep(0.01)
            continue

        # push latest frame to GPU inference (non-blocking)
        worker.submit(frame)

        # drain latest detection result (CPU side)
        try:
            dets = result_q.get_nowait()
            tracker.update(dets)
            current_tracks = tracker.get()
        except queue.Empty:
            pass  # use previous tracks — boxes keep gliding

        # ── CPU rendering ────────────────────────────────────────────────
        vis = renderer.draw(frame, current_tracks,
                            worker.fps, fps_rend, device_str)

        now = time.time()
        fps_rend = 0.9 * fps_rend + 0.1 * (1.0 / max(now - t_prev_rend, 1e-6))
        t_prev_rend = now

        cv2.imshow("ESP32-CAM · Cinematic Detector", vis)
        key = cv2.waitKey(1) & 0xFF

        if key == ord('q'):
            break
        elif key == ord('d') or key == ord('D'):
            renderer.active = not renderer.active
            renderer.flash  = 1.0
            print(f"[UI] Detection overlay {'ON' if renderer.active else 'OFF'}")
        elif key == ord('+') or key == ord('='):
            tracker.alpha = min(1.0, tracker.alpha + 0.05)
            print(f"[UI] Smoothing alpha → {tracker.alpha:.2f}")
        elif key == ord('-'):
            tracker.alpha = max(0.05, tracker.alpha - 0.05)
            print(f"[UI] Smoothing alpha → {tracker.alpha:.2f}")
        elif key == ord('s') or key == ord('S'):
            fname = f"snapshot_{snap_n:04d}.jpg"
            cv2.imwrite(fname, vis)
            snap_n += 1
            print(f"[UI] Saved {fname}")

    worker.stop()
    cv2.destroyAllWindows()
    print("Bye!")


if __name__ == "__main__":
    main()