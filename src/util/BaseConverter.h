// Code got from stackoverflow.com
// Arbitrary precision base conversion by Daniel Gehriger <gehriger@linkcad.com>   

#include <string>

class BaseConverter
{
public:
    std::string GetSourceBaseSet() const { return sourceBaseSet_; }
    std::string GetTargetBaseSet() const { return targetBaseSet_; }
    unsigned int GetSourceBase() const { return (unsigned int)sourceBaseSet_.length(); }
    unsigned int GetTargetBase() const { return (unsigned int)targetBaseSet_.length(); }

    /// <summary>
    /// Constructor
    /// </summary>
    /// <param name="sourceBaseSet">Characters used for source base</param>
    /// <param name="targetBaseSet">Characters used for target base</param>
    BaseConverter(const std::string& sourceBaseSet, const std::string& targetBaseSet);

    /// <summary>
    /// Get a base converter for decimal to binary numbers
    /// </summary>
    static const BaseConverter& DecimalToBinaryConverter();

    /// <summary>
    /// Get a base converter for binary to decimal numbers
    /// </summary>
    static const BaseConverter& BinaryToDecimalConverter();

    /// <summary>
    /// Get a base converter for decimal to binary numbers
    /// </summary>
    static const BaseConverter& DecimalToHexConverter();

    /// <summary>
    /// Get a base converter for binary to decimal numbers
    /// </summary>
    static const BaseConverter& HexToDecimalConverter();

    /// <summary>
    /// Convert a value in the source number base to the target number base.
    /// </summary>
    /// <param name="value">Value in source number base.</param>
    /// <returns>Value in target number base.</returns>
    std::string  Convert(std::string value) const;


    /// <summary>
    /// Convert a value in the source number base to the target number base.
    /// </summary>
    /// <param name="value">Value in source number base.</param>
    /// <param name="minDigits">Minimum number of digits for returned value.</param>
    /// <returns>Value in target number base.</returns>
    std::string Convert(const std::string& value, size_t minDigits) const;

    /// <summary>
    /// Convert a decimal value to the target base.
    /// </summary>
    /// <param name="value">Decimal value.</param>
    /// <returns>Result in target base.</returns>
    std::string FromDecimal(unsigned int value) const;

    /// <summary>
    /// Convert a decimal value to the target base.
    /// </summary>
    /// <param name="value">Decimal value.</param>
    /// <param name="minDigits">Minimum number of digits for returned value.</param>
    /// <returns>Result in target base.</returns>
    std::string FromDecimal(unsigned int value, size_t minDigits) const;

    /// <summary>
    /// Convert value in source base to decimal.
    /// </summary>
    /// <param name="value">Value in source base.</param>
    /// <returns>Decimal value.</returns>
    unsigned int ToDecimal(std::string value) const;

private:
    /// <summary>
    /// Divides x by y, and returns the quotient and remainder.
    /// </summary>
    /// <param name="baseDigits">Base digits for x and quotient.</param>
    /// <param name="x">Numerator expressed in base digits; contains quotient, expressed in base digits, upon return.</param>
    /// <param name="y">Denominator</param>
    /// <returns>Remainder of x / y.</returns>
    static unsigned int divide(const std::string& baseDigits, 
                               std::string& x, 
                               unsigned int y);

    static unsigned int base2dec(const std::string& baseDigits,
                                 const std::string& value);

    static std::string dec2base(const std::string& baseDigits, unsigned int value);

private:
    static const char*  binarySet_;
    static const char*  decimalSet_;
    static const char*  hexSet_;
    std::string         sourceBaseSet_;
    std::string         targetBaseSet_;
};
