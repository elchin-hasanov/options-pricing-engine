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
    std::set<std::string> loadExistingKeys(const std::string& path) {
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
                std::string key = cols[0] + "|" + cols[2];
                seen.insert(key);
            }
        }

        return seen;
    }
}

int main() {
    try {
        const std::string apiKey = "x2c7oBy9fxMds2hG8BT0uOZXuxxc_Zv6";
        const double riskFreeRate = 0.04;

        const int numExpiries = 10;
        const int strikesPerExpiry = 10;

        const std::vector<std::string> asOfDates = {
            "2026-03-03",
            "2026-03-06",
            "2026-03-10",
            "2026-03-13",
            "2026-03-17",
            "2026-03-20",
            "2026-03-24",
            "2026-03-27"
        };

        const std::string outputPath = "data/processed/spy_iv_panel.csv";

        OptionProcessor processor("SPY", apiKey);

        std::filesystem::create_directories("data/processed");

        bool fileExists = std::filesystem::exists(outputPath);
        std::set<std::string> seenKeys = loadExistingKeys(outputPath);

        std::ofstream out(outputPath, std::ios::app);
        if (!out) {
            throw std::runtime_error("Failed to open panel CSV output file");
        }

        if (!fileExists) {
            out << "asOfDate,expiration,contractSymbol,spot,strike,maturity,marketPrice,impliedVol\n";
        }

        int totalWritten = 0;

        for (const auto& asOfDate : asOfDates) {
            try {
                std::cout << "\n=== Processing date " << asOfDate << " ===" << std::endl;

                double spot = processor.fetchHistoricalSpotPrice(asOfDate);
                std::cout << "Historical spot: " << spot << std::endl;

                std::vector<OptionRecord> calls = processor.fetchSurfaceData(
                    spot,
                    riskFreeRate,
                    asOfDate,
                    numExpiries,
                    strikesPerExpiry
                );

                std::cout << "Valid calls fetched this date: " << calls.size() << std::endl;

                int writtenThisDate = 0;

                for (const auto& call : calls) {
                    std::string key = asOfDate + "|" + call.contractSymbol;
                    if (seenKeys.count(key)) {
                        continue;
                    }

                    double iv = ImpliedVolSolver::(
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

                    seenKeys.insert(key);
                    writtenThisDate++;
                    totalWritten++;

                    std::cout << "Wrote "
                              << call.contractSymbol
                              << " | Exp: " << call.expiration
                              << " | Strike: " << call.strike
                              << " | IV: " << iv
                              << std::endl;
                }

                std::cout << "New rows written for " << asOfDate
                          << ": " << writtenThisDate << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Skipping date " << asOfDate
                          << " due to error: " << e.what() << std::endl;
            }
        }

        out.close();

        std::cout << "\nTotal new rows written: " << totalWritten << std::endl;
        std::cout << "Panel CSV path: " << outputPath << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
    }

    return 0;
}