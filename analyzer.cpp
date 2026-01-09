#include "analyzer.h"

#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <vector>

using namespace std;

namespace {

struct Store {
    unordered_map<string, long long> zoneCount;
    unordered_map<string, unordered_map<int, long long>> zoneHourCount;
};

static unordered_map<const TripAnalyzer*, Store>& storage() {
    static unordered_map<const TripAnalyzer*, Store> s;
    return s;
}

static Store& getStore(const TripAnalyzer* self) {
    return storage()[self];
}

static inline bool isWS(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static string trimCopy(const string& s) {
    size_t i = 0;
    while (i < s.size() && isWS(s[i])) ++i;
    if (i == s.size()) return "";

    size_t j = s.size();
    while (j > i && isWS(s[j - 1])) --j;

    return s.substr(i, j - i);
}

static void splitCSV(const string& line, vector<string>& out) {
    out.clear();
    out.reserve(8);

    size_t start = 0;
    while (true) {
        size_t comma = line.find(',', start);
        if (comma == string::npos) {
            out.emplace_back(line.substr(start));
            break;
        }
        out.emplace_back(line.substr(start, comma - start));
        start = comma + 1;
    }
}

static bool parseHour(const string& raw, int& hour) {
    // keep same behavior: trim, then expect hour at positions 11-12
    string t = trimCopy(raw);
    if (t.size() < 13) return false;

    char d1 = t[11];
    char d2 = t[12];
    if (d1 < '0' || d1 > '9' || d2 < '0' || d2 > '9') return false;

    int h = (d1 - '0') * 10 + (d2 - '0');
    if (h < 0 || h > 23) return false;

    hour = h;
    return true;
}

} // namespace

void TripAnalyzer::ingestFile(const string& csvPath) {
    Store& st = getStore(this);
    st.zoneCount.clear();
    st.zoneHourCount.clear();

    ifstream fin(csvPath);
    if (!fin) return;

    string line;
    if (!getline(fin, line)) return; // skip header (same)

    vector<string> cols;
    cols.reserve(8);

    while (getline(fin, line)) {
        if (line.empty()) continue;

        splitCSV(line, cols);
        if (cols.size() < 6) continue;

        string zone = trimCopy(cols[1]);
        string dt   = trimCopy(cols[3]);
        if (zone.empty() || dt.empty()) continue;

        int hour = 0;
        if (!parseHour(dt, hour)) continue;

        ++st.zoneCount[zone];
        ++st.zoneHourCount[zone][hour];
    }
}

vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    vector<ZoneCount> out;
    if (k <= 0) return out;

    const Store& st = getStore(this);
    out.reserve(st.zoneCount.size());

    for (const auto& [zone, cnt] : st.zoneCount) {
        out.push_back(ZoneCount{zone, cnt});
    }

    sort(out.begin(), out.end(),
         [](const ZoneCount& a, const ZoneCount& b) {
             if (a.count != b.count) return a.count > b.count;
             return a.zone < b.zone;
         });

    if ((int)out.size() > k) out.resize(k);
    return out;
}

vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    vector<SlotCount> out;
    if (k <= 0) return out;

    const Store& st = getStore(this);

    // optional micro-opt: reserve approximate size
    size_t approx = 0;
    for (const auto& z : st.zoneHourCount) approx += z.second.size();
    out.reserve(approx);

    for (const auto& [zone, hm] : st.zoneHourCount) {
        for (const auto& [hour, cnt] : hm) {
            out.push_back(SlotCount{zone, hour, cnt});
        }
    }

    sort(out.begin(), out.end(),
         [](const SlotCount& a, const SlotCount& b) {
             if (a.count != b.count) return a.count > b.count;
             if (a.zone != b.zone) return a.zone < b.zone;
             return a.hour < b.hour;
         });

    if ((int)out.size() > k) out.resize(k);
    return out;
}
