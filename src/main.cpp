#include "ImpliedVolSolver.hpp"
#include "OptionProcessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    std::set<std::string> loadExistingContracts(const std::string& path) {
        std::set<std::string> seen;

        std::ifstream in(path);
        if (!in) {
            return seen;
        }

        std::string line;
        bool first = true;
        while (std::getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            if (first) {
                first = false;
                continue;
            }

            std::stringstream ss(line);
            std::string cell;
            std::vector<std::string> cols;

            while (std::getline(ss, cell, ',')) {
                cols.push_back(cell);
            }

            if (cols.size() >= 3) {
                seen.insert(cols[2]);
            }
        }

        return seen;
    }
}

int main() {
    try {
        const std::string apiKey = "x2c7oBy9fxMds2hG8BT0uOZXuxxc_Zv6";
        const std::string asOfDate = "2026-03-03";
        const double riskFreeRate = 0.04;

        const int numExpiries = 10;
        const int strikesPerExpiry = 10;

        const std::string outputPath = "data/processed/spy_iv_surface.csv";

        OptionProcessor processor("SPY", apiKey);

        double spot = processor.fetchHistoricalSpotPrice(asOfDate);
        std::cout << "Historical spot price on " << asOfDate
                  << ": " << spot << std::endl;

        std::vector<OptionRecord> calls = processor.fetchSurfaceData(
            spot,
            riskFreeRate,
            asOfDate,
            numExpiries,
            strikesPerExpiry
        );

        std::cout << "Valid calls fetched this run: " << calls.size() << std::endl;

        std::filesystem::create_directories("data/processed");

        bool fileExists = std::filesystem::exists(outputPath);
        std::set<std::string> seenContracts = loadExistingContracts(outputPath);

        std::ofstream out(outputPath, std::ios::app);
        if (!out) {
            throw std::runtime_error("csv not opening");
        }

        if (!fileExists) {
            out << "asOfDate,expiration,contractSymbol,spot,strike,maturity,marketPrice,impliedVol\n";
        }

        int written = 0;
        int skippedExisting = 0;

        for (const auto& call : calls) {
            if (seenContracts.count(call.contractSymbol)) {
                skippedExisting++;
                continue;
            }

            double iv = ImpliedVolSolver::impliedVolatility(
                spot,
                call.strike,
                call.maturity,
                riskFreeRate,
                call.marketPrice,
                0.20,
                1e-6,
                100
            );

            if (!(iv > 0.0) || iv > 5.0) {
                continue;
            }

            out << asOfDate << ","
                << call.expiration << ","
                << call.contractSymbol << ","
                << spot << ","
                << call.strike << ","
                << call.maturity << ","
                << call.marketPrice << ","
                << iv << "\n";

            seenContracts.insert(call.contractSymbol);
            written++;

            std::cout << "Wrote "
                      << call.contractSymbol
                      << " | Exp: " << call.expiration
                      << " | Strike: " << call.strike
                      << " | Price: " << call.marketPrice
                      << " | IV: " << iv
                      << std::endl;
        }

        out.close();

        std::cout << "skipped already existing: " << skippedExisting << std::endl;
        std::cout << "new rows written this run: " << written << std::endl;
        std::cout << "CSV path: " << outputPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }

    return 0;
}