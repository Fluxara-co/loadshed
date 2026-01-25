#pragma once

#include <string>
#include <random>

namespace microservice {

/**
 * @brief UUID utility class for generating and validating UUIDs
 */
class UUIDUtils {
public:
    /**
     * @brief Generate a new UUID v4 (random)
     * @return Generated UUID string in format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
     */
    static std::string generateUUID();
    
    /**
     * @brief Validate if a string is a valid UUID
     * @param uuid String to validate
     * @return true if valid UUID, false otherwise
     */
    static bool isValidUUID(const std::string& uuid);
    
    /**
     * @brief Generate a simple unique identifier (fallback if UUID generation fails)
     * @return Simple unique identifier string
     */
    static std::string generateSimpleId();

private:
    static std::random_device random_device_;
    static std::mt19937 random_generator_;
    static std::uniform_int_distribution<int> hex_distribution_;
    
    /**
     * @brief Convert integer to hex string
     * @param value Integer value
     * @param width Width of hex string (padded with zeros)
     * @return Hex string representation
     */
    static std::string intToHex(int value, int width = 2);
    
    /**
     * @brief Check if character is valid hex
     * @param c Character to check
     * @return true if valid hex character, false otherwise
     */
    static bool isValidHexChar(char c);
};

} // namespace microservice
