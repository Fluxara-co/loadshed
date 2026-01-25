#include "microservice/utils/uuid_utils.h"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace microservice {

// Static member initialization
std::random_device UUIDUtils::random_device_;
std::mt19937 UUIDUtils::random_generator_(random_device_());
std::uniform_int_distribution<int> UUIDUtils::hex_distribution_(0, 15);

std::string UUIDUtils::generateUUID() {
    try {
        std::ostringstream uuid_stream;
        
        // Generate 32 random hex characters
        for (int i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                uuid_stream << "-";
            }
            
            int random_value = hex_distribution_(random_generator_);
            uuid_stream << std::hex << random_value;
        }
        
        std::string uuid = uuid_stream.str();
        
        // Ensure version 4 format (set version bits to 0100)
        if (uuid.length() >= 14) {
            uuid[14] = '4';
        }
        
        // Ensure variant bits (set to 10xx)
        if (uuid.length() >= 19) {
            char variant_char = uuid[19];
            int variant_value = (variant_char >= '0' && variant_char <= '9') ? 
                              (variant_char - '0') : (variant_char - 'a' + 10);
            variant_value = (variant_value & 0x3) | 0x8; // Set to 10xx
            uuid[19] = (variant_value < 10) ? ('0' + variant_value) : ('a' + variant_value - 10);
        }
        
        return uuid;
    } catch (const std::exception&) {
        // Fallback to simple ID if UUID generation fails
        return generateSimpleId();
    }
}

bool UUIDUtils::isValidUUID(const std::string& uuid) {
    if (uuid.length() != 36) {
        return false;
    }
    
    // Check format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
    for (size_t i = 0; i < uuid.length(); ++i) {
        char c = uuid[i];
        
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') {
                return false;
            }
        } else {
            if (!isValidHexChar(c)) {
                return false;
            }
        }
    }
    
    // Check version (4th character after 3rd dash should be '4')
    if (uuid.length() >= 14 && uuid[14] != '4') {
        return false;
    }
    
    return true;
}

std::string UUIDUtils::generateSimpleId() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    std::ostringstream id_stream;
    id_stream << "id_" << millis << "_" << std::hex << random_device_();
    
    return id_stream.str();
}

std::string UUIDUtils::intToHex(int value, int width) {
    std::ostringstream hex_stream;
    hex_stream << std::hex << std::setfill('0') << std::setw(width) << value;
    return hex_stream.str();
}

bool UUIDUtils::isValidHexChar(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

} // namespace microservice
