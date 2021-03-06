#include "subcommand.hpp"
#include <chrono>
#include "odgi.hpp"
#include "xg.hpp"
#include "args.hxx"
#include "threads.hpp"
#include "index.hpp"
#include "chain.hpp"
#include "align.hpp"
#include "mapper.hpp"

namespace dozyg {

using namespace dozyg::subcommand;

int main_map(int argc, char** argv) {

    // trick argumentparser to do the right thing with the subcommand
    for (uint64_t i = 1; i < argc-1; ++i) {
        argv[i] = argv[i+1];
    }
    std::string prog_name = "dozyg map";
    argv[0] = (char*)prog_name.c_str();
    --argc;
    
    args::ArgumentParser parser("map sequences to a graph");
    args::HelpFlag help(parser, "help", "display this help summary", {'h', "help"});
    args::ValueFlag<std::string> idx_in_file(parser, "FILE", "load the index from this prefix", {'i', "index"});
    args::ValueFlagList<std::string> input_files(parser, "FILE", "input file, either FASTA or FASTQ, optionally gzipped, multiple allowed", {'f', "input-file"});
    args::ValueFlag<std::string> query_seq(parser, "SEQ", "query one sequence", {'s', "one-sequence"});
    args::ValueFlag<uint64_t> max_gap_length(parser, "N", "maximum gap length in chaining (default 1000)", {'g', "max-gap-length"});
    args::ValueFlag<double> max_mismatch_rate(parser, "FLOAT", "maximum allowed mismatch rate (default 0.1)", {'r', "max-mismatch-rate"});
    args::ValueFlag<double> chain_overlap(parser, "FLOAT", "maximum allowed query overlap between chains superchains (default 0.75)", {'c', "chain-overlap-max"});
    args::ValueFlag<uint64_t> chain_min_anchors(parser, "N", "minimum number of anchors in a chain (3)", {'a', "chain-min-n-anchors"});
    args::ValueFlag<uint64_t> align_best_n(parser, "N", "align the best N superchains", {'n', "align-best-n"});
    args::Flag write_chains(parser, "bool", "write chains for each alignment", {'C', "write-chains"});
    args::Flag write_superchains(parser, "bool", "write superchains for each alignment", {'S', "write-superchains"});
    args::Flag dont_align(parser, "bool", "don't align, just chain", {'D', "dont-align"});
    args::ValueFlag<uint64_t> threads(parser, "N", "number of threads to use", {'t', "threads"});
    
    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help) {
        std::cout << parser;
        return 0;
    } catch (args::ParseError e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }
    if (argc==1) {
        std::cout << parser;
        return 1;
    }

    assert(argc > 0);

    if (args::get(threads)) {
        omp_set_num_threads(args::get(threads));
    }

    std::string idx_prefix;
    if (args::get(idx_in_file).empty()) {
        std::cerr << "[dozyg map] Error: an index basename is required (-i)" << std::endl;
    } else {
        idx_prefix = args::get(idx_in_file);
    }

    dozyg_index_t index;
    index.load(idx_prefix);

    const uint64_t& kmer_length = index.kmer_length;
    uint64_t max_gap = args::get(max_gap_length)
        ? args::get(max_gap_length) : 1000;

    double mismatch_rate = args::get(max_mismatch_rate)
        ? args::get(max_mismatch_rate)
        : 0.2;

    double chain_overlap_max = args::get(chain_overlap)
        ? args::get(chain_overlap)
        : 0.75;

    uint64_t chain_min_n_anchors = args::get(chain_min_anchors)
        ? args::get(chain_min_anchors)
        : 3;

    uint64_t best_n = args::get(align_best_n) ? args::get(align_best_n) : 1;

    uint64_t n_threads = args::get(threads) ? args::get(threads) : 1;

    const std::vector<std::string> inputs(args::get(input_files));

    if (!inputs.empty()) {
        map_reads(inputs,
                  index,
                  max_gap,
                  mismatch_rate,
                  chain_min_n_anchors,
                  chain_overlap_max,
                  best_n,
                  n_threads,
                  !args::get(dont_align),
                  args::get(write_chains),
                  args::get(write_superchains));
    } else if (!args::get(query_seq).empty()) {
        const std::string& query = args::get(query_seq);
        auto anchors = anchors_for_query(index,
                                         query.c_str(),
                                         query.length());
        auto query_chains = chains(anchors,
                                   kmer_length,
                                   max_gap,
                                   mismatch_rate,
                                   chain_min_n_anchors);
        auto query_superchains = superchains(query_chains,
                                             kmer_length,
                                             mismatch_rate,
                                             chain_overlap_max);
        std::string query_name = "unknown";
        dz_s* dz = setup_dozeu();
        for (auto& superchain : query_superchains) {
            alignment_t aln = superalign(
                dz,
                query_name,
                query.length(),
                query.c_str(),
                superchain,
                index,
                index.kmer_length,
                mismatch_rate,
                max_gap);
            write_alignment_gaf(std::cout, aln, index);
        }
    }

    return 0;
}

static Subcommand dozyg_map("map", "map sequences to an index",
                            PIPELINE, 3, main_map);


}
