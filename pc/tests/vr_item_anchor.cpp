#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
using std::isfinite;
typedef float M34[3][4];
static struct {
    int active, head_pose_valid, have_anchor;
    float world_scale;
    M34 world_from_seated, head_pose;
} s_vr;
int g_pc_paused;
static int fp_active, flat_scene;
static int pc_fp_view_is_active() { return fp_active; }
static int pc_vr_flat_scene_active() { return flat_scene; }
#include "anchor_source.inc"

static int checks, failures;
static void check(bool condition, const char* label) {
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}
static bool near(double a, double b, double eps = 0.0001) { return std::fabs(a - b) < eps; }

/* Independent double-precision Y-X-Z rotation used to define a physical scene. */
static void rotation(double yaw, double pitch, double roll, double r[3][3]) {
    double cy = std::cos(yaw), sy = std::sin(yaw);
    double cp = std::cos(pitch), sp = std::sin(pitch);
    double cr = std::cos(roll), sr = std::sin(roll);
    r[0][0] = cy*cr + sy*sp*sr; r[0][1] = -cy*sr + sy*sp*cr; r[0][2] = sy*cp;
    r[1][0] = cp*sr; r[1][1] = cp*cr; r[1][2] = -sp;
    r[2][0] = -sy*cr + cy*sp*sr; r[2][1] = sy*sr + cy*sp*cr; r[2][2] = cy*cp;
}

static void scene(double scale, double anchor_yaw, double yaw, double pitch, double roll) {
    double wrot[3][3], hrot[3][3];
    rotation(anchor_yaw, 0, 0, wrot);
    rotation(yaw, pitch, roll, hrot);
    const double origin[3] = {1270.0, 43.0, -935.0}; // player can be anywhere in town
    const double height[3] = {0.0, -0.75, 0.0};
    const double head[3] = {0.23, 1.51, -0.18}; // room-scale offset, not centered
    s_vr.active = s_vr.head_pose_valid = s_vr.have_anchor = fp_active = 1;
    g_pc_paused = flat_scene = 0;
    s_vr.world_scale = (float)scale;
    for (int row = 0; row < 3; ++row) {
        s_vr.world_from_seated[row][3] = (float)origin[row];
        for (int col = 0; col < 3; ++col) {
            s_vr.world_from_seated[row][col] = (float)(wrot[col][row] / scale);
            s_vr.world_from_seated[row][3] -= (float)(wrot[col][row] * height[col] / scale);
            s_vr.head_pose[row][col] = (float)hrot[row][col];
        }
        s_vr.head_pose[row][3] = (float)head[row];
    }
    M34 before_w, before_h, result;
    std::memcpy(before_w, s_vr.world_from_seated, sizeof(M34));
    std::memcpy(before_h, s_vr.head_pose, sizeof(M34));
    check(pc_vr_item_presentation_mtx(&result[0][0]) == 1, "eligible scene has anchor");
    check(!std::memcmp(before_w, s_vr.world_from_seated, sizeof(M34)), "world transform unchanged");
    check(!std::memcmp(before_h, s_vr.head_pose, sizeof(M34)), "tracking pose unchanged");

    double seated[3], head_space[3] = {};
    for (int row = 0; row < 3; ++row) {
        seated[row] = height[row];
        for (int col = 0; col < 3; ++col)
            seated[row] += scale * wrot[row][col] * (result[col][3] - origin[col]);
    }
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            head_space[row] += hrot[col][row] * (seated[col] - head[col]);
    check(near(head_space[0], 0), "item horizontally centered in headset");
    check(near(head_space[1], -0.14), "item slightly below gaze");
    check(near(head_space[2], -0.90), "item 90cm ahead at every world scale");
    // Independent eye projections: both eyes must see it near the center.
    for (int eye = 0; eye < 2; ++eye) {
        double eye_x = eye ? 0.032 : -0.032;
        check(std::fabs((head_space[0] - eye_x) / -head_space[2]) < 0.05,
              "presentation within central stereo field");
    }
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            double billboard_eye = 0;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    billboard_eye += hrot[i][row] * wrot[i][j] * result[j][col];
            check(near(billboard_eye, row == col ? 1 : 0), "unit billboard faces tracked head");
        }
    }
}

static void rejected(const char* label) {
    float out[12], before[12];
    for (int i = 0; i < 12; ++i) before[i] = out[i] = (float)(100 + i);
    check(!pc_vr_item_presentation_mtx(out), label);
    check(!std::memcmp(before, out, sizeof(out)), "rejection leaves output unchanged");
}

int main() {
    const double scales[] = {0.01, 0.025, 0.06};
    const double anchors[] = {-2.2, 0.0, 1.4};
    const double yaws[] = {-2.8, -0.7, 0.0, 1.8};
    const double pitches[] = {-0.65, 0.0, 0.55};
    const double rolls[] = {0.0, 0.27};
    for (double scale : scales) for (double a : anchors) for (double y : yaws)
        for (double p : pitches) for (double r : rolls) scene(scale, a, y, p, r);
    check(!pc_vr_item_presentation_mtx(NULL), "null output rejected");
    s_vr.active = 0; rejected("flat desktop rejected"); s_vr.active = 1;
    fp_active = 0; rejected("third person rejected"); fp_active = 1;
    flat_scene = 1; rejected("flat scene rejected"); flat_scene = 0;
    g_pc_paused = 1; rejected("pause rejected"); g_pc_paused = 0;
    s_vr.head_pose_valid = 0; rejected("lost head tracking rejected"); s_vr.head_pose_valid = 1;
    s_vr.have_anchor = 0; rejected("uninitialized camera anchor rejected"); s_vr.have_anchor = 1;
    const float bad_scales[] = {0, -1, 0.00001f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()};
    for (float scale : bad_scales) { s_vr.world_scale = scale; rejected("invalid scale rejected"); }
    s_vr.world_scale = 0.06f;
    s_vr.head_pose[1][2] = std::numeric_limits<float>::quiet_NaN();
    rejected("invalid tracked pose rejected atomically");
    s_vr.head_pose[1][2] = 0;
    s_vr.world_from_seated[2][3] = std::numeric_limits<float>::infinity();
    rejected("invalid camera anchor rejected atomically");
    std::printf("VR item anchor: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
