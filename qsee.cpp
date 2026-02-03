#include "Input.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// --- Data Structures ---
struct Atom {
  std::string element;
  double x, y, z;
};

struct Vec3 {
  double x, y, z;
};

// Modular input parameter structure (ready for future descriptions)
struct InputParameter {
  std::string section;     // e.g., "QM", "BASIS", "SCF"
  std::string key;         // e.g., "reference", "basis"
  std::string value;       // e.g., "GBLYP", "6-31G(D)"
  std::string description; // For future use: explanation of parameter
};

// Complete input file data
struct InputFileData {
  std::string filename;
  std::string title;         // From comment at top
  std::string chronusq_line; // The chronusq: directive
  int charge = 0;
  int multiplicity = 1;
  std::vector<Atom> atoms;
  std::vector<InputParameter> parameters;

  // Get element composition string (e.g., "H5" or "C6H12O6")
  std::string get_formula() const {
    std::unordered_map<std::string, int> counts;
    for (const auto &atom : atoms) {
      counts[atom.element]++;
    }
    // Standard order: C, H, then alphabetical
    std::string formula;
    if (counts.count("C")) {
      formula += "C" + (counts["C"] > 1 ? std::to_string(counts["C"]) : "");
      counts.erase("C");
    }
    if (counts.count("H")) {
      formula += "H" + (counts["H"] > 1 ? std::to_string(counts["H"]) : "");
      counts.erase("H");
    }
    std::vector<std::string> others;
    for (const auto &[elem, count] : counts) {
      others.push_back(elem);
    }
    std::sort(others.begin(), others.end());
    for (const auto &elem : others) {
      formula += elem + (counts[elem] > 1 ? std::to_string(counts[elem]) : "");
    }
    return formula;
  }
};

// --- Globals for signal handling ---
volatile sig_atomic_t running = 1;

void signal_handler(int) { running = 0; }

// --- Base64 encoding for kitty protocol ---
std::string base64_encode(const std::vector<uint8_t> &data) {
  static const char lookup[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int val = 0, valb = -6;
  for (uint8_t c : data) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(lookup[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6)
    out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4)
    out.push_back('=');
  return out;
}

// --- Element colors (RGB) ---
struct Color {
  uint8_t r, g, b;
};

Color get_element_color(const std::string &element) {
  static std::unordered_map<std::string, Color> colors = {
      {"H", {255, 255, 255}}, // White
      {"C", {144, 144, 144}}, // Grey
      {"N", {48, 80, 248}},   // Blue
      {"O", {255, 13, 13}},   // Red
      {"S", {255, 255, 48}},  // Yellow
      {"P", {255, 128, 0}},   // Orange
      {"F", {144, 224, 80}},  // Green
      {"Cl", {31, 240, 31}},  // Green
      {"Br", {166, 41, 41}},  // Brown
  };
  auto it = colors.find(element);
  if (it != colors.end())
    return it->second;
  return {200, 200, 200}; // Default grey
}

// --- File parsing ---
InputFileData parse_inp_file(const std::string &filename) {
  InputFileData data;
  data.filename = filename;

  // 1. Manually scan for Title (first meaningful comment)
  {
    std::ifstream file(filename);
    if (file.is_open()) {
      std::string line;
      bool found_title = false;
      while (std::getline(file, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos)
          continue;
        line = line.substr(start);

        if (line[0] == '#') {
          if (line.length() > 1) {
            std::string comment = line.substr(1);
            size_t cstart = comment.find_first_not_of(" \t");
            if (cstart != std::string::npos) {
              data.title = comment.substr(cstart);
              found_title = true;
              break; // Found it
            }
          }
        } else if (line[0] == '[') {
          break; // Hit a section, stop looking for title
        }
      }
    }
  }

  // 2. Use Robust Input Parser
  try {
    Input input(filename);
    input.parse();

    // Retrieve simple properties
    if (input.containsData("MOLECULE.CHARGE"))
      data.charge = input.getData<int>("MOLECULE.CHARGE");
    if (input.containsData("MOLECULE.MULT"))
      data.multiplicity = input.getData<int>("MOLECULE.MULT");

    // Geometry
    std::string geom_str;
    if (input.containsData("MOLECULE.GEOM")) {
      geom_str = input.getData<std::string>("MOLECULE.GEOM");
    } else if (input.containsData("GEOMETRY")) { // Fallback/Alternative
      geom_str = input.getData<std::string>("GEOMETRY");
    }

    if (!geom_str.empty()) {
      std::istringstream iss(geom_str);
      std::string line;
      while (std::getline(iss, line)) {
        std::istringstream ls(line);
        Atom atom;
        if (ls >> atom.element >> atom.x >> atom.y >> atom.z) {
          data.atoms.push_back(atom);
        }
      }
    }

    // Populate Parameters for Display
    for (const auto &kv : input.getDict()) {
      std::string full_key = kv.first;
      std::string value = kv.second;

      // Skip Geometry blob in parameters list to avoid clutter
      if (full_key == "MOLECULE.GEOM" || full_key == "GEOMETRY")
        continue;

      InputParameter param;
      size_t dot_pos = full_key.find('.');
      if (dot_pos != std::string::npos) {
        param.section = full_key.substr(0, dot_pos);
        param.key = full_key.substr(dot_pos + 1);
      } else {
        param.section = "GLOBAL";
        param.key = full_key;
      }
      param.value = value;
      data.parameters.push_back(param);
    }

  } catch (const std::exception &e) {
    std::cerr << "Parser Error: " << e.what() << std::endl;
  }

  return data;
}

// --- 3D Math ---
Vec3 rotate_x(const Vec3 &v, double angle) {
  double c = std::cos(angle);
  double s = std::sin(angle);
  return {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
}

Vec3 rotate_y(const Vec3 &v, double angle) {
  double c = std::cos(angle);
  double s = std::sin(angle);
  return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

Vec3 rotate_z(const Vec3 &v, double angle) {
  double c = std::cos(angle);
  double s = std::sin(angle);
  return {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
}

// Camera view modes
enum class ViewMode { ISOMETRIC, XY, XZ, YZ };

// Apply initial camera rotation based on view mode
Vec3 apply_camera_view(const Vec3 &v, ViewMode mode) {
  switch (mode) {
  case ViewMode::XY:
    // Looking down Z-axis (no rotation needed)
    return v;
  case ViewMode::XZ:
    // Looking down Y-axis (rotate -90° around X)
    return rotate_x(v, -M_PI / 2.0);
  case ViewMode::YZ:
    // Looking down X-axis (rotate 90° around Y)
    return rotate_y(v, M_PI / 2.0);
  case ViewMode::ISOMETRIC:
  default:
    // 3/4 view: rotate to see from (1, 1, 1) direction
    // First tilt down ~35.26° (arctan(1/√2)), then rotate 45° around Y
    Vec3 tilted = rotate_x(v, -M_PI / 5.5); // ~32° down tilt
    return rotate_y(tilted, M_PI / 4.0);    // 45° Y rotation
  }
}

// --- Circle drawing (Bresenham's algorithm) ---
void draw_circle_outline(std::vector<uint8_t> &rgba, int width, int height,
                         int cx, int cy, int radius, const Color &color) {
  auto set_pixel = [&](int x, int y) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
      int idx = (y * width + x) * 4;
      rgba[idx] = color.r;
      rgba[idx + 1] = color.g;
      rgba[idx + 2] = color.b;
      rgba[idx + 3] = 255;
    }
  };

  // Draw 8 symmetric points
  auto plot_circle_points = [&](int x, int y) {
    set_pixel(cx + x, cy + y);
    set_pixel(cx - x, cy + y);
    set_pixel(cx + x, cy - y);
    set_pixel(cx - x, cy - y);
    set_pixel(cx + y, cy + x);
    set_pixel(cx - y, cy + x);
    set_pixel(cx + y, cy - x);
    set_pixel(cx - y, cy - x);
  };

  int x = 0, y = radius;
  int d = 3 - 2 * radius;

  while (x <= y) {
    plot_circle_points(x, y);
    if (d < 0) {
      d = d + 4 * x + 6;
    } else {
      d = d + 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

// --- Kitty Graphics Protocol ---
void display_frame(const std::vector<uint8_t> &rgba, int width, int height,
                   int col_offset) {
  std::string payload = base64_encode(rgba);

  // Delete previous image with id=1 first
  std::cout << "\033_Ga=d,d=i,i=1;\033\\";

  // Move cursor to position for image (row 1, column col_offset)
  std::cout << "\033[1;" << col_offset << "H";

  // Transmit and display new frame
  // a=T: transmit and display, f=32: RGBA, s/v: dimensions
  // i=1: image id, q=2: quiet mode (suppress responses)
  std::cout << "\033_Ga=T,f=32,s=" << width << ",v=" << height << ",i=1,q=2;"
            << payload << "\033\\" << std::flush;
}

void clear_graphics() {
  // Delete all images with id=1
  std::cout << "\033_Ga=d,d=i,i=1;\033\\" << std::flush;
}

// --- Terminal text styling ---
namespace style {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string DIM = "\033[2m";
const std::string CYAN = "\033[36m";
const std::string YELLOW = "\033[33m";
const std::string GREEN = "\033[32m";
const std::string MAGENTA = "\033[35m";
const std::string WHITE = "\033[97m";
const std::string BLUE = "\033[34m";
} // namespace style

// --- Info display ---
// Helper to print at specific row, column
void print_at(int row, int col, const std::string &text) {
  std::cout << "\033[" << row << ";" << col << "H" << text;
}

void display_info_panel(const InputFileData &data, int image_cols) {
  // Text goes on LEFT, image goes on RIGHT
  // image_cols tells us where the image starts (approximately)
  // We print text from column 1 up to image_cols - 2

  int row = 1;
  int text_width = image_cols - 4; // Leave some padding before image
  if (text_width < 30)
    text_width = 30; // Minimum width

  // Extract just the base filename for display
  std::string display_name = data.filename;
  size_t last_slash = display_name.find_last_of("/\\");
  if (last_slash != std::string::npos) {
    display_name = display_name.substr(last_slash + 1);
  }

  // File header
  print_at(row, 1,
           "\033[K" + style::BOLD + style::CYAN +
               "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" + style::RESET);
  row++;
  print_at(row, 1,
           "\033[K" + style::BOLD + style::WHITE + " 📁 " + display_name +
               style::RESET);
  row++;
  if (!data.title.empty()) {
    print_at(row, 1,
             "\033[K" + style::DIM + "    " + data.title + style::RESET);
    row++;
  }
  print_at(row, 1,
           "\033[K" + style::BOLD + style::CYAN +
               "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" + style::RESET);
  row++;
  row++; // blank line

  // Molecule info
  print_at(row, 1,
           "\033[K" + style::BOLD + style::YELLOW + " ⚛  MOLECULE" +
               style::RESET);
  row++;
  print_at(row, 1,
           "\033[K" + style::DIM + " ─────────────────────────────" +
               style::RESET);
  row++;
  print_at(row, 1,
           "\033[K    Formula:      " + style::BOLD + data.get_formula() +
               style::RESET);
  row++;
  print_at(row, 1,
           "\033[K    Atoms:        " + std::to_string(data.atoms.size()));
  row++;
  print_at(
      row, 1,
      "\033[K    Charge:       " + std::string(data.charge >= 0 ? "+" : "") +
          std::to_string(data.charge));
  row++;
  print_at(row, 1,
           "\033[K    Multiplicity: " + std::to_string(data.multiplicity));
  row++;
  row++; // blank line

  // Parameters section header
  print_at(row, 1,
           "\033[K" + style::BOLD + style::WHITE + " ⚙  INPUT PARAMETERS" +
               style::RESET);
  row++;
  print_at(row, 1,
           "\033[K" + style::BOLD + style::CYAN +
               "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" + style::RESET);
  row++;

  // Group parameters by section
  std::unordered_map<std::string, std::vector<const InputParameter *>> sections;
  for (const auto &param : data.parameters) {
    // Skip MOLECULE section items as we show them above
    if (param.section != "MOLECULE") {
      sections[param.section].push_back(&param);
    }
  }

  // Display sections in order
  std::vector<std::string> section_order = {"QM", "BASIS", "SCF", "MISC",
                                            "INTS"};
  for (const auto &sec_name : section_order) {
    if (sections.count(sec_name) && !sections[sec_name].empty()) {
      print_at(row, 1,
               "\033[K" + style::BOLD + style::GREEN + "  " + sec_name +
                   style::RESET);
      row++;
      for (const auto *param : sections[sec_name]) {
        std::string line = "     " + param->key + ": " + param->value;
        print_at(row, 1,
                 "\033[K" + style::CYAN + line.substr(0, text_width) +
                     style::RESET);
        row++;
      }
      row++;
      sections.erase(sec_name);
    }
  }

  // Display any remaining sections
  for (const auto &[sec_name, params] : sections) {
    if (!params.empty()) {
      print_at(row, 1,
               "\033[K" + style::BOLD + style::MAGENTA + "  " + sec_name +
                   style::RESET);
      row++;
      for (const auto *param : params) {
        std::string line = "     " + param->key + ": " + param->value;
        print_at(row, 1,
                 "\033[K" + style::CYAN + line.substr(0, text_width) +
                     style::RESET);
        row++;
      }
      row++;
    }
  }

  // Exit instructions
  print_at(row, 1,
           "\033[K" + style::DIM + " Press Ctrl+C to exit" + style::RESET);

  std::cout << std::flush;
}

// --- Main ---
int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input.inp> [-xy|-xz|-yz]"
              << std::endl;
    std::cerr << "  -xy : View the XY plane (camera along Z-axis)" << std::endl;
    std::cerr << "  -xz : View the XZ plane (camera along Y-axis)" << std::endl;
    std::cerr << "  -yz : View the YZ plane (camera along X-axis)" << std::endl;
    std::cerr << "  (default: isometric 3/4 view)" << std::endl;
    return 1;
  }

  // Parse command line for view mode
  ViewMode view_mode = ViewMode::ISOMETRIC;
  for (int i = 2; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-xy" || arg == "xy")
      view_mode = ViewMode::XY;
    else if (arg == "-xz" || arg == "xz")
      view_mode = ViewMode::XZ;
    else if (arg == "-yz" || arg == "yz")
      view_mode = ViewMode::YZ;
  }

  // Parse input file
  InputFileData input_data = parse_inp_file(argv[1]);
  if (input_data.atoms.empty()) {
    std::cerr << "No atoms found in input file." << std::endl;
    return 1;
  }

  // Create reference to atoms for easier use
  std::vector<Atom> &atoms = input_data.atoms;

  std::cerr << "Loaded " << atoms.size() << " atoms ("
            << input_data.get_formula() << ")" << std::endl;

  // Setup signal handler for clean exit
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);

  // Rendering parameters
  const int width = 256;
  const int height = 256;
  const int atom_radius = 12;

  // Animation parameters
  // 1 rotation per 6 seconds = π/3 rad/s
  const double rotation_speed = M_PI / 3.0;
  const int target_fps = 30;
  const auto frame_duration = std::chrono::milliseconds(1000 / target_fps);

  // Find center of molecule for centering
  double cx = 0, cy = 0, cz = 0;
  for (const auto &atom : atoms) {
    cx += atom.x;
    cy += atom.y;
    cz += atom.z;
  }
  cx /= atoms.size();
  cy /= atoms.size();
  cz /= atoms.size();

  // Center atoms
  for (auto &atom : atoms) {
    atom.x -= cx;
    atom.y -= cy;
    atom.z -= cz;
  }

  // Calculate bounding box to determine proper scale
  // We need to find the max extent considering rotation (so use max of all
  // dims)
  double max_extent = 0.0;
  for (const auto &atom : atoms) {
    // Since we rotate, any axis could become the projected x or y
    // So use the 3D distance from origin as worst case
    double dist =
        std::sqrt(atom.x * atom.x + atom.y * atom.y + atom.z * atom.z);
    if (dist > max_extent)
      max_extent = dist;
  }

  // Scale to fit in viewport with padding for atom radius
  // viewport_radius = half of smallest dimension minus padding
  double viewport_radius = (std::min(width, height) / 2.0) - atom_radius - 10;
  double scale = (max_extent > 0.001) ? (viewport_radius / max_extent) : 80.0;

  double angle = 0.0;
  auto last_time = std::chrono::steady_clock::now();

  // Enter alternate screen buffer (preserves command history)
  std::cout << "\033[?1049h"; // Enter alternate screen
  std::cout << "\033[?25l";   // Hide cursor
  std::cout << "\033[2J";     // Clear screen
  std::cout << "\033[H";      // Move to home position
  std::cout << std::flush;

  while (running) {
    // Home cursor (don't clear screen - causes flickering)
    std::cout << "\033[H" << std::flush;

    auto frame_start = std::chrono::steady_clock::now();

    // Calculate delta time
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time).count();
    last_time = now;

    // Update rotation angle
    angle += rotation_speed * dt;
    if (angle > 2.0 * M_PI)
      angle -= 2.0 * M_PI;

    // Create frame buffer (transparent background)
    std::vector<uint8_t> rgba(width * height * 4, 0);

    // Transform and project atoms
    struct ProjectedAtom {
      int x, y;
      double z;
      Color color;
    };
    std::vector<ProjectedAtom> projected;

    for (const auto &atom : atoms) {
      Vec3 pos = {atom.x, atom.y, atom.z};

      // Apply initial camera view transformation
      Vec3 viewed = apply_camera_view(pos, view_mode);

      // Apply animation rotation (around Y-axis)
      Vec3 rotated = rotate_y(viewed, angle);

      // Orthographic projection (simple x, y mapping)
      int screen_x = static_cast<int>(width / 2.0 + rotated.x * scale);
      int screen_y =
          static_cast<int>(height / 2.0 - rotated.y * scale); // Flip Y

      projected.push_back(
          {screen_x, screen_y, rotated.z, get_element_color(atom.element)});
    }

    // Sort by depth (back to front)
    std::sort(projected.begin(), projected.end(),
              [](const ProjectedAtom &a, const ProjectedAtom &b) {
                return a.z < b.z; // Draw far atoms first
              });

    // Draw atoms
    for (const auto &p : projected) {
      draw_circle_outline(rgba, width, height, p.x, p.y, atom_radius, p.color);
    }

    // Display frame at right side of screen
    // Assuming 40 columns for text on left, image starts at column 42
    int text_columns = 42;
    display_frame(rgba, width, height, text_columns);

    // Display info panel on left side
    display_info_panel(input_data, text_columns);

    // Frame timing
    auto frame_end = std::chrono::steady_clock::now();
    auto elapsed = frame_end - frame_start;
    if (elapsed < frame_duration) {
      std::this_thread::sleep_for(frame_duration - elapsed);
    }
  }

  // Cleanup
  clear_graphics();
  std::cout << "\033[?25h"; // Show cursor
  std::cout
      << "\033[?1049l"; // Exit alternate screen (restores original terminal)
  std::cout << std::flush;
  std::cerr << "Exited cleanly." << std::endl;

  return 0;
}
