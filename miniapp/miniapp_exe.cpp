#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_set>

#include "mfem.hpp"
#include "miniapp_lib.hpp"


double unitrand() { return (double)rand() / RAND_MAX; }
const std::unordered_set<std::string> eos_options { "ideal_gas", "constant_host" };


//! ----------------------------------------------------------------------------
struct MiniAppArgs {
    const char *device_name    = "cpu";
    const char *eos_name       = "ideal_gas";
    const char *model_path     = "";
    bool is_cpu                = true;

    int seed                   = 0;
    double empty_element_ratio = -1;

    int stop_cycle             = 1;

    int num_mats               = 5;
    int num_elems              = 10000;
    int num_qpts               = 64;
    bool pack_sparse_mats      = true;

    bool parse(int argc, char** argv) {

        mfem::OptionsParser args(argc, argv);
        args.AddOption(&device_name, "-d", "--device", "Device config string");
        args.AddOption(&model_path, "-S", "--surrogate", "Path to surrogate model");
        args.AddOption(&stop_cycle, "-c", "--stop-cycle", "Stop cycle");
        args.AddOption(&num_mats, "-m", "--num-mats", "Number of materials");
        args.AddOption(&num_elems, "-e", "--num-elems", "Number of elements");
        args.AddOption(&num_qpts, "-q", "--num-qpts", "Number of quadrature points per element");
        args.AddOption(&empty_element_ratio,
                       "-r",
                       "--empty-element-ratio",
                       "Fraction of elements that are empty "
                       "for each material. If -1 use a random value for each. ");
        args.AddOption(&seed, "-s", "--seed", "Seed for rand");
        args.AddOption(&pack_sparse_mats,
                       "-p",
                       "--pack-sparse",
                       "-np",
                       "--do-not-pack-sparse",
                       "pack sparse material data before evals (cpu only)");
        args.AddOption(&eos_name, "-z", "--eos", "EOS model type");

        args.Parse();
        if (!args.Good())
        {
           args.PrintUsage(std::cout);
           return false;
        }

        if (eos_options.find(eos_name) == eos_options.end()) {
           std::cerr << "Unsupported eos `" << eos_name << "'" << std::endl << "Available options: " << std::endl;
           for (const auto & s : eos_options)
           {
              std::cerr << " - " << s << std::endl;
           }
           return false;
        }
        else
        {
           std::cout << "Using `" << eos_name << "'" << std::endl;
        }

        args.PrintOptions(std::cout);
        std::cout << "Device:" << device_name << std::endl;
        if (strcmp(device_name, "cuda") == 0){
            is_cpu = false;
            std::cout << "IS_CPU:" << is_cpu << std::endl;
        }
        std::cout << std::endl;

        // small validation
        assert(stop_cycle > 0);
        assert(num_mats > 0);
        assert(num_elems > 0);
        assert(num_qpts > 0);
#ifdef __ENABLE_TORCH__
        if (model_path == nullptr){
            std::cerr<< "Compiled with Py-Torch enabled. It is mandatory to provide a surrogate model\n";
            exit(-1);
        }
#endif
        assert((empty_element_ratio >= 0 && empty_element_ratio < 1) || empty_element_ratio == -1);

        return true;
    }
};


//! ----------------------------------------------------------------------------
//! entry point to the mini app
//! ----------------------------------------------------------------------------
int main(int argc, char **argv) {

    // order of indicators is [Mat x Elem]
    // order of all data is   [Mat x Elem x Qpt]

    MiniAppArgs args;
    bool success = args.parse(argc, argv);
    if (!success) {
        exit(EXIT_FAILURE);
    }

    // set up a randomization seed
    srand(args.seed);

    // -------------------------------------------------------------------------
    // setup indicators
    //  to represent which combinations of materials and elements exist
    // -------------------------------------------------------------------------
    bool indicators[args.num_mats * args.num_elems];

    for (int mat_idx = 0; mat_idx < args.num_mats; ++mat_idx) {

        // min ratio if empty_element_ratio is -1
        const double min_ratio = 0.2;
        const double ratio     = args.empty_element_ratio == -1 ? unitrand() * (1 - min_ratio) + min_ratio
                                                                 : 1 - args.empty_element_ratio;
        const int num_nonzero_elems = ratio * args.num_elems;
        std::cout << "using " << num_nonzero_elems << "/"<< args.num_elems << " for material " << mat_idx << std::endl;

        int nz = 0;
        for (int elem_idx = 0; elem_idx < args.num_elems; ++elem_idx) {

            const int me = elem_idx + mat_idx*args.num_elems;
            indicators[me] = false;

            if (nz < num_nonzero_elems) {
                if (((num_nonzero_elems - nz) == (args.num_elems - elem_idx)) || unitrand() <= ratio) {
                    indicators[me] = true;
                    std::cout << " setting (mat = " << mat_idx << ", elem = " << elem_idx << ") = 1\n";
                    nz++;
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // initialize inputs
    // -------------------------------------------------------------------------
    // inputs
    std::vector<double> density (args.num_mats * args.num_elems * args.num_qpts, 0.);
    std::vector<double> energy (args.num_mats * args.num_elems * args.num_qpts, 0.);

    // init inputs
    // TODO: what are good initial values?
    for (int mat_idx = 0; mat_idx < args.num_mats; ++mat_idx) {
        for (int elem_idx = 0; elem_idx < args.num_elems; ++elem_idx) {

           const int me = mat_idx*args.num_elems + elem_idx;
            if (!indicators[me])
                continue;

            for (int qpt_idx = 0; qpt_idx < args.num_qpts; ++qpt_idx) {
                //density[qpt_idx + me*args.num_qpts] = 100*mat_idx + 10*elem_idx + qpt_idx;
                //energy[qpt_idx + me*args.num_qpts] = 100*mat_idx + 10*elem_idx + qpt_idx;
                density[qpt_idx + me*args.num_qpts] = .1 + unitrand();
                energy[qpt_idx + me*args.num_qpts] = .1 + unitrand();
            }
        }
    }

    // -------------------------------------------------------------------------
    miniapp_lib(args.is_cpu, args.device_name, args.stop_cycle, args.pack_sparse_mats,
                args.num_mats, args.num_elems, args.num_qpts, args.model_path, args.eos_name,
                density.data(), energy.data(), indicators);


    return EXIT_SUCCESS;
}

//! ----------------------------------------------------------------------------