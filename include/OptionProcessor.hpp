#ifndef OPTIONPROCESSOR_HPP
#define OPTIONPROCESSOR_HPP

#include "OptionStructs.hpp"
#include <string>
#include <vector>

class OptionProcessor {
public:
    OptionProcessor(const std::string& symbol, const std::string& apiKey);

    double fetchSpotPrice() const;
    double fetchHistoricalSpotPrice(const std::string& asOfDate) const;

    std::vector<OptionRecord> fetchSurfaceData(
        double spot,
        double riskFreeRate,
        const std::string& asOfDate,
        int numExpiries,
        int strikesPerExpiry
    ) const;

private:
    std::string symbol;
    std::string apiKey;

    std::string makeHttpRequest(const std::string& url) const;

    std::vector<RawOptionRecord> fetchHistoricalCallChain(
        const std::string& asOfDate
    ) const;

    std::vector<RawOptionRecord> parseContracts(
        const std::string& response
    ) const;

    double fetchOptionClosePrice(
        const std::string& contractSymbol,
        const std::string& asOfDate
    ) const;

    std::vector<OptionRecord> preprocessCalls(
        const std::vector<RawOptionRecord>& rawQuotes,
        double spot,
        double riskFreeRate
    ) const;
};

#endif