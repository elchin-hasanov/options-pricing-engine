#include "OptionProcessor.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;

namespace {
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        size_t totalSize = size * nmemb;
        std::string* buffer = static_cast<std::string*>(userp);
        buffer->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    long parseDateToUnixEndOfDay(const std::string& date) {
        std::tm tm = {};
        std::istringstream ss(date);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            throw std::runtime_error("failed to parse date: " + date);
        }

        tm.tm_hour = 16;
        tm.tm_min = 0;
        tm.tm_sec = 0;
        tm.tm_isdst = -1;

        return static_cast<long>(std::mktime(&tm));
    }

    std::string urlEncode(const std::string& value) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("failed to initialize CURL for URL encoding");
        }

        char* encoded = curl_easy_escape(
            curl,
            value.c_str(),
            static_cast<int>(value.size())
        );
        if (!encoded) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("failed to URL encode string");
        }

        std::string result(encoded);
        curl_free(encoded);
        curl_easy_cleanup(curl);
        return result;
    }

    template <typename T>
    std::vector<T> evenlySampleSorted(const std::vector<T>& items, int targetCount) {
        if (targetCount <= 0 || items.empty()) {
            return {};
        }

        if (static_cast<int>(items.size()) <= targetCount) {
            return items;
        }

        std::vector<T> sampled;
        sampled.reserve(static_cast<size_t>(targetCount));

        for (int i = 0; i < targetCount; ++i) {
            int idx = static_cast<int>(
                std::round(i * (items.size() - 1.0) / (targetCount - 1.0))
            );
            sampled.push_back(items[static_cast<size_t>(idx)]);
        }

        return sampled;
    }
}

OptionProcessor::OptionProcessor(const std::string& symbol, const std::string& apiKey)
    : symbol(symbol), apiKey(apiKey) {
}

std::string OptionProcessor::makeHttpRequest(const std::string& url) const {
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string response;
    long httpCode = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error(
            std::string("HTTP request failed: ") + curl_easy_strerror(res)
        );
    }

    if (httpCode >= 400) {
        throw std::runtime_error(
            "HTTP request returned status " + std::to_string(httpCode)
            + " with body: " + response
        );
    }

    return response;
}

double OptionProcessor::fetchSpotPrice() const {
    std::string url =
        "https://query1.finance.yahoo.com/v8/finance/chart/" + symbol;

    std::string response = makeHttpRequest(url);
    json j = json::parse(response);

    if (!j.contains("chart")
        || !j["chart"].contains("result")
        || j["chart"]["result"].empty()
        || !j["chart"]["result"][0].contains("meta")
        || !j["chart"]["result"][0]["meta"].contains("regularMarketPrice")) {
        throw std::runtime_error("failed to parse spot price from Yahoo response");
    }

    return j["chart"]["result"][0]["meta"]["regularMarketPrice"].get<double>();
}

double OptionProcessor::fetchHistoricalSpotPrice(const std::string& asOfDate) const {
    std::ostringstream url;
    url << "https://api.massive.com/v1/open-close/"
        << symbol
        << "/" << asOfDate
        << "?adjusted=true"
        << "&apiKey=" << apiKey;

    std::string response = makeHttpRequest(url.str());
    json j = json::parse(response);

    if (!j.contains("close") || j["close"].is_null()) {
        throw std::runtime_error(
            "historical spot close missing for " + symbol + " on " + asOfDate
        );
    }

    return j["close"].get<double>();
}

std::vector<RawOptionRecord> OptionProcessor::parseContracts(
    const std::string& response
) const {
    std::vector<RawOptionRecord> contracts;
    json j = json::parse(response);

    if (!j.contains("results") || !j["results"].is_array()) {
        throw std::runtime_error("massive contracts response missing results array");
    }

    for (const auto& item : j["results"]) {
        RawOptionRecord record{};

        if (!item.contains("ticker")
            || !item.contains("expiration_date")
            || !item.contains("strike_price")
            || !item.contains("contract_type")) {
            continue;
        }

        if (item["ticker"].is_null()
            || item["expiration_date"].is_null()
            || item["strike_price"].is_null()
            || item["contract_type"].is_null()) {
            continue;
        }

        record.contractSymbol = item["ticker"].get<std::string>();
        record.expiration = item["expiration_date"].get<std::string>();
        record.strike = item["strike_price"].get<double>();
        record.type = item["contract_type"].get<std::string>();
        record.expirationUnix = parseDateToUnixEndOfDay(record.expiration);

        if (item.contains("open_interest") && !item["open_interest"].is_null()) {
            record.openInterest = item["open_interest"].get<double>();
        }

        if (item.contains("day") && item["day"].is_object()) {
            const auto& day = item["day"];
            if (day.contains("volume") && !day["volume"].is_null()) {
                record.volume = day["volume"].get<double>();
            }
        }

        contracts.push_back(record);
    }

    return contracts;
}

std::vector<RawOptionRecord> OptionProcessor::fetchHistoricalCallChain(
    const std::string& asOfDate
) const {
    std::ostringstream url;
    url << "https://api.massive.com/v3/reference/options/contracts"
        << "?underlying_ticker=" << symbol
        << "&contract_type=call"
        << "&as_of=" << asOfDate
        << "&limit=1000"
        << "&sort=expiration_date"
        << "&order=asc"
        << "&apiKey=" << apiKey;

    std::string response = makeHttpRequest(url.str());
    return parseContracts(response);
}

double OptionProcessor::fetchOptionClosePrice(
    const std::string& contractSymbol,
    const std::string& asOfDate
) const {
    const std::string encodedTicker = urlEncode(contractSymbol);

    std::ostringstream url;
    url << "https://api.massive.com/v1/open-close/"
        << encodedTicker
        << "/" << asOfDate
        << "?adjusted=true"
        << "&apiKey=" << apiKey;

    std::string response = makeHttpRequest(url.str());
    json j = json::parse(response);

    if (!j.contains("close") || j["close"].is_null()) {
        return 0.0;
    }

    return j["close"].get<double>();
}

std::vector<OptionRecord> OptionProcessor::preprocessCalls(
    const std::vector<RawOptionRecord>& rawQuotes,
    double spot,
    double riskFreeRate
) const {
    std::vector<OptionRecord> records;

    std::time_t now = std::time(nullptr);
    const double secondsPerYear = 365.0 * 24.0 * 60.0 * 60.0;

    for (const auto& quote : rawQuotes) {
        if (quote.type != "call") {
            continue;
        }

        if (quote.strike <= 0.0 || quote.closePrice <= 0.0) {
            continue;
        }

        const double maturity =
            static_cast<double>(quote.expirationUnix - now) / secondsPerYear;

        if (maturity <= 0.0) {
            continue;
        }

        const double marketPrice = quote.closePrice;

        const double lowerBound = std::max(
            spot - quote.strike * std::exp(-riskFreeRate * maturity),
            0.0
        );
        const double upperBound = spot;

        if (marketPrice < lowerBound || marketPrice > upperBound) {
            continue;
        }

        OptionRecord record;
        record.contractSymbol = quote.contractSymbol;
        record.type = quote.type;
        record.expiration = quote.expiration;
        record.strike = quote.strike;
        record.maturity = maturity;
        record.marketPrice = marketPrice;
        record.volume = quote.volume;
        record.openInterest = quote.openInterest;

        records.push_back(record);
    }

    return records;
}

std::vector<OptionRecord> OptionProcessor::fetchSurfaceData(
    double spot,
    double riskFreeRate,
    const std::string& asOfDate,
    int numExpiries,
    int strikesPerExpiry
) const {
    const std::vector<RawOptionRecord> allContracts = fetchHistoricalCallChain(asOfDate);

    if (allContracts.empty()) {
        throw std::runtime_error("No option contracts returned for " + symbol);
    }

    std::cout << "Contracts returned: " << allContracts.size() << std::endl;

    std::set<std::string> expirySet;
    for (const auto& c : allContracts) {
        if (c.type == "call" && c.expiration > asOfDate) {
            expirySet.insert(c.expiration);
        }
    }

    std::vector<std::string> allExpiries;
    for (const auto& expiry : expirySet) {
        allExpiries.push_back(expiry);
    }

    if (allExpiries.empty()) {
        throw std::runtime_error("No valid expiries found after " + asOfDate);
    }

    std::vector<std::string> expiries = evenlySampleSorted(allExpiries, numExpiries);

    std::cout << "Selected expiries:" << std::endl;
    for (const auto& expiry : expiries) {
        std::cout << "  " << expiry << std::endl;
    }

    const long asOfUnix = parseDateToUnixEndOfDay(asOfDate);
    const long realNow = static_cast<long>(std::time(nullptr));

    std::vector<RawOptionRecord> selectedRaw;

    for (const auto& expiry : expiries) {
        std::vector<RawOptionRecord> expiryContracts;

        for (const auto& c : allContracts) {
            if (c.type != "call") {
                continue;
            }
            if (c.expiration != expiry) {
                continue;
            }
            if (c.strike < 0.80 * spot || c.strike > 1.20 * spot) {
                continue;
            }
            expiryContracts.push_back(c);
        }

        std::sort(
            expiryContracts.begin(),
            expiryContracts.end(),
            [](const RawOptionRecord& a, const RawOptionRecord& b) {
                return a.strike < b.strike;
            }
        );

        expiryContracts = evenlySampleSorted(expiryContracts, strikesPerExpiry);

        std::cout << "Expiry " << expiry
                  << " selected contracts: " << expiryContracts.size()
                  << std::endl;

        for (auto candidate : expiryContracts) {
            try {
                candidate.closePrice =
                    fetchOptionClosePrice(candidate.contractSymbol, asOfDate);

                const long actualTimeToExpiry = candidate.expirationUnix - asOfUnix;
                candidate.expirationUnix = realNow + actualTimeToExpiry;

                if (candidate.closePrice > 0.0) {
                    selectedRaw.push_back(candidate);
                }

                std::cout << "Fetched "
                          << candidate.contractSymbol
                          << " strike=" << candidate.strike
                          << " close=" << candidate.closePrice
                          << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Skipping " << candidate.contractSymbol
                          << " due to pricing error: " << e.what()
                          << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::seconds(18));
        }
    }

    std::cout << "Contracts with historical prices: " << selectedRaw.size() << std::endl;
    return preprocessCalls(selectedRaw, spot, riskFreeRate);
}