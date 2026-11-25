// results.cpp
#include "results.hpp"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "checksum.hpp"

namespace fs = std::filesystem;

namespace pdc {

namespace {

std::string timestamp_now() {
    const std::time_t t = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

// Quotes a CSV field and escapes embedded quotes.
std::string csv_field(const std::string& s) {
    std::string out = "\"";
    for (char c : s) out += (c == '"') ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}

}  // namespace

void print_result(const RunResult& r) {
    std::printf("\n=== %s | %d process(es) x %d thread(s) | %d images ===\n",
                r.version.c_str(), r.procs, r.threads, r.images);
    std::printf("  read     %10.3f s\n", r.times.read);
    std::printf("  compute  %10.3f s\n", r.times.compute);
    std::printf("  comm     %10.3f s\n", r.times.comm);
    std::printf("  write    %10.3f s\n", r.times.write);
    std::printf("  TOTAL    %10.3f s\n", r.total);
    std::printf("  checksum %s\n", to_hex(r.checksum).c_str());
}

void append_csv(const std::string& path, const RunResult& r) {
    const bool is_new = !fs::exists(path) || fs::file_size(path) == 0;
    if (fs::path(path).has_parent_path()) fs::create_directories(fs::path(path).parent_path());

    std::ofstream f(path, std::ios::app);
    if (!f) throw std::runtime_error("Could not open CSV file: " + path);

    if (is_new)
        f << "timestamp,version,procs,threads,workers,images,schedule,"
             "read_s,compute_s,comm_s,write_s,total_s,checksum,label\n";

    char nums[256];
    std::snprintf(nums, sizeof(nums), "%.6f,%.6f,%.6f,%.6f,%.6f",
                  r.times.read, r.times.compute, r.times.comm, r.times.write, r.total);

    f << timestamp_now() << ',' << r.version << ',' << r.procs << ',' << r.threads << ','
      << r.procs * r.threads << ',' << r.images << ',' << r.schedule << ',' << nums << ','
      << to_hex(r.checksum) << ',' << csv_field(r.label) << '\n';
}

}  // namespace pdc
