#pragma once

#define ACTIVATION_RANGE         416
#define LABELS_FILE_NAME         "coco-dataset.labels"
#define YOLO_ONNX_FILE_NAME      "yolov8n.onnx"        // fast: 60 FPS COCO
#define YOLO_CSGO_FILE_NAME      "csgo_player.onnx"   // accurate but slower ~25 FPS