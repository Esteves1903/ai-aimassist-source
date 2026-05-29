#include "scanner.h"
#include "main.h"

void detector::draw_box(float conf, int left, int top, int right, int bottom, cv::Mat& frame, cv::Scalar color)
{
    cv::rectangle(frame, cv::Point(left, top), cv::Point(right, bottom), color);
    const std::string label = cv::format("%.2f", conf);
    cv::putText(frame, label, cv::Point(left, top), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255));
}

// scans for red enemy name tag above/near the YOLO box — returns centroid or (-1,-1)
static cv::Point2f findNameTag(const cv::Mat& frame, const cv::Rect& box)
{
    const int sx = std::max(0, box.x - box.width / 4);
    const int sy = std::max(0, box.y - box.height / 2);
    const int sw = std::min(frame.cols - sx, box.width + box.width / 2);
    const int sh = std::min(frame.rows - sy, box.height / 2 + box.height / 6);
    if (sw <= 0 || sh <= 0) return cv::Point2f(-1.f, -1.f);

    cv::Mat hsv;
    cv::cvtColor(frame(cv::Rect(sx, sy, sw, sh)), hsv, cv::COLOR_BGR2HSV);

    // red wraps around 0/180 in HSV — detect both halves
    cv::Mat m1, m2, mask;
    cv::inRange(hsv, cv::Scalar(0,   180, 150), cv::Scalar(8,   255, 255), m1);
    cv::inRange(hsv, cv::Scalar(172, 180, 150), cv::Scalar(180, 255, 255), m2);
    cv::bitwise_or(m1, m2, mask);

    const cv::Moments mom = cv::moments(mask, true);
    if (mom.m00 < 12.0) return cv::Point2f(-1.f, -1.f);

    return cv::Point2f(sx + (float)(mom.m10 / mom.m00),
                       sy + (float)(mom.m01 / mom.m00));
}

// samples pixels above the box to detect team indicator color
static bool isEnemy(const cv::Mat& frame, const cv::Rect& box)
{
    const int sample_w = box.width / 2 > 4 ? box.width / 2 : 4;
    const int sample_h = 6;
    const int sample_x = box.x + box.width / 4;
    const int sample_y = box.y - sample_h - 3;

    if (sample_x < 0 || sample_y < 0 ||
        sample_x + sample_w > frame.cols ||
        sample_y + sample_h > frame.rows)
        return true; // out of bounds — assume enemy

    const cv::Scalar mean_color = cv::mean(frame(cv::Rect(sample_x, sample_y, sample_w, sample_h)));
    const double blue  = mean_color[0];
    const double green = mean_color[1];
    const double red   = mean_color[2];

    if (green > red * 1.4 && green > blue * 1.2) return false; // green dominant = friendly
    return true; // red dominant or unclear = enemy
}

std::vector<cv::String> detector::get_outputs_names(const cv::dnn::Net& net)
{
    static std::vector<cv::String> names;
    if (names.empty())
    {
        const std::vector<int> outLayers = net.getUnconnectedOutLayers();
        const std::vector<cv::String> layersNames = net.getLayerNames();
        names.resize(outLayers.size());
        for (size_t i = 0; i < outLayers.size(); ++i)
            names[i] = layersNames[outLayers[i] - 1];
    }
    return names;
}
static bool IsCursorVisible()
{
    CURSORINFO mouseInfo = { sizeof(CURSORINFO) };
    if (GetCursorInfo(&mouseInfo))
    {
        const HCURSOR handle = mouseInfo.hCursor;
        return (reinterpret_cast<intptr_t>(handle) > 50000) && (reinterpret_cast<intptr_t>(handle) < 100000);
    }
    return false;
}

void detector::postprocess(cv::Mat& frame, const std::vector<cv::Mat>& outs)
{
    std::vector<int> classes_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    // YOLOv8 ONNX output: [1, 84, num_anchors]
    // 84 = [cx, cy, w, h] in pixel space (0..ACTIVATION_RANGE) + 80 class scores
    // No separate objectness — max class score IS the confidence
    if (!outs.empty() && outs[0].dims == 3)
    {
        const int num_channels = outs[0].size[1]; // 84
        const int num_anchors  = outs[0].size[2];

        // Create [num_channels, num_anchors] view, transpose to [num_anchors, num_channels]
        cv::Mat raw(num_channels, num_anchors, CV_32F, (void*)outs[0].ptr<float>());
        cv::Mat data;
        cv::transpose(raw, data);

        for (int i = 0; i < data.rows; i++)
        {
            const cv::Mat scores = data.row(i).colRange(4, data.cols);
            cv::Point class_id_point;
            double max_score;
            cv::minMaxLoc(scores, nullptr, &max_score, nullptr, &class_id_point);

            if ((float)max_score < var::confidence) continue;
            if (class_id_point.x != 0) continue; // person class only

            const float* row = data.ptr<float>(i);
            const int centerX = (int)row[0];
            const int centerY = (int)row[1];
            const int width   = (int)row[2];
            const int height  = (int)row[3];

            // tamanho minimo
            if (width < 10 || height < 22) continue;
            // caixas demasiado largas = paredes/chao
            if (height < width * 0.55f) continue;
            // postes/arvores: muito estreitos e altos
            if (height > width * 7) continue;
            // caixas demasiado grandes = nao e jogador
            if (width > frame.cols * 0.55f) continue;
            if (height > frame.rows * 0.85f) continue;
            // ceu/teto: caixa inteiramente no topo 20%
            if (centerY + height / 2 < (int)(frame.rows * 0.20f)) continue;
            // arma/maos do proprio jogador: caixa no fundo 15% do frame
            if (centerY - height / 2 > (int)(frame.rows * 0.85f)) continue;

            const int left = centerX - width / 2;
            const int top  = centerY - height / 2;

            classes_ids.push_back(class_id_point.x);
            confidences.push_back((float)max_score);
            boxes.push_back(cv::Rect(left, top, width, height));
        }
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, var::confidence, m_threshold, indices);

    const int frame_cx = frame.cols / 2;
    const int frame_cy = frame.rows / 2;
    int best_idx = -1;
    float best_dist = FLT_MAX;

    static int lock_cx = -1, lock_cy = -1;
    const float lock_radius_sq = var::sticky_radius * var::sticky_radius;
    int locked_idx = -1;
    float locked_dist = FLT_MAX;

    const int scr_w = GetSystemMetrics(SM_CXSCREEN);
    const int scr_h = GetSystemMetrics(SM_CYSCREEN);
    const float esp_scale = (float)ACTIVATION_RANGE / (float)m_activation_range;
    var::radar_enemy_count = 0;
    var::esp_box_count = 0;
    for (size_t i = 0; i < indices.size(); ++i)
    {
        const int idx = indices[i];
        const cv::Rect& box = boxes[idx];
        const bool enemy = isEnemy(frame, box);

        const cv::Scalar box_color = enemy ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
        draw_box(confidences[idx], box.x, box.y, box.x + box.width, box.y + box.height, frame, box_color);

        if (!enemy) continue;

        const int cx = box.x + box.width / 2;
        const int cy = box.y + box.height / 2;

        if (var::radar_enemy_count < 32)
            var::radar_enemies[var::radar_enemy_count++] = { (float)(cx - frame_cx), (float)(cy - frame_cy) };

        if (var::esp_box_count < 32)
            var::esp_boxes[var::esp_box_count++] = {
                (int)((scr_w - ACTIVATION_RANGE) / 2.0f + box.x * esp_scale),
                (int)((scr_h - ACTIVATION_RANGE) / 2.0f + box.y * esp_scale),
                (int)(box.width  * esp_scale),
                (int)(box.height * esp_scale)
            };

        const float dist_center = static_cast<float>((cx - frame_cx) * (cx - frame_cx) + (cy - frame_cy) * (cy - frame_cy));

        // check if this detection matches the locked target
        if (lock_cx >= 0)
        {
            const float dist_lock = static_cast<float>((cx - lock_cx) * (cx - lock_cx) + (cy - lock_cy) * (cy - lock_cy));
            if (dist_lock < lock_radius_sq && dist_lock < locked_dist)
            {
                locked_dist = dist_lock;
                locked_idx = idx;
            }
        }

        if (dist_center < best_dist)
        {
            best_dist = dist_center;
            best_idx = idx;
        }
    }

    // prefer locked target, fall back to closest
    if (locked_idx >= 0)
    {
        best_idx = locked_idx;
        best_dist = locked_dist;
    }

    // update lock position
    if (best_idx >= 0)
    {
        lock_cx = boxes[best_idx].x + boxes[best_idx].width / 2;
        lock_cy = boxes[best_idx].y + boxes[best_idx].height / 2;
    }
    else
    {
        lock_cx = lock_cy = -1;
    }

    // enforce FOV circle if enabled
    if (best_idx >= 0 && var::fovCircle)
    {
        const float scale = (float)ACTIVATION_RANGE / (float)m_activation_range;
        const float fov_in_yolo = var::fov_radius / scale;
        if (best_dist > fov_in_yolo * fov_in_yolo)
            best_idx = -1;
    }

    const bool target_in_crosshair = best_idx >= 0 && best_dist <= var::trigger_fov * var::trigger_fov;
    aimbot::trigger_shoot(target_in_crosshair);

    if (best_idx >= 0)
    {
        const cv::Rect& box = boxes[best_idx];
        var::boxX   = box.x;
        var::boxY   = box.y;
        var::Height = box.height;
        var::Width  = box.width;

        // color refinement: find red name tag and use it for precise head position
        if (var::color_aim)
        {
            const cv::Point2f tag = findNameTag(frame, box);
            if (tag.x >= 0.f)
            {
                // name tag centroid ≈ head level — override YOLO box position
                var::boxX   = (int)(tag.x) - box.width / 2;
                var::boxY   = (int)(tag.y);
                var::Height = box.height / 5; // small height → aim stays near tag level
            }
        }

        var::screen_box_x = (int)((scr_w - ACTIVATION_RANGE) / 2.0f + box.x * esp_scale);
        var::screen_box_y = (int)((scr_h - ACTIVATION_RANGE) / 2.0f + box.y * esp_scale);
        var::screen_box_w = (int)(box.width  * esp_scale);
        var::screen_box_h = (int)(box.height * esp_scale);

        if (!IsCursorVisible() && var::aim_active)
            aimbot::aim_to(var::boxX, var::boxY, var::Width, var::Height);
    }
    else
    {
        var::screen_box_w = 0;
    }
}


static DWORD s_fps = 0;

void detector::start(cv::Mat& image)
{
    const DWORD current_ticks = GetTickCount();

    cv::Mat blob;
    cv::dnn::blobFromImage(image, blob, 1.0 / 255.0,
        cv::Size(m_activation_range, m_activation_range), cv::Scalar(0, 0, 0), true, false);
    m_net.setInput(blob);
    std::vector<cv::Mat> outs;
    m_net.forward(outs, get_outputs_names(m_net));

    postprocess(image, outs);

    const std::string label = cv::format("FPS: %u [%s]", static_cast<unsigned int>(s_fps), m_backend_name.c_str());
    cv::putText(image, label, cv::Point(0, 15),
        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255));

    cv::Mat detected_frame;
    image.convertTo(detected_frame, CV_8U);
    frame = detected_frame;
    cv::imshow("detection", detected_frame);


    const DWORD delta_ms = GetTickCount() - current_ticks;
    if (delta_ms > 0)
        s_fps = 1000 / delta_ms;
}
void detector::setupOptimalBackend()
{
    // Try CUDA first (fastest if available)
    try
    {
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        
        // Test if CUDA is actually available by trying a forward pass
        cv::Mat test_blob = cv::dnn::blobFromImage(cv::Mat::zeros(m_activation_range, m_activation_range, CV_8UC3), 1.0 / 255.0);
        m_net.setInput(test_blob);
        std::vector<cv::Mat> test_outs;
        m_net.forward(test_outs, get_outputs_names(m_net));
        
        if (!test_outs.empty())
        {
            m_backend_name = "CUDA";
            std::cout << "[Detector] Using CUDA backend (GPU accelerated)" << std::endl;
            return;
        }
    }
    catch (const cv::Exception& e)
    {
        // CUDA not available or failed, try OpenCL
    }
    catch (...)
    {
        // CUDA not available, try OpenCL
    }

    // Try OpenCL (good performance, widely supported)
    if (cv::ocl::haveOpenCL())
    {
        try
        {
            cv::ocl::setUseOpenCL(true);
            cv::ocl::useOpenCL();
            
            m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
            m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
            
            // Test OpenCL
            cv::Mat test_blob = cv::dnn::blobFromImage(cv::Mat::zeros(m_activation_range, m_activation_range, CV_8UC3), 1.0 / 255.0);
            m_net.setInput(test_blob);
            std::vector<cv::Mat> test_outs;
            m_net.forward(test_outs, get_outputs_names(m_net));
            
            if (!test_outs.empty())
            {
                m_backend_name = "OpenCL";
                std::cout << "[Detector] Using OpenCL backend (GPU accelerated)" << std::endl;
                return;
            }
        }
        catch (const cv::Exception& e)
        {
            cv::ocl::setUseOpenCL(false);
        }
        catch (...)
        {
            cv::ocl::setUseOpenCL(false);
        }
    }

    // Fallback to CPU (always available)
    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    m_backend_name = "CPU";
    std::cout << "[Detector] Using CPU backend (fallback mode)" << std::endl;
}

detector::detector(std::string dataset_labels_path, std::string onnx_path)
{
    std::ifstream file_labels(dataset_labels_path);
    std::string line;
    while (std::getline(file_labels, line))
        m_classes.push_back(line);

    m_net = cv::dnn::readNetFromONNX(onnx_path);
    if (m_net.empty())
    {
        MessageBoxA(NULL, "Failed to load YOLOv8 model!\nPlace yolov8n.onnx in the same folder as the exe.", "Error", MB_ICONERROR);
        exit(1);
    }

    if (var::debug_console)
        std::cout << "[Scanner] YOLOv8 model loaded!" << std::endl;

    m_activation_range = ACTIVATION_RANGE;
    setupOptimalBackend();
}
