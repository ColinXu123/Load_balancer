/**
 * @file main.cpp
 * @brief Entry point: reads settings, builds the system, runs the simulation.
 *
 * @mainpage Load Balancer Simulation
 *
 * A clock-driven simulation of a company's web traffic. Randomly generated
 * requests pass through an IP-range firewall (IPBlocker) and a Switch that
 * routes them by job type to one or more LoadBalancer instances. Each
 * balancer keeps a RequestQueue, dispatches work to a pool of WebServer
 * objects, and adds or removes servers to keep its queue between 50 and 80
 * requests per active server.
 *
 * @section usage Usage
 * @code
 * make
 * ./loadbalancer                         # prompts for servers and cycles
 * ./loadbalancer -s 20 -t 100000 -q      # quiet run, 20 servers, 100k cycles
 * ./loadbalancer -s 20 -t 100000 -v --log logs/verbose.log
 * ./loadbalancer --single                # one balancer instead of the switch
 * @endcode
 *
 * @section classes Main classes
 *  - Request: one unit of work (IPs, processing time, job type).
 *  - RequestQueue: FIFO of pending requests.
 *  - WebServer: processes one request at a time; can drain before removal.
 *  - LoadBalancer: queue + server pool + clock + scaling rule + statistics.
 *  - Switch: firewall check and routing of 'S' and 'P' jobs to balancers.
 *  - IPBlocker: blocked IPv4 ranges (CIDR or a-b form).
 *  - RequestGenerator: random arrivals, IPs, job types and processing times.
 *  - Config: key=value configuration file.
 *  - Logger: colored terminal output and a plain log file; verbose/quiet modes.
 */
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "Config.h"
#include "IPBlocker.h"
#include "Logger.h"
#include "RequestGenerator.h"
#include "Switch.h"

namespace {

/**
 * @brief Command-line options; empty strings mean "not given".
 */
struct Options {
    std::string configPath = "config.txt";  ///< Configuration file.
    std::string servers;                    ///< -s value.
    std::string cycles;                     ///< -t value.
    std::string seed;                       ///< --seed value.
    std::string logPath;                    ///< --log value.
    int  verbose  = -1;                     ///< 1 verbose, 0 quiet, -1 from config.
    int  useSwitch = -1;                    ///< 1 switch, 0 single, -1 from config.
    bool noColor  = false;                  ///< --no-color given.
    std::vector<std::string> overrides;     ///< --set key=value entries.
};

/// Prints command-line help.
void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  -c, --config FILE   configuration file (default config.txt)\n"
        << "  -s, --servers N     initial number of web servers\n"
        << "  -t, --cycles N      simulation length in clock cycles\n"
        << "  -v, --verbose       log every significant event\n"
        << "  -q, --quiet         log only start/end summaries and checkpoints\n"
        << "      --switch        route S and P jobs to separate balancers\n"
        << "      --single        use one balancer for all jobs\n"
        << "      --seed N        random seed (0 = random)\n"
        << "      --log FILE      also write output to FILE (no color codes)\n"
        << "      --no-color      disable ANSI colors\n"
        << "      --set KEY=VALUE override any configuration key (repeatable)\n"
        << "  -h, --help          show this help\n"
        << "Servers and cycles are prompted for when not given and input is a terminal.\n";
}

/// Parses argv into Options; exits on --help or a malformed argument.
Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto value = [&](const std::string& flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "-c" || a == "--config")       o.configPath = value(a);
        else if (a == "-s" || a == "--servers") o.servers = value(a);
        else if (a == "-t" || a == "--cycles")  o.cycles = value(a);
        else if (a == "--seed")                 o.seed = value(a);
        else if (a == "--log")                  o.logPath = value(a);
        else if (a == "-v" || a == "--verbose") o.verbose = 1;
        else if (a == "-q" || a == "--quiet")   o.verbose = 0;
        else if (a == "--switch")               o.useSwitch = 1;
        else if (a == "--single")               o.useSwitch = 0;
        else if (a == "--no-color")             o.noColor = true;
        else if (a == "--set")                  o.overrides.push_back(value(a));
        else if (a == "-h" || a == "--help")    { printUsage(argv[0]); std::exit(0); }
        else {
            std::cerr << "Unknown option: " << a << "\n";
            printUsage(argv[0]);
            std::exit(2);
        }
    }
    return o;
}

/**
 * @brief Gets a positive integer from the command line, a prompt, or a default.
 * @param given    Value from the command line (empty if absent).
 * @param prompt   Question to ask when running interactively.
 * @param fallback Default when nothing is given.
 * @return A positive integer.
 */
long obtainPositive(const std::string& given, const std::string& prompt, long fallback) {
    std::string text = given;
    if (text.empty() && isatty(STDIN_FILENO)) {
        std::cout << prompt << " [" << fallback << "]: " << std::flush;
        std::getline(std::cin, text);
    }
    if (text.empty()) return fallback;
    try {
        const long v = std::stol(text);
        if (v > 0) return v;
    } catch (...) {
    }
    std::cerr << "Invalid value '" << text << "', using " << fallback << "\n";
    return fallback;
}

} // namespace

/**
 * @brief Program entry point.
 * @param argc Argument count.
 * @param argv Argument values (see printUsage()).
 * @return 0 on success, non-zero on a configuration error.
 */
int main(int argc, char** argv) {
    const Options opt = parseArgs(argc, argv);

    Config config;
    const bool configLoaded = config.load(opt.configPath);
    if (!configLoaded) {
        std::cerr << "Warning: could not read '" << opt.configPath
                  << "', using built-in defaults.\n";
    }
    for (const auto& kv : opt.overrides) {
        const auto eq = kv.find('=');
        if (eq == std::string::npos) {
            std::cerr << "Ignoring --set '" << kv << "' (expected KEY=VALUE).\n";
            continue;
        }
        config.set(kv.substr(0, eq), kv.substr(eq + 1));
    }
    if (!opt.seed.empty()) config.set("seed", opt.seed);

    const long servers = obtainPositive(opt.servers, "Number of web servers",
                                        config.getInt("default_servers", 20));
    const long cycles  = obtainPositive(opt.cycles, "Simulation length in clock cycles",
                                        config.getInt("default_cycles", 10000));

    const bool verbose   = opt.verbose >= 0 ? opt.verbose == 1 : config.getBool("verbose", false);
    const bool useSwitch = opt.useSwitch >= 0 ? opt.useSwitch == 1 : config.getBool("use_switch", true);
    const bool color     = !opt.noColor && config.getBool("color", true);
    const std::string logPath = !opt.logPath.empty() ? opt.logPath : config.getString("log_file", "");

    if (!Logger::init(verbose, color, logPath)) {
        std::cerr << "Warning: could not open log file '" << logPath << "'.\n";
    }

    IPBlocker blocker;
    for (const auto& spec : config.blockedRanges()) {
        if (!blocker.addRange(spec)) {
            std::cerr << "Warning: ignoring malformed blocked_range '" << spec << "'.\n";
        }
    }

    RequestGenerator generator(config, blocker, static_cast<unsigned>(config.getInt("seed", 0)));
    Switch net(config, generator, blocker, static_cast<int>(servers), useSwitch);

    const int perServer = static_cast<int>(config.getInt("initial_requests_per_server", 100));
    net.fillInitialQueues(perServer);

    // ---- Initial configuration ----------------------------------------------
    auto heading = [](const std::string& title) {
        Logger::always("==================== " + title + " ====================", LogColor::Bold);
    };
    auto item = [](const std::string& label, const std::string& value) {
        std::ostringstream os;
        os << "  " << std::left << std::setw(26) << label << value;
        Logger::always(os.str());
    };
    auto num = [](double v) {
        std::ostringstream os;
        os << v;
        return os.str();
    };

    heading("LOAD BALANCER SIMULATION: INITIAL CONFIGURATION");
    item("Config file", configLoaded ? opt.configPath : std::string("(built-in defaults)"));
    item("Logging mode", verbose ? "verbose (every event)" : "quiet (major events + summaries)");
    item("Routing", net.switchMode() ? "switch: S -> LB-Streaming, P -> LB-Processing"
                                     : "single balancer: all jobs -> LB-Main");
    item("Random seed", std::to_string(generator.seed()));
    item("Starting web servers", std::to_string(servers));
    item("Simulation length", std::to_string(cycles) + " clock cycles");
    item("Initial queue", std::to_string(perServer) + " requests per server");
    item("Scaling rule", "add if queue > " + std::to_string(config.getInt("scale_high_threshold", 80)) +
                         " x servers, remove if < " + std::to_string(config.getInt("scale_low_threshold", 50)) +
                         " x servers");
    item("Scaling wait", std::to_string(config.getInt("scale_interval", 250)) + " cycles after each change");
    item("Server limits", std::to_string(config.getInt("min_servers", 1)) + " to " +
                          std::to_string(config.getInt("max_servers", 500)) + " per balancer");
    item("Arrival rate", num(config.getDouble("arrival_rate", 0.6)) + " requests/cycle on average");
    item("Traffic wave", "+/-" + num(config.getDouble("traffic_wave_amplitude", 0.5) * 100) +
                         "% every " + std::to_string(config.getInt("traffic_wave_period", 25000)) + " cycles");
    item("Traffic spikes", "x" + num(config.getDouble("spike_multiplier", 3.0)) + ", chance " +
                           num(config.getDouble("spike_chance", 0.00005)) + "/cycle");
    item("Traffic lulls", "x" + num(config.getDouble("lull_multiplier", 0.1)) + ", chance " +
                          num(config.getDouble("lull_chance", 0.00005)) + "/cycle");
    item("Spike/lull length", std::to_string(config.getInt("event_min_length", 1000)) + "-" +
                              std::to_string(config.getInt("event_max_length", 5000)) + " cycles");
    item("Job mix", num(config.getDouble("streaming_ratio", 0.35) * 100) + "% streaming (S), rest processing (P)");
    item("Job times", "P " + std::to_string(config.getInt("processing_time_min", 5)) + "-" +
                      std::to_string(config.getInt("processing_time_max", 45)) + ", S " +
                      std::to_string(config.getInt("streaming_time_min", 40)) + "-" +
                      std::to_string(config.getInt("streaming_time_max", 120)) + " cycles");
    {
        std::ostringstream os;
        os << blocker.rangeSpecs().size() << " range(s):";
        for (const auto& r : blocker.rangeSpecs()) os << "  " << r;
        item("Firewall", os.str());
    }
    item("Attack traffic", num(config.getDouble("attack_probability", 0.05) * 100) +
                           "% of requests from blocked ranges");
    Logger::always("Starting state:", LogColor::Bold);
    for (const auto& lb : net.balancers()) {
        Logger::always("  Start  " + lb->statusLine(), LogColor::Cyan);
    }

    // ---- Main loop ----------------------------------------------------------
    heading("SIMULATION RUNNING");
    const long statusInterval = config.getInt("status_interval", 5000);
    for (long t = 0; t < cycles; ++t) {
        net.step();
        if (statusInterval > 0 && net.clock() % statusInterval == 0 && net.clock() < cycles) {
            std::ostringstream head;
            head << "[cycle " << net.clock() << "] status  (traffic " << std::fixed
                 << std::setprecision(2) << generator.trafficLevel() << "x, "
                 << generator.trafficState() << ")";
            Logger::always(head.str(), LogColor::Cyan);
            for (const auto& lb : net.balancers()) {
                Logger::always("  " + lb->statusLine(), LogColor::Cyan);
            }
        }
    }

    // ---- End status ---------------------------------------------------------
    heading("END STATUS");
    for (const auto& lb : net.balancers()) {
        Logger::always("  End    " + lb->statusLine(), LogColor::Cyan);
    }
    item("Traffic spikes / lulls", std::to_string(generator.spikeCount()) + " / " +
                                   std::to_string(generator.lullCount()));
    heading("FINAL SUMMARY");
    for (const auto& lb : net.balancers()) {
        lb->printSummary();
    }
    // Totals across balancers (in single mode this adds the firewall and accounting check).
    net.printCombinedSummary();
    Logger::always("==================================================================", LogColor::Bold);
    Logger::close();
    return 0;
}