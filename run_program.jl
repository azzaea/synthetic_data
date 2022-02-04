using ArgParse, YAML

include("utils/parameter_parsing.jl")
include("preprocessing/preprocessing.jl")
include("optimisation/abc.jl")
include("algorithms/genotype/genotype_algorithm.jl")
include("algorithms/phenotype/phenotype_algorithm.jl")
include("evaluation/evaluation.jl")

"""Executes the program, according to which pipelines and configuration options are specified in the input

Note that any combination of pipelines can be run together, except the optimisation pipeline, which exits immediately after running

Also note that there is a specific ordering to the pipeline execution
"""
function run_program(pipelines, options)
    if pipelines["preprocessing"]
        @info "Running the preprocessing pipeline"
        run_preprocessing(options)
    end

    if pipelines["optimisation"]
        @info "Optimising model parameter values"
        run_optimisation(options)
        exit(0)
    end

    if pipelines["genotype"]
        @info "Generating synthetic genotype data"
        create_synthetic_genotype(options)
    end

    if pipelines["phenotype"]
        @info "Generating synthetic phenotype data"
        create_synthetic_phenotype(options)
    end

    if pipelines["evaluation"]
        @info "Evaluating synthetic data quality"
        run_evaluation(options)
    end
end


function parse_commandline()
    s = ArgParseSettings()

    @add_arg_table s begin
        "--config"
            help = "filepath to YAML configuration file"
            arg_type = String
            required = false
        "--preprocessing"
            help = "preprocesses raw data to use as input for synthetic data generation"
            action = :store_true
        "--genotype"
            help = "generate synthetic genotype data"
            action = :store_true
        "--phenotype"
            help = "generate synthetic phenotype data (requires synthetic genotype data input)"
            action = :store_true
        "--evaluation"
            help = "evaluate synthetic data quality (requires synthetic genotype data input)"
            action = :store_true
        "--optimisation"
            help = "run procedure for optimising model parameter values"
            action = :store_true
    end

    return parse_args(s)
end


function main()
    parsed_args = parse_commandline()

    options = YAML.load_file("config.yml")
    if parsed_args["config"] != nothing
        options_override = YAML.load_file(parsed_args["config"])
        options = merge(options, options_override)
    end

    pipelines = Dict("preprocessing" => parsed_args["preprocessing"], 
                     "genotype" => parsed_args["genotype"],
                     "phenotype" => parsed_args["phenotype"],
                     "evaluation" => parsed_args["evaluation"],
                     "optimisation" => parsed_args["optimisation"])

    @info "Creating output directories"
    outdirs = [@sprintf("%s/%s", options["filepaths"]["general"]["output_dir"], x) for x in ["evaluation", "optimisation", "reference"]]
    push!(outdirs, options["filepaths"]["general"]["output_dir"])
    for outdir in outdirs
        if !isdir(outdir)
            mkdir(outdir)
        end
    end

    println("Running pipelines:")
    for (arg,val) in pipelines
        println("  $arg  =>  $val")
    end

    run_program(pipelines, options)
    
end

main()
