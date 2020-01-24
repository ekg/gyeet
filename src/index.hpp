#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <sdsl/bit_vectors.hpp>
#include "mmap_allocator.h"
#include "mmap_exception.h"
#include "mmappable_vector.h"
#include "ips4o.hpp"
#include "pmhf.hpp"
#include "algorithms/kmer.hpp"
#include "algorithms/hash.hpp"
#include "threads.hpp"
#include "utility.hpp"
#include "dna.hpp"
//#include "mmap.hpp"

namespace gyeet {

using namespace mmap_allocator_namespace;


// wrap some long-winded but simple functions for manipulating handles

inline uint64_t handle_rank(const handle_t& handle) {
    return number_bool_packing::unpack_number(handle);
}

inline bool handle_is_rev(const handle_t& handle) { 
    return number_bool_packing::unpack_bit(handle);
}


// seq_pos_t defines an oriented position in the sequence space of the graph
// 
// the position can be on the forward or reverse complement of the graph
// 
// to simplify use during clustering, the offset is always measured
// relative to the beginning of the sequence vector on that strand
// 
// the encoding (with orientation in the most significant bit)
// allows us to increment and decrement positions using standard operators
//
typedef std::uint64_t seq_pos_t;

// these are helper functions for creating and interrogating the seq_pos_t's
struct seq_pos {
    constexpr static uint64_t OFFSET_BITS      = 63;
    constexpr static uint64_t ORIENTATION_MASK = static_cast<uint64_t>(1) << OFFSET_BITS;
    constexpr static uint64_t OFFSET_MASK      = ORIENTATION_MASK - 1;
    static seq_pos_t encode(const uint64_t& offset, const bool& reverse_complement) {
        return offset | (reverse_complement ? ORIENTATION_MASK : 0);
    }
    static bool is_rev(const seq_pos_t& pos) { return pos & ORIENTATION_MASK; }
    static uint64_t offset(const seq_pos_t& pos) { return pos & OFFSET_MASK; }
    static std::string to_string(const seq_pos_t& pos) {
        std::stringstream ss;
        ss << offset(pos) << (is_rev(pos) ? "-" : "+");
        return ss.str();
    }
};

struct kmer_pos_t {
    uint64_t hash;
    seq_pos_t begin;
    seq_pos_t end;
};

struct kmer_start_end_t {
    seq_pos_t begin;
    seq_pos_t end;
};

struct node_ref_t {
    uint64_t seq_idx = 0; // index among sequences
    uint64_t edge_idx = 0; // index among edges
    uint64_t count_prev = 0;
};

class gyeet_index_t {
public:
    // the kmer sizes that this graph was built on
    std::vector<uint64_t> kmer_sizes;
    // total sequence length of the graph
    uint64_t seq_length = 0;
    // forward sequence of the graph, stored here for fast access during alignment
    mmappable_vector<char> seq_fwd;
    // reverse complemented sequence of the graph, for fast access during alignment
    mmappable_vector<char> seq_rev;
    // mark node starts
    sdsl::bit_vector seq_bv;
    // lets us map between our seq vector and handles (when our input graph is compacted!)
    sdsl::bit_vector::rank_1_type seq_bv_rank;
    // edge count
    uint64_t n_edges = 0;
    // what's the most-efficient graph topology we can store?
    mmappable_vector<handle_t> edges;
    // node count
    uint64_t n_nodes = 0;
    // refer to ranges in edges
    mmappable_vector<node_ref_t> node_ref;
    // number of kmers in the index
    uint64_t n_kmers = 0;
    // number of kmer positions in the index
    uint64_t n_kmer_positions = 0;
    // our kmer hash table
    boophf_t* bphf = nullptr;
    // our kmer reference table (maps from bphf to index in kmer_pos_vec)
    mmappable_vector<uint64_t> kmer_pos_ref;
    // our kmer positions
    mmappable_vector<kmer_start_end_t> kmer_pos_table;
    // if we're loaded, helps during teardown
    bool loaded = false;

    // constructor/destructor
    gyeet_index_t(void);
    ~gyeet_index_t(void);

    // build from an input graph
    void build(const HandleGraph& graph,
               const uint64_t& kmer_length,
               const uint64_t& max_furcations,
               const uint64_t& max_degree,
               const std::string& out_prefix);

    // load an existing index
    void load(const std::string& in_prefix);

    // access: iterate over values for a given sequence
    void for_values_of(const std::string& seq, const std::function<void(const kmer_start_end_t& v)>& lambda);

    // graph accessors
    size_t get_length(const handle_t& h);
    bool is_reverse(const handle_t& h);
    seq_pos_t get_seq_pos(const handle_t& h);
};

}
