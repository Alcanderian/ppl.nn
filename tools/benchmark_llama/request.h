#pragma once

#include <vector>
#include <queue>
#include <string>
#include <fstream>
#include <functional>

#include "ppl/common/log.h"

struct Request {
    std::vector<int64_t> token_ids;
    int64_t generation_len;
};

struct Response {
    std::vector<int64_t> token_ids;
};

class RequestQueue {
public:
    bool GenerateRandomRequests(
        const int64_t batch_size,
        const int64_t micro_batch,
        const int64_t vocab_size,
        const int64_t input_len,
        const int64_t generation_len)
    {
        while (!requests_.empty())
            requests_.pop();

        auto __generation_len = generation_len;
        if (generation_len <= 0) {
            LOG(WARNING) << "generation_len <= 0, it will forced to be 1.";
            __generation_len = 1;
        }

        // We must make actual batch_size aligned with micro_batch
        for (int64_t b = 0; (b < batch_size) || (b % micro_batch); ++b) {
            Request req;
            req.generation_len = __generation_len;
            req.token_ids.resize(input_len);
            for (int64_t i = 0; i < input_len; ++i) {
                req.token_ids[i] = GetRandomTokenId(i) % vocab_size;
            }
            requests_.push(req);
        }

        LOG(INFO) << "gen requests count from rand: " << requests_.size();
        return true;
    }

    bool ReadRequestsFromFile(
        const int64_t batch_size,
        const int64_t micro_batch,
        const int64_t vocab_size,
        const std::string &input_file_path,
        const std::string &generation_lens_file_path,
        const int64_t generation_len)
    {
        while (!requests_.empty())
            requests_.pop();

        auto loop_cond_size = [&]() {
            return (int64_t)requests_.size() < batch_size;
        };

        auto loop_cond_aligned = [&]() {
            return (requests_.size() % micro_batch) != 0;
        };

        auto scan_file = [&](const std::function<bool()>& loop_cond) {
            std::ifstream input_file(input_file_path, std::ios::in);
            if (!input_file.is_open()) {
                LOG(ERROR) << "error openning " << input_file_path;
                return false;
            }

            std::ifstream genlen_file;
            bool constant_genlen = generation_lens_file_path.empty();
            if (!constant_genlen) {
                genlen_file = std::ifstream(generation_lens_file_path, std::ios::in);
                if (!genlen_file.is_open()) {
                    LOG(ERROR) << "error openning " << generation_lens_file_path;
                    return false;
                }
            }
            std::string tokenid_str;
            std::string genlen_str;
            uint32_t current_line = 0;
            while (std::getline(input_file, tokenid_str) && loop_cond()) {
                int64_t genlen = generation_len;
                if (!constant_genlen) {
                    if (std::getline(genlen_file, genlen_str)) {
                        genlen = std::stoi(genlen_str);
                    } else {
                        LOG(WARNING) << "line(" << current_line << ") of "
                            << generation_lens_file_path << " is empty";
                    }
                }
                current_line++;

                if (tokenid_str.empty())
                    continue;

                std::stringstream tokenid_str_stream(tokenid_str);
                std::string tokenid;

                Request req;
                req.generation_len = genlen;
                if (req.generation_len <= 0) {
                    LOG(WARNING) << "generation_len <= 0 line(" << current_line
                        << "), it will forced to be 1.";
                    req.generation_len = 1;
                }

                while (std::getline(tokenid_str_stream, tokenid, ','))
                    req.token_ids.push_back(std::stoi(tokenid) % vocab_size);

                requests_.push(req);
            }

            return true;
        };

        if (!scan_file(loop_cond_size)) {
            return false;
        }
        while (loop_cond_aligned()) {
            LOG(WARNING) << "Request queue's size("
                << requests_.size()
                << ") is not aligned with micro_batch("
                << micro_batch
                << "). So we are going to repeat the input file.";
            if(!scan_file(loop_cond_aligned)) {
                return false;
            }
        }

        LOG(INFO) << "read request count from file: " << requests_.size();
        return true;
    }

    std::vector<Request> PopRequests(uint64_t micro_batch) {
        if (requests_.size() > 0 && requests_.size() < micro_batch) {
            LOG(WARNING) << "met queue tail, required: " << micro_batch
                << ", remain: " << requests_.size();
            return std::vector<Request>();
        }

        std::vector<Request> ret;
        ret.reserve(micro_batch);

        while (!requests_.empty() && ret.size() < micro_batch) {
            ret.push_back(requests_.front());
            requests_.pop();
        }

        LOG(INFO) << "pop request count: " << ret.size()
            << ", remain: " << requests_.size();
        return ret;
    }

private:
    int32_t GetRandomTokenId(const int32_t seed) {
        static const std::vector<int32_t> random_ids = {
            4854, 28445, 26882, 19570, 28904, 7224, 11204, 12608, 23093,
            5763, 17481, 3637, 4989, 8263, 18072, 7607, 10287, 6389, 30521,
            19284, 1001, 30170, 16117, 11688, 3189, 4694, 18740, 6585, 3299,
            289, 14008, 22789, 12043, 29885, 19050, 24321, 11134, 6291, 26101,
            21448, 9998, 11708, 13471, 4035, 6285, 15050, 3445, 30546, 3335,
            9024, 20135, 462, 27882, 29628, 2573, 29186, 24879, 16327,
            13250, 2196, 4584, 14253, 24544, 14142, 21916, 26777, 22673, 23681, 29726,
            4875, 15073, 25115, 29674, 19967, 14119, 18069, 23952, 4903, 14050, 7884,
            25496, 25353, 8206, 17718, 24951, 22931, 25282, 27350, 7459, 15428, 13848,
            17086, 30838, 6330, 19846, 21990, 12750, 18192, 23364, 31189, 2049, 5170,
            18875, 1550, 24837, 20623, 5968, 21205, 12275, 11288, 31214, 17545, 25403,
            22595, 26832, 27094, 4287, 2088, 14693, 30114, 11775, 16566, 1128, 9841,
            6723, 4064, 19010, 10563, 16391, 22630, 25224, 4214, 10438, 4197, 20711,
            25095, 8637, 1249, 21827, 15920, 1269, 24989, 18823, 10217, 4197, 18277,
            3692, 3326, 16183, 12565, 11703, 20781, 26531, 9290, 11666, 18146, 20460,
            3866, 30325, 23696, 14540, 15313, 17313, 11808, 24707, 7762, 7928, 31121,
            188, 27724, 20011, 21316, 26679, 8934, 25191, 7640, 12644, 2745, 28379,
            2915, 30257, 11475, 23502, 18365, 16392, 6913, 26862, 12704, 18085, 28552,
            7072, 23477, 30879, 26014, 10777, 22887, 25528, 13986, 16807, 7838, 1914,
            29227, 13069, 9977, 15107, 22174, 2453, 4482, 25644, 20425, 23556, 22172,
            15768, 15790, 29825, 14381, 30648, 9594, 22624, 11919, 4756, 8095, 3566,
            25349, 7798, 1451, 16108, 1740, 20877, 8163, 30604, 31876, 24077, 18241,
            7281, 6266, 4243, 7069, 19769, 22766, 18629, 11727, 19192, 26391, 26689,
            25834, 19592, 7891, 21956, 14238, 27197, 12860, 31620, 25199, 30635,
            20908, 10656, 12847, 2502, 12412, 4969, 12149, 13885, 19198, 2346,
            23433, 8594, 26669, 25496, 3386, 15291, 7447, 27139, 14139, 9704,
            7289, 2297, 18465, 15065, 29629, 29297, 18111, 16321, 23181, 4635,
            5194, 5680, 20010, 22590, 2653, 3869, 24767, 1965, 24028, 30772, 23175,
            29866, 2205, 18108, 15062, 3118, 9045, 5723, 6415, 31082, 2188, 7311,
            20256, 19578, 21254, 16531, 16726, 3079, 10648, 10834, 11582, 19042, 4120,
            21394, 18674, 23845, 1607, 16299, 22337, 22147, 4969, 25872, 24250, 29371,
            23383, 13664, 9146, 23049, 17562, 3404, 1871, 27293, 1761, 16423, 13860,
            10916, 2501, 18750, 31245, 9438, 7113, 27553, 19404, 3935, 19308, 19074, 10950,
            2523, 10560, 8343, 9880, 27166, 15279, 14267, 20852, 14966, 24011, 22818, 15692,
            1707, 5708, 9276, 24446, 27951, 4064, 3860, 11723, 14799, 14288, 14789, 24125,
            30444, 29224, 9204, 17018, 13849, 21455, 17831, 8628, 1219, 6999, 22257, 7093, 21735,
            9971, 17377, 12209, 17336, 13298, 25329, 13935, 31161, 22448, 23774, 748, 20329,
            534, 30021, 14973, 6819, 20014, 22457, 29490, 21, 16223, 5492, 12695, 17176, 11757,
            21868, 9953, 11467, 19631, 8310, 22225, 21181, 2503, 31558, 3028, 16996, 22232,
            3690, 21498, 3742, 5285, 7486, 30377, 28383, 24183, 25623, 19988, 15639, 30002,
            31411, 10780, 17521, 20937, 15612, 20057, 8355, 8916, 974, 30669, 18007, 164,
            24930, 5119, 31156, 5946, 7294, 12805, 8349, 24333, 25220, 22156, 17136, 30967,
            22668, 18047, 23242, 31038, 16002, 6195, 7639, 3549, 26399, 24178, 2848, 5888,
            12496, 7480, 23608, 479, 31809, 30003, 26686, 19203, 22386, 7131, 4202, 3938, 4982,
            31438, 3689, 29917, 19597, 28127, 4193, 18764, 2921, 4958, 22711, 93, 9594, 2494,
            25492, 29359, 1596, 19777, 16806, 31869, 30211, 18345, 25026, 7879, 31933, 3583,
            24569, 13110, 26598, 28383, 18403, 31994, 26340, 16875, 7114, 7372, 21954, 27227,
            9279, 9757, 29061, 8525, 13101, 7744, 14296, 3679, 20769, 681, 12047, 3626, 14519,
            1882, 3318, 17983, 19078, 10225, 11902, 22704, 448, 17143, 4973, 4354, 8100, 16630,
            21754, 17219, 21381, 17471, 15750, 21204, 16511, 13165, 15525, 21326, 30660, 17947,
            13702, 3995, 4059, 20, 30822, 22434, 19823, 7723, 13703, 20727, 11601, 17352, 13278,
            31426, 20254, 6780, 8720, 17786, 15357, 5186, 11210, 23357, 6095, 21162, 640, 17668,
            26775, 15785, 24912, 3374, 16072, 1838, 10180, 10731, 21572, 29611, 19191, 515, 10627,
            12119, 6484, 9732, 8013, 22587, 1849, 3148, 18262, 15175, 13366, 20509, 5587, 30812,
            2584, 31511, 11407, 6734, 18259, 13605, 9521, 25685, 30029, 31019, 6722, 3166, 15975,
            12804, 17449, 29155, 26789, 23069, 19316, 26635, 29030, 21767, 24352, 12835, 5827,
            21404, 15769, 15340, 31644, 6557, 4483, 15009, 5492, 30064, 29790, 30548, 22490, 30943,
            12428, 29600, 5910, 12041, 26366, 28920, 3731, 5983, 1577, 3275, 15440, 4307, 10031,
            20999, 8512, 766, 8616, 23190, 2754, 17507, 8830, 28490, 19489, 30404, 18750, 19824,
            9129, 13398, 28868, 9680, 14908, 1086, 25230, 3432, 18402, 21096, 26573, 13830, 10086,
            30708, 29992, 2173, 22163, 1572, 7598, 26022, 20475, 29632, 13133, 21975, 13792, 29371,
            18452, 17421, 27734, 5914, 7317, 21842, 10833, 9780, 19507, 456, 15224, 20667, 45, 25414,
            17738, 527, 31635, 31812, 8268, 23148, 24295, 1167, 2536, 14759, 10377, 2069, 13663, 12073,
            16907, 29637, 5153, 4634, 25994, 397, 31527, 1150, 18942, 28864, 25195, 20448, 6497, 16291,
            25399, 6059, 20762, 10191, 9196, 5438, 30897, 9234, 21348, 15318, 10919, 8330, 1781, 4175, 22058,
            12618, 23993, 27484, 19815, 13835, 14605, 30530, 12528, 15855, 15094, 25708, 16082, 14820,
            19526, 7676, 9215, 19222, 21365, 20375, 8183, 7369, 7940, 17555, 24506, 8138, 3027, 10721,
            17146, 18460, 12332, 5174, 12780, 25184, 2895, 19014, 7408, 19011, 1396, 4581, 23738, 18612,
            18277, 2646, 27617, 17913, 14895, 11038, 22787, 23271, 4618, 29633, 28035, 25643, 6758, 29526,
            2681, 2217, 22770, 1632, 20076, 30737, 4613, 6318, 19603, 24994, 2587, 24149, 7230, 29733, 21695,
            12255, 22514, 26849, 5111, 17797, 24847, 16833, 12742, 12003, 5286, 17873, 10942, 23972, 21230,
            6546, 14866, 18500, 15393, 22536, 8133, 25296, 22484, 19982, 13087, 29776, 23359, 10425, 22028,
            11190, 16693, 2118, 23351, 27817, 21382, 1189, 25925, 19520, 27026, 2639, 15749, 18384, 29283,
            29672, 21813, 19320, 31083, 23918, 26421, 11032, 25719, 19729, 30445, 14226, 8696, 29600, 9000,
            15486, 29377, 1422, 12197, 6116, 3543, 21149, 28361, 6570, 26061, 3658, 21072, 2339, 19848, 17606,
            2944, 24911, 6300, 13493, 16401, 19117, 31785, 22760, 24634, 26375, 7856, 20481, 25122, 14345,
            16559, 6296, 27652, 13643, 15577, 21088, 1292, 6931, 31824, 15488, 25473, 19310, 20581, 21956,
            9402, 4613, 1639, 840, 26369, 28685, 30877, 17166, 659, 28898, 11557, 19939, 31031, 18452, 29644,
            19566, 12301, 472, 20018, 19573, 8257, 25520, 3814, 10656, 13039, 14661, 2207, 26849, 21633, 23418,
            16230, 13791, 6774, 27429, 9088, 3167, 15050, 7711, 20597, 24940, 26294, 16510, 4960, 1806, 25994,
            20792, 5446, 10808, 6183, 17514, 3541, 28826, 22857, 23680, 16870, 20164, 3110, 5153, 19392, 26894,
            9187, 721, 27523, 7362, 1268, 15641, 15800, 11869, 10599, 12818, 13302, 19468, 26556, 29696, 30405,
            9210, 1918, 13974, 17268, 19746, 13401, 17902, 9654, 26288, 17900, 23369, 3759, 2450, 30977, 30906,
            17485, 17301, 26017, 14638};
        return random_ids[seed % random_ids.size()];
    }

    std::queue<Request> requests_;
};
